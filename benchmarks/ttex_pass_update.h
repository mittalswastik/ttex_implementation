#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include <omp.h>
#include <omp-tools.h>
#include <execinfo.h>
#include <assert.h>
#include <fcntl.h>
#include<sys/ioctl.h>
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

void resetTimer(timespec t, int fd){ 
  //timer_settime(temp->thread_timer_id,0,&temp->thread_timer, NULL);
  int32_t id = syscall(__NR_gettid);
  printf("scheduling cpu is %d for id %d\n: ", sched_getcpu(),id);
  //printf("modification thread id: %d\n", temp_thread_data->id);
  updated_timer test;
  test.id = id;
  test.time_val = t.tv_nsec/100000;
  ioctl(fd, MOD_TIMER_NEW, &test);
  printf("modification call made\n");
}

receive_timer receiveTime(int fd){
  receive_timer rtime;
  ioctl(fd, GET_TIMER, &rtime);
  return rtime;
}

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

extern "C" void __attribute__((noinline))
ompt_test(int parallel_region_id, int sub_id, int loop_id, int split_id)
{
  if(parallel_region_id == -1 && sub_id == -1 && loop_id == -1 && split_id == -1){
    return;
  }

  printf("Testing OMPT Test\n");

  printf("OMPT TEST CALLED %d %d %d\n",parallel_region_id,sub_id,loop_id);

  ompt_data_t *current_thread = ompt_get_thread_data();
  thread_info* temp_thread_data = (thread_info*) current_thread->ptr;

  if(temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].timer_set_flag){
    timespec temp;
    //clock_gettime(CLOCK_MONOTONIC, &temp);
    receive_timer t = receiveTime(temp_thread_data->fd);
    temp.tv_sec = 0;
    temp.tv_nsec = t.time_val;
    timespec temp_2  = temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et;
    temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et = getRegionElapsedTime(temp_2,temp);
  }
  // do not need to check previous timer set or not in this case
  temp_thread_data->thread_current_timeout.push_back(loop_execution[parallel_region_id][sub_id].expected_execution);
  temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].timer_set_flag = true;
  temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].sub_region_id = temp_thread_data->counter-1;
  temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].loop_id = loop_id;
  //clock_gettime(CLOCK_MONOTONIC, &temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et);
  receive_timer t = receiveTime(temp_thread_data->fd);
  temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et.tv_sec = 0;
  temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et.tv_nsec = t.time_val;
  resetTimer(temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].wcet,temp_thread_data->fd);
  // get parallel id here
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

  printf("thread begin data is : %d\n", thread_begin.parallel_region_id);

  // thread_data->value = my_next_id();
  thread_info* temp_thread_data = (thread_info*) calloc(1, sizeof(thread_info));
  temp_thread_data->id = my_next_id();
  temp_thread_data->counter = 0;
  printf("----------------------- thread begin 1_2---------------------\n");
  temp_thread_data->thread_current_timeout = std::vector<timeout_node>();
  printf("----------------------- thread begin 2---------------------\n");
  if(temp_thread_data->thread_current_timeout.size() == 0){
    printf("----------------------- size empty ---------------------\n");
  }
  temp_thread_data->thread_current_timeout.push_back(thread_begin);
  //createTimer(temp_thread_data);
  
  int core_id;
  printf("----------------------- thread begin ---------------------\n");
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

  //thread_info* temp_thread_data = (thread_info*) malloc(sizeof(thread_info));

  // printf("----------------------- thread begin ---------------------\n");
  int fd = open("/dev/etx_device", O_RDWR);
  temp_thread_data->fd = fd;
  //temp_thread_data->id = syscall(__NR_gettid);
  if(temp_thread_data->fd < 0) {
      printf("Cannot open device file...\n");
      return;
  }

  thread_data->ptr = temp_thread_data;  // storing the thread data such that accessible to all events

  int32_t id = syscall(__NR_gettid);
  printf("Thread id and core is %d & %d:\n", id, core_id);

  // thread_data->value = my_next_id();
  ioctl(temp_thread_data->fd, START_TIMER, &id);

  printf("thread data value: %ld\n",temp_thread_data->id);

  thread_data->ptr = temp_thread_data;
  temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].timer_set_flag = true;
  //clock_gettime(CLOCK_MONOTONIC, &temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et);
  receive_timer t = receiveTime(temp_thread_data->fd);
  temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et.tv_sec = 0;
  temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et.tv_nsec = t.time_val;
  resetTimer(temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].wcet,temp_thread_data->fd);
  // if(temp_thread_data->id == 0)
  //sleep(8);

  // printf("-------------------------- thread begin ends ------------------------\n");
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
  printf("parallel id sent is: %d\n", id);
  // uint64_t tid = ompt_get_thread_data()->value;
  ompt_data_t *current_thread = ompt_get_thread_data();
  parallel_data->value = id-1;//ompt_get_parallel_info();
  thread_info* temp_thread_data = (thread_info*) current_thread->ptr;
  printf("Parallel region id is parallel region: %d\n", parallel_data->value);
  //para_id_map[parallel_data->value] = id-1; // id I send from clang starts from 1
  //printf("thread data value: %d\n", ((thread_info*) current_thread)->current->parallel_region_id);

  if(temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].timer_set_flag){
    timespec temp;
    //clock_gettime(CLOCK_MONOTONIC, &temp);
    receive_timer t = receiveTime(temp_thread_data->fd);
    temp.tv_sec = 0;
    temp.tv_nsec = t.time_val;
    timespec temp_2  = temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et;
    temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et = getRegionElapsedTime(temp_2,temp);
  }

  temp_thread_data->thread_current_timeout.push_back(parallel_begin);
  temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].timer_set_flag = true;
  //clock_gettime(CLOCK_MONOTONIC, &temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et);
  receive_timer t = receiveTime(temp_thread_data->fd);
  temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et.tv_sec = 0;
  temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et.tv_nsec = t.time_val;
  resetTimer(temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].wcet,temp_thread_data->fd); 
  printf("-------- end of parallel begin -----------\n");
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
  
  temp_thread_data->counter = sub_parallel_id;

  if(sub_parallel_id != 0){ // start of work

    //printf("start of work, parallel id, sub_parallel_id%d %d\n", parallel_data->value, sub_parallel_id-1);
    printf("Thread id is **************** %d\n", temp_thread_data->id);
    printf("Sub region is *************** %d\n", sub_parallel_id);

    if(temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].timer_set_flag){
      timespec temp;
      receive_timer t = receiveTime(temp_thread_data->fd);
      temp.tv_sec = 0;
      temp.tv_nsec = t.time_val;
      timespec temp_2 = temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et;
      temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et = getRegionElapsedTime(temp_2,temp);
      printf("timer is been set for callback work\n");
    }

    vector<timeout_node> temp_node;

    if(parallel_region[parallel_data->value][sub_parallel_id-1].ref > omp_sections_ref && count != -1){ // -1 count means thread enters but not takes any section will go to work end
      printf("count value is: %d\n", count);
      temp_thread_data->thread_current_timeout.push_back(parallel_region[parallel_data->value][sub_parallel_id-1].expected_execution[count-1]);
      temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].timer_set_flag = true;
      temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].parallel_region_id = parallel_data->value;
      temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].sub_region_id = sub_parallel_id;
      resetTimer(temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].wcet,temp_thread_data->fd);
    }

    else if (parallel_region[parallel_data->value][sub_parallel_id-1].ref == omp_for_ref){
      printf("value is----------\n");
      temp_thread_data->thread_current_timeout.push_back(parallel_region[parallel_data->value][sub_parallel_id-1].expected_execution[0]); // only one vector in other case
      temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].timer_set_flag = true;
      temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].parallel_region_id = parallel_data->value;
      temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].sub_region_id = sub_parallel_id;
      resetTimer(temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].wcet,temp_thread_data->fd);
    }

    else if (parallel_region[parallel_data->value][sub_parallel_id-1].ref == omp_single_ref){
      printf("--------------ompt single is detected---------%d %d\n", parallel_data->value, sub_parallel_id-1);
      temp_thread_data->thread_current_timeout.push_back(parallel_region[parallel_data->value][sub_parallel_id-1].expected_execution[0]); // only one vector in other case
      temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].timer_set_flag = true;
      temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].sub_region_id = sub_parallel_id;
      temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].parallel_region_id = parallel_data->value;
      resetTimer(temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].wcet,temp_thread_data->fd);
    }

    else {
      // thread not assigned any work
      temp_thread_data->thread_current_timeout.push_back(work_begin);
      temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].parallel_region_id = parallel_data->value;
      temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].timer_set_flag = true;
      temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].sub_region_id = sub_parallel_id;
      //temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].sub_region_id = sub_parallel_id-1;
      resetTimer(max_timeout, temp_thread_data->fd); // thread will directly enter callback work end or sleep
      /* have to add implcit barrier callback waitime here and in implcit barrier reset to maxtimeout*/
    }

    //sleep(5);

    printf("checking for error\n");

    // for(int l = 0 ; l < temp_node.size() ; l++){
    //     temp_thread_data->thread_current_timeout.push_back(temp_node[l]);
    // }

   // maxvuln will pick the last id -- for nested parallelism
  }

  else {
    printf("--------------callback end---------%d %d\n", parallel_data->value, sub_parallel_id);
    // work end - set the et of the current execution
    //clock_gettime(CLOCK_MONOTONIC, &temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et);

    // if(temp_thread_data->id == 4){
    //   sleep(3);
    // }
    printf("Thread id is **************** %d\n", temp_thread_data->id);

    if(temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].timer_set_flag){
      timespec temp;
      receive_timer t = receiveTime(temp_thread_data->fd);
      temp.tv_sec = 0;
      temp.tv_nsec = t.time_val;
      timespec temp_2  = temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et;temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et = getRegionElapsedTime(temp_2,temp);
      printf("timer is been set for callback work\n");
    }
    
    temp_thread_data->thread_current_timeout.push_back(work_end);
    temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].parallel_region_id = parallel_data->value;
  }

  temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].timer_set_flag = true;
  //clock_gettime(CLOCK_MONOTONIC, &temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et);
  receive_timer t = receiveTime(temp_thread_data->fd);
  temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et.tv_sec = 0;
  temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et.tv_nsec = t.time_val;
  resetTimer(temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].wcet,temp_thread_data->fd);
}

