#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include <omp.h>
#include <omp-tools.h>
#include <execinfo.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <sys/resource.h>
#include <bits/stdc++.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>

using namespace std;

#define MAX_SPLIT 100000000
#define PERIOD_IN_NANOS (100UL * 1000000UL)
#define NSEC_PER_SEC 1000000000

#define max_threads 10

#define omp_for_ref -1
#define omp_single_ref -2
#define omp_sections_ref 0

/*
  All these id's are just used to identify in the end using the thread logged time details as to what region was the thread executing at that time
  A non-negative parallel id would mean thread was executing a region within the thread
  but an sub_region_id of -1 in the timeout with positive parallel id suggest that it was executing thread begin region
  work_begin region even thoughwould have a positive sub region id but id in timeout node suggest this region as all these regions would have a different timeout 
  based on what parallel region they were executing in
*/

#define parallel_begin_id -2
#define thread_begin_id -1
#define parallel_end_id -3 
#define work_begin_id -4 
#define work_end_id -5  //  work end id can remain -5 as it does not provide an id in itself

uint64_t global_id = 0;

int thread_priority = 10;
unsigned long int time_val_msec = 10000;

typedef struct modified_timer{
        int32_t id;
        unsigned long int time_val;
} updated_timer;

typedef struct receive_timer {
  unsigned long int time_val;
} receive_timer;

#define START_TIMER _IOW('S',2,int32_t*) // start the timer
#define MOD_TIMER _IOW('U',3,int32_t*) // modify the timer
#define DEL_TIMER _IOW('D',3,int32_t*) // delete the timer
#define MOD_TIMER_NEW _IOW('UT',3,updated_timer*)
#define GET_TIMER _IOR('R',3,receive_timer*) // retreive the current time from the kernel

static ompt_get_thread_data_t ompt_get_thread_data;
static ompt_get_unique_id_t ompt_get_unique_id;
static ompt_enumerate_states_t ompt_enumerate_states;

typedef struct timespec timespec;
timespec start_time, end_time;

timespec max_timeout; // on callback work end thread can sleep or work but unknown so no time noted

typedef struct timeout_node {
  int parallel_region_id;
  int sub_region_id;
  int sections_id;
  int loop_id;
  timespec wcet;
  timespec et; // actual time taken
  bool timer_set_flag;
} timeout_node;

typedef struct details{ 
  int ref; // -2 means omp for and >0 means omp section and -1 means single
  vector<timeout_node> expected_execution;
} details;

details **parallel_region;

typedef struct loop_details {
  // int splits; // number of calls (max count / split factor in the pass) // no need as every split call would have the same wcet (same priority thread executing same code)
  int loop_id;
  timeout_node expected_execution; 
} loop_details;

loop_details **loop_execution;

typedef struct thread_info {
  int id;
  int kid;
  int fd;
  int counter; // stores the work id for additiional callbacks ... if this is 0 then the additional callback is in parallel region outside any directive
  vector<timeout_node> thread_current_timeout;
} thread_info;

// predefine timeout nodes for parallel_begin, end, thread_begin, end --- these region should execute in similar time irrespective of anything

timeout_node parallel_begin;
timeout_node parallel_end;
timeout_node thread_begin; // thread end would be thread end no timeout needed
timeout_node work_begin; // only for threads which do not get any task assigned (only in case of sections though)
timeout_node work_end;
timeout_node sync_region;

//vector<uint64_t> ast_size;
int parallel_size;
int *parallel_arr_size;
int *loop_arr_size;

unordered_map<int, vector<timeout_node> > log_data; // assuming max 100 threads

bool logdata = true; // first execution requires logging and subsequent execution do not -- this will basically be false for measuring execution time



extern "C" int my_core_id() // to assign unique id's to thread (openmp might assign an id of finished thread to another)
{
  static uint64_t ID=0;
  int ret = (int) __sync_fetch_and_add(&ID,1);
  //assert(ret<MAX_THREADS && "Maximum number of allowed threads is limited by MAX_THREADS");
  return ret;
}

extern "C" int my_next_id() // to assign unique id's to thread (openmp might assign an id of finished thread to another)
{
  static uint64_t ID=0;
  int ret = (int) __sync_fetch_and_add(&ID,1);
  //assert(ret<MAX_THREADS && "Maximum number of allowed threads is limited by MAX_THREADS");
  return ret;
}

timespec getRegionElapsedTime(timespec start, timespec stop)
{
  timespec elapsed_time;
  if ((stop.tv_nsec - start.tv_nsec) < 0)
  {
    elapsed_time.tv_sec = stop.tv_sec - start.tv_sec - 1;
    elapsed_time.tv_nsec = stop.tv_nsec - start.tv_nsec + 1000000000;
  }
  else
  {
    elapsed_time.tv_sec = stop.tv_sec - start.tv_sec;
    elapsed_time.tv_nsec = stop.tv_nsec - start.tv_nsec;
  }
  return elapsed_time;
}

timespec timespec_normalise(timespec ts)
{
  while(ts.tv_nsec >= NSEC_PER_SEC)
  {
    ++(ts.tv_sec);
    ts.tv_nsec -= NSEC_PER_SEC;
  }
  
  while(ts.tv_nsec <= -NSEC_PER_SEC)
  {
    --(ts.tv_sec);
    ts.tv_nsec += NSEC_PER_SEC;
  }
  
  if(ts.tv_nsec < 0 && ts.tv_sec > 0)
  {
    /* Negative nanoseconds while seconds is positive.
     * Decrement tv_sec and roll tv_nsec over.
    */
    
    --(ts.tv_sec);
    ts.tv_nsec = NSEC_PER_SEC - (-1 * ts.tv_nsec);
  }
  else if(ts.tv_nsec > 0 && ts.tv_sec < 0)
  {
    /* Positive nanoseconds while seconds is negative.
     * Increment tv_sec and roll tv_nsec over.
    */
    
    ++(ts.tv_sec);
    ts.tv_nsec = -NSEC_PER_SEC - (-1 * ts.tv_nsec);
  }
  
  return ts;
}

timespec timespec_add(timespec ts1, timespec ts2)
{
  /* Normalise inputs to prevent tv_nsec rollover if whole-second values
   * are packed in it.
  */
  ts1 = timespec_normalise(ts1);
  ts2 = timespec_normalise(ts2);
  
  ts1.tv_sec  += ts2.tv_sec;
  ts1.tv_nsec += ts2.tv_nsec;
  
  return timespec_normalise(ts1);
}

timespec timespec_sub(timespec ts1, timespec ts2)
{
  /* Normalise inputs to prevent tv_nsec rollover if whole-second values
   * are packed in it.
  */
  ts1 = timespec_normalise(ts1);
  ts2 = timespec_normalise(ts2);
  
  ts1.tv_sec  -= ts2.tv_sec;
  ts1.tv_nsec -= ts2.tv_nsec;
  
  return timespec_normalise(ts1);
}

////////////////////////////////////////////////////////////////////////

// static void countCounter (thread_data *ptr)
// {
//   __sync_add_and_fetch(&(ptr->node->counter),1);
// }

////////////////////////////////////////////////////////////////////////


extern "C"  void
on_ompt_callback_thread_begin(
  ompt_thread_t thread_type,
  ompt_data_t *thread_data)
{
  
  printf("----------------------- thread begin ---------------------\n");

  // thread_data->value = my_next_id();
  thread_info* temp_thread_data = (thread_info*) calloc(1, sizeof(thread_info));
  temp_thread_data->id = my_next_id();
  temp_thread_data->counter = 0;
  
  int core_id;
  core_id = my_core_id()%6;

  printf("core used is : %d\n",core_id);

  pthread_t thread;
  pthread_attr_t attr;
  struct sched_param param;
  int policy;

  thread = pthread_self();

  // Initialize thread attributes
  pthread_attr_init(&attr);

  int result = pthread_getschedparam(thread, &policy, &param);
  if (result != 0) {
    printf("scheduling policy not retreived\n");
  }

  else {
    printf("scheduling policy priority is : %d\n", param.sched_priority); 
  }

  // Set thread attributes to make it a real-time thread
  // pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
  // 

  // Set the desired priority for the thread
  param.sched_priority = thread_priority;
  policy = SCHED_FIFO;
  //pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
  pthread_setschedparam(thread, policy, &param); //pthread_attr_setschedparam is used before pthread_create

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);

  pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);


  thread_data->ptr = temp_thread_data;  // storing the thread data such that accessible to all events

  int32_t id = syscall(__NR_gettid);
  printf("Thread id and core is %d & %d:\n", id, core_id);
}


extern "C" void
on_ompt_callback_parallel_begin(
  ompt_data_t *encountering_task_data,
  const ompt_frame_t *encountering_task_frame,
  ompt_data_t *parallel_data,
  unsigned int requested_parallelism,
  int flags,
  const void *codeptr_ra,
  unsigned int id)
{
  printf("=================================== parallel begin ==================================\n");
  printf("parallel id sent is: %d\n", id);
  // uint64_t tid = ompt_get_thread_data()->value;
  ompt_data_t *current_thread = ompt_get_thread_data();
  parallel_data->value = id-1;//ompt_get_parallel_info();
  thread_info* temp_thread_data = (thread_info*) current_thread->ptr;
  printf("Parallel region id is parallel region: %d\n", parallel_data->value);
  printf("thead id is %d\n", temp_thread_data->id);
}