extern "C" void
on_ompt_callback_sync_region(
  ompt_sync_region_t kind,
  ompt_scope_endpoint_t endpoint,
  ompt_data_t *parallel_data,
  ompt_data_t *task_data,
  const void *codeptr_ra)
{
  ompt_data_t *current_thread = ompt_get_thread_data();
  thread_info* temp_thread_data = (thread_info*) current_thread->ptr;

  // Can use kind to identify the type of wait .... but do not need it

  if(temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].timer_set_flag){
    temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].timer_set_flag = false;
    timespec temp;
    receive_timer t = receiveTime(temp_thread_data->fd);
    temp.tv_sec = 0;
    temp.tv_nsec = t.time_val;
    timespec temp_2  = temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et;
    temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et = getRegionElapsedTime(temp_2,temp);
  }

  resetTimer(sync_region.wcet,temp_thread_data->fd);
  printf("Sync region called --- Thread id is **************** %d\n", temp_thread_data->id);

}


extern "C"  void
on_ompt_callback_parallel_end(
  ompt_data_t *parallel_data,
  ompt_data_t *encountering_task_data,
  int flags,
  const void *codeptr_ra)
{
  ompt_data_t *current_thread = ompt_get_thread_data();
  thread_info* temp_thread_data = (thread_info*) current_thread->ptr;
  
  if(temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].timer_set_flag){
    timespec temp;
    receive_timer t = receiveTime(temp_thread_data->fd);
    temp.tv_sec = 0;
    temp.tv_nsec = t.time_val;
    timespec temp_2  = temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et;
    temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et = getRegionElapsedTime(temp_2,temp);
  }

  temp_thread_data->thread_current_timeout.push_back(parallel_end);
  // clock_gettime(CLOCK_MONOTONIC, &temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et);
  //pthread_mutex_unlock(&lock_t);
  temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].timer_set_flag = true;
  //clock_gettime(CLOCK_MONOTONIC, &temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et);
  receive_timer t = receiveTime(temp_thread_data->fd);
  temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et.tv_sec = 0;
  temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et.tv_nsec = t.time_val;
  resetTimer(temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].wcet,temp_thread_data->fd);
  // if(temp_current_thread->id == 0)
   //sleep(10);
}

extern "C"  void
on_ompt_callback_thread_end(
  ompt_data_t *thread_data)
{
  thread_info* temp_thread_data = (thread_info*) thread_data->ptr;
  // if(temp_current_thread->id == 2)
  //  sleep(5);

  if(temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].timer_set_flag){
    timespec temp;
    receive_timer t = receiveTime(temp_thread_data->fd);
    temp.tv_sec = 0;
    temp.tv_nsec = t.time_val;
    timespec temp_2  = temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et;
    temp_thread_data->thread_current_timeout[temp_thread_data->thread_current_timeout.size()-1].et = getRegionElapsedTime(temp_2,temp);
  }

  if(logdata){
    printf("thread id: %d\n", temp_thread_data->id); // removing this line gives seg fault ... why? was that the main problem thread_end in async callback
    log_data[temp_thread_data->id] = temp_thread_data->thread_current_timeout;
  }

   printf("+++++++++++++++++++++++log data size %d\n", log_data[temp_thread_data->id].size());

  // thread_info* temp = thread_timeout_map[thread_data->value];
  // thread_timeout_map.erase(thread_data->value);
}

#define register_callback_t(name, type)                       \
do {                                                           \
  type f_##name = &on_##name;                                 \
  if (ompt_set_callback(name, (ompt_callback_t)f_##name) ==   \
      ompt_set_never)                                         \
    printf("0: Could not register callback '" #name "'\n");   \
} while(0)

#define register_callback(name) register_callback_t(name, name##_t)

extern "C" void initializeTimeoutData(){

  timespec exec_time;
  exec_time.tv_sec = 2;
  exec_time.tv_nsec = 0; // 1 sec wcet to everything for now

  timespec testing_time;
  testing_time.tv_sec = 0;
  testing_time.tv_nsec = 1000; // 1 sec wcet to everything for now

  max_timeout.tv_sec = 20;
  max_timeout.tv_nsec = 1000*1000*1000;

  parallel_begin.wcet = exec_time;
  thread_begin.wcet = exec_time;
  parallel_end.wcet = exec_time;
  work_begin.wcet = exec_time;
  work_end.wcet = exec_time;
  sync_region.wcet = max_timeout;

  parallel_begin.parallel_region_id = parallel_begin_id;
  thread_begin.parallel_region_id = -100;
  parallel_end.parallel_region_id = parallel_end_id;
  work_begin.parallel_region_id = -100;
  work_end.parallel_region_id = -100;
  sync_region.parallel_region_id = -100;

  parallel_begin.sub_region_id = -100;
  thread_begin.sub_region_id = thread_begin_id;
  parallel_end.sub_region_id = -100;
  work_begin.sub_region_id = work_begin_id;
  work_end.sub_region_id = work_end_id;
  sync_region.sub_region_id = -10;

  parallel_begin.sections_id = -10;
  thread_begin.sections_id = -10;
  parallel_end.sections_id = -10;
  work_begin.sections_id = -10;
  work_end.sections_id = -10;

  parallel_begin.loop_id = -10;
  thread_begin.loop_id = -10;
  parallel_end.loop_id = -10;
  work_begin.loop_id = -10;
  work_end.loop_id = -10;

  parallel_begin.timer_set_flag = false;
  thread_begin.timer_set_flag = false;
  parallel_end.timer_set_flag = false;
  work_begin.timer_set_flag = false;
  work_end.timer_set_flag = false;
  sync_region.timer_set_flag = false;

  printf("Printing details %d\n", parallel_size);

  for(int i = 0 ; i < parallel_size; i++){

    printf("Parallel region 1 %d\n", sizeof(parallel_arr_size)/sizeof(int));
  

    for (int j = 0 ; j < loop_arr_size[i] ; j++) {
      timeout_node temp;
      temp.wcet = exec_time;
      temp.sub_region_id = -10; // this will be updated by previous timeout sub_region_id , counter for now
      temp.parallel_region_id = i;
      temp.loop_id = j;
      temp.sections_id = -10;
      temp.timer_set_flag = false;
      loop_execution[i][j].expected_execution = temp;
    }

    /* 
    added callback is an extensible idea ... if openmp supports different priority for sub regions this can be extended to have 
    different callback time for different subregions like work_callback

    Also thats the reason why we stick to openmp sub region specific timed analysis than basic block

    We support OpenMP extension of have real-time task in implicit task models

    Talk about all this in paper
    */

    for(int j = 0 ; j < parallel_arr_size[i] ; j++){

      printf("sub parallel region id: %d\n", parallel_region[i][j].ref);
      timeout_node temp;
     
      if(parallel_region[i][j].ref > omp_sections_ref){ //sections

        for(int k = 0 ; k < parallel_region[i][j].ref ; k++){
          // here will be another loop here for max vul ... then temp_v will have more noes per section
          temp.wcet = exec_time;
          temp.sub_region_id = j;
          temp.parallel_region_id = i;
          temp.sections_id = k;
          temp.loop_id = -10;
          temp.timer_set_flag = false;
          parallel_region[i][j].expected_execution.push_back(temp); // that loops last line
        }
        // printf("checking parallel region impl ............... %d\n", parallel_region[i][j].expected_execution[1]->sections_id);
      }
      
      else if(parallel_region[i][j].ref == omp_single_ref) { // single
          
          temp.wcet = exec_time;
          temp.sub_region_id = j;
          temp.parallel_region_id = i;
          temp.sections_id = -10;
          temp.loop_id = -10;
          temp.timer_set_flag = false;
          parallel_region[i][j].expected_execution.push_back(temp);
      }

      else if(parallel_region[i][j].ref == omp_for_ref) { // omp for // *loop_split factor in the other case
        
          temp.wcet = exec_time;
          temp.sub_region_id = j;
          temp.parallel_region_id = i;
          temp.sections_id = -10;
          temp.loop_id = -10;
          temp.timer_set_flag = false;
          parallel_region[i][j].expected_execution.push_back(temp);
      }
    }
  }
}

extern "C" int ompt_initialize(
  ompt_function_lookup_t lookup,
  int initial_device_num,
  ompt_data_t *tool_data)
{
  initializeTimeoutData();

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
  //register_callback(ompt_callback_implicit_barrier);

  //bool flag = true;

  ompt_test(-1,-1,-1,-1);

  // pthread_mutex_init(&lock_t,NULL);
  clock_gettime(CLOCK_MONOTONIC, &start_time);

  printf("Checking initial\n");
  return 1; //success
}

extern "C" void ompt_finalize(ompt_data_t* data)
{
  clock_gettime(CLOCK_MONOTONIC, &end_time);
  printf("Logging\n\n\n\n\n\n\n");
  printf("%d\n", log_data.size());
  for (const auto & [ key, value ] : log_data) {
    printf("------------------------------------------------------------\n");
    printf("Thread id %d\n\n", key);
    for(int j = 0 ; j < value.size() ; j++){
      printf("Parallel region: %d\n", value[j].parallel_region_id);
      printf("Sub region: %d\n", value[j].sub_region_id);
      printf("sections region: %d\n", value[j].sections_id);
      printf("Loop Id: %d\n", value[j].loop_id);
      //timespec temp_time = getRegionElapsedTime(start_time,value[j].et);
      printf("%d seconds and %ld nanoseconds have elapsed!", value[j].et.tv_sec, value[j].et.tv_nsec);
      printf("\n");
    }
    printf("\n\n\n");
  }
  printf("Checking final\n");
}

extern "C" ompt_start_tool_result_t* ompt_start_tool(
  unsigned int omp_version,
  const char *runtime_version)
{
  static ompt_start_tool_result_t ompt_start_tool_result = {&ompt_initialize,&ompt_finalize,{.ptr=NULL}};
  return &ompt_start_tool_result;
}