extern "C"  void
on_ompt_callback_work(
  ompt_work_t wstype,
    ompt_scope_endpoint_t endpoint,
    ompt_data_t *parallel_data,
    ompt_data_t *task_data,
    uint64_t count,
    const void *codeptr_ra,
    unsigned int sub_parallel_id)
{

  ompt_data_t *current_thread = ompt_get_thread_data();
  thread_info* temp_thread_data = (thread_info*) current_thread->ptr;

  if(endpoint == 1){
    printf("=============================== work begin ================================%d\n", endpoint);
    printf("call back work begin %d\n", temp_thread_data->id);
  }
  
  else {
    printf("=============================== work end ================================%d\n", endpoint);
    printf("call back work end %d\n", temp_thread_data->id);
  }
  
}

extern "C" void
on_ompt_callback_sync_region(
  ompt_sync_region_t kind,
  ompt_scope_endpoint_t endpoint,
  ompt_data_t *parallel_data,
  ompt_data_t *task_data,
  const void *codeptr_ra)
{
  printf("==============================sync region =============================\n");
  ompt_data_t *current_thread = ompt_get_thread_data();
  thread_info* temp_thread_data = (thread_info*) current_thread->ptr;
  printf("sync region is: %d\n", temp_thread_data->id); 
}

extern "C" void
on_ompt_callback_implicit_task(
  ompt_scope_endpoint_t endpoint,
  ompt_data_t *parallel_data,
  ompt_data_t *task_data,
  unsigned int actual_parallelism,
  unsigned int index,
  int flags
){
  ompt_data_t *current_thread = ompt_get_thread_data();
  thread_info* temp_thread_data = (thread_info*) current_thread->ptr;

  if(endpoint == 1){
    printf("=============================== task begin ================================%d\n", endpoint);
    printf("call back task begin %d\n", temp_thread_data->id);
  }
  
  else {
    printf("=============================== task end ================================%d\n", endpoint);
    printf("call back task end %d\n", temp_thread_data->id);
  }
}


extern "C"  void
on_ompt_callback_parallel_end(
  ompt_data_t *parallel_data,
  ompt_data_t *encountering_task_data,
  int flags,
  const void *codeptr_ra)
{
  printf("============================ parallel end =============================\n");
  ompt_data_t *current_thread = ompt_get_thread_data();
  thread_info* temp_thread_data = (thread_info*) current_thread->ptr;
  printf("Parallel end call back thread id is: %d\n", temp_thread_data->id); 
}

extern "C"  void
on_ompt_callback_thread_end(
  ompt_data_t *thread_data)
{
  printf("=============================== thread end ==============================\n");
  thread_info* temp_thread_data = (thread_info*) thread_data->ptr;
  printf("Thread end callback thread id: %d\n", temp_thread_data->id); 
}

#define register_callback_t(name, type)                       \
do {                                                           \
  type f_##name = &on_##name;                                 \
  if (ompt_set_callback(name, (ompt_callback_t)f_##name) ==   \
      ompt_set_never)                                         \
    printf("0: Could not register callback '" #name "'\n");   \
} while(0)

#define register_callback(name) register_callback_t(name, name##_t)


extern "C" int ompt_initialize(
  ompt_function_lookup_t lookup,
  int initial_device_num,
  ompt_data_t *tool_data)
{

  // printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@ %d\n", parallel_region[2][0].parallel_id);

  ompt_set_callback_t ompt_set_callback = (ompt_set_callback_t) lookup("ompt_set_callback");
  ompt_get_thread_data = (ompt_get_thread_data_t) lookup("ompt_get_thread_data");
  ompt_get_unique_id = (ompt_get_unique_id_t) lookup("ompt_get_unique_id");
  ompt_enumerate_states = (ompt_enumerate_states_t) lookup("ompt_enumerate_states");

  register_callback(ompt_callback_parallel_begin);
  register_callback(ompt_callback_parallel_end);
  register_callback(ompt_callback_thread_begin);
  register_callback(ompt_callback_thread_end);
  register_callback(ompt_callback_work);
  register_callback(ompt_callback_sync_region);
  register_callback(ompt_callback_implicit_task);

  printf("Checking initial\n");
  return 1; //success
}

extern "C" void ompt_finalize(ompt_data_t* data)
{
  printf("Finalizing code\n");
}

extern "C" ompt_start_tool_result_t* ompt_start_tool(
  unsigned int omp_version,
  const char *runtime_version)
{
  static ompt_start_tool_result_t ompt_start_tool_result = {&ompt_initialize,&ompt_finalize,{.ptr=NULL}};
  return &ompt_start_tool_result;
}