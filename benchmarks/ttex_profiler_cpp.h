#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include <omp.h>
#include <omp-tools.h>
#include <execinfo.h>
#include <assert.h>
#include <pthread.h>
#include <sys/resource.h>
#include <bits/stdc++.h>
#include <signal.h>
#include <sys/time.h>

using namespace std;

#define MAX_SPLIT 100000000
#define PERIOD_IN_NANOS (100UL * 1000000UL)
#define NSEC_PER_SEC 1000000000

#define max_threads 10

#define omp_for_ref -1
#define omp_single_ref -2
#define omp_sections_ref 0

#define parallel_begin_id -1
#define thread_begin_id -2
#define parallel_end_id -3

uint64_t global_id = 0;

pthread_mutex_t lock_t;

void timer_handler (int signum)
{
    printf ("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXxTimed out!nXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
    // return;
    exit(0);
}

timespec max_timeout; // on callback work end thread can sleep or work but unknown so no time noted

typedef struct timeout_node {
  int parallel_region_id;
  int sub_region_id;
  int sections_id;
  timespec wcet;
  timespec et; // actual time taken
  timeout_node *next;
} timeout_node;

typedef struct details{ 
  int ref; // -2 means omp for and >0 means omp section and -1 means single
  int loop_split_factor; // or the maxvul value
  // int loop_split_factor2;
  // int loop_split_factor3;
  timeout_node* expected_execution;
} details;

details **parallel_region;

typedef struct thread_info {
  uint64_t id;
  timeout_node *current;
  timer_t thread_timer_id;
  struct itimerspec thread_timer;
} thread_info;

typedef struct log_data {
  timeout_node *front;
  timeout_node *end;
} log_data;

static ompt_get_thread_data_t ompt_get_thread_data;
static ompt_get_unique_id_t ompt_get_unique_id;
static ompt_enumerate_states_t ompt_enumerate_states;

// predefine timeout nodes for parallel_begin, end, thread_begin, end --- these region should execute in similar time irrespective of anything

timeout_node* parallel_begin;
timeout_node* parallel_end;
timeout_node* thread_begin; // thread end would be thread end no timeout needed

// vector<uint64_t> ast_size;
int parallel_size;
int *ast_size;

unordered_map<uint64_t,log_data*> thread_log_data;

bool logdata = true; // first execution requires logging and subsequent execution do not -- this will basically be false for measuring execution time
struct sigevent timer_event;

typedef struct timespec timespec;
timespec start_time, end_time;

void createTimer(){
  timer_event.sigev_notify = SIGEV_SIGNAL;
  timer_event.sigev_signo = SIGALRM;
  struct sigaction sa;
  memset (&sa, 0, sizeof (sa));
  sa.sa_handler = &timer_handler;
  sa.sa_flags = 0;
  sigaction (SIGALRM, &sa, NULL);
}

void resetTimer(timespec t, thread_info *temp){ //thread id
  //timespec current;
  //clock_gettime(CLOCK_MONOTONIC, &current);
  //timespec result = timespec_sub(t,current);
  temp->thread_timer.it_value.tv_sec = t.tv_sec;
  temp->thread_timer.it_value.tv_nsec = t.tv_nsec;
  timer_settime(temp->thread_timer_id,0,&temp->thread_timer, NULL);
}

extern "C" uint64_t my_next_id() // to assign unique id's to thread (openmp might assign an id of finished thread to another)
{
  static uint64_t ID=0;
  uint64_t ret = __sync_fetch_and_add(&ID,1);
  //assert(ret<MAX_THREADS && "Maximum number of allowed threads is limited by MAX_THREADS");
  return ret;
}

extern "C" timespec get_elapsed_time(timespec* start, timespec* stop)
{
  timespec elapsed_time;
  if ((stop->tv_nsec - start->tv_nsec) < 0) 
  {
    elapsed_time.tv_sec = stop->tv_sec - start->tv_sec - 1;
    elapsed_time.tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
  } 
  else 
  {
    elapsed_time.tv_sec = stop->tv_sec - start->tv_sec;
    elapsed_time.tv_nsec = stop->tv_nsec - start->tv_nsec;
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

extern "C" void
ompt_test ()
{
  printf("Recording an iteration\n");
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
  
  // printf("----------------------- thread begin ---------------------\n");

  // thread_data->value = my_next_id();
  thread_info* temp_thread_data = (thread_info*) malloc(sizeof(thread_info));
  temp_thread_data->id = my_next_id();
  temp_thread_data->current = (timeout_node*) malloc(sizeof(timeout_node));
  *(temp_thread_data->current) = *thread_begin;
  timer_create(CLOCK_MONOTONIC, &timer_event, &temp_thread_data->thread_timer_id);
  
  printf("thread data value: %ld\n",temp_thread_data->id);

  if(logdata){
    pthread_mutex_lock(&lock_t);
    printf("//////////////////log data thread id%ld %d\n", temp_thread_data->id, thread_log_data.size());
    thread_log_data[temp_thread_data->id] = (log_data*) malloc(sizeof(log_data));
    thread_log_data[temp_thread_data->id]->front = temp_thread_data->current;
    thread_log_data[temp_thread_data->id]->end = temp_thread_data->current;
    // thread_log_data[thread_data->value]->parallel_region_id = -1; //denotes master thread begin
    // thread_log_data[thread_data->value]->sub_region_id = -1;
    pthread_mutex_unlock(&lock_t);
  }

  thread_data->ptr = temp_thread_data;
  resetTimer(temp_thread_data->current->wcet,temp_thread_data);
  if(temp_thread_data->id == 0)
  sleep(5);

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
  // uint64_t tid = ompt_get_thread_data()->value;
  ompt_data_t *current_thread = ompt_get_thread_data();
  parallel_data->value = id-1;//ompt_get_parallel_info();
  thread_info* temp_current_thread = (thread_info*) current_thread->ptr;
  printf("Parallel region id is parallel region: %d\n", parallel_data->value);
  //para_id_map[parallel_data->value] = id-1; // id I send from clang starts from 1
  //printf("thread data value: %d\n", ((thread_info*) current_thread)->current->parallel_region_id);

  // clock_gettime(CLOCK_MONOTONIC, &temp_current_thread->current->et);

  temp_current_thread->current->next = (timeout_node*) malloc(sizeof(timeout_node));
  *(temp_current_thread->current->next) = *parallel_begin;
  temp_current_thread->current = temp_current_thread->current->next;

  if(logdata){
    pthread_mutex_lock(&lock_t);
     printf("//////////////////log data thread id%ld %d\n", temp_current_thread->id, thread_log_data.size());
    clock_gettime(CLOCK_MONOTONIC, &thread_log_data[temp_current_thread->id]->end->et); 
    thread_log_data[temp_current_thread->id]->end->next = (timeout_node*) malloc(sizeof(timeout_node));
    *(thread_log_data[temp_current_thread->id]->end->next) = *(temp_current_thread->current);
    thread_log_data[temp_current_thread->id]->end = thread_log_data[temp_current_thread->id]->end->next;
    pthread_mutex_unlock(&lock_t);
  }

  resetTimer(temp_current_thread->current->wcet,temp_current_thread); 

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
  thread_info* temp_current_thread = (thread_info*) current_thread->ptr;
  // printf("----------------- start of callback work------------------ sub parallel id: %d\n", sub_parallel_id);
  // ompt_data_t *current_thread = ompt_get_thread_data();
  // printf("Parallel region id is: %d \n", parallel_data->value);
  if(sub_parallel_id != 0){ // start of work

    //printf("start of work, parallel id, sub_parallel_id%d %d\n", parallel_data->value, sub_parallel_id-1);

    if(parallel_region[parallel_data->value][sub_parallel_id-1].ref > omp_sections_ref && count != -1){ // -1 count means thread enters but not takes any section will go to work end
      temp_current_thread->current->next = (timeout_node*) malloc(sizeof(timeout_node));
      *(temp_current_thread->current->next) = parallel_region[parallel_data->value][sub_parallel_id-1].expected_execution[count-1];
      temp_current_thread->current = temp_current_thread->current->next;
    }

    else {
      temp_current_thread->current->next = (timeout_node*) malloc(sizeof(timeout_node)); 
      *(temp_current_thread->current->next) = *(parallel_region[parallel_data->value][sub_parallel_id-1].expected_execution);
      temp_current_thread->current = temp_current_thread->current->next;
    }

    if(logdata){
      pthread_mutex_lock(&lock_t);
      global_id = temp_current_thread->id;
      clock_gettime(CLOCK_MONOTONIC, &thread_log_data[temp_current_thread->id]->end->et);
      printf("//////////////////log data thread id%ld %d\n", temp_current_thread->id, thread_log_data.size());
      thread_log_data[temp_current_thread->id]->end->next = (timeout_node*) malloc(sizeof(timeout_node));
      *(thread_log_data[temp_current_thread->id]->end->next) = *(temp_current_thread->current);
      thread_log_data[temp_current_thread->id]->end = thread_log_data[temp_current_thread->id]->end->next;
      pthread_mutex_unlock(&lock_t);
    }

    resetTimer(temp_current_thread->current->wcet,temp_current_thread);
  }

  else {
    // work end - set the et of the current execution
    clock_gettime(CLOCK_MONOTONIC, &temp_current_thread->current->et);
    resetTimer(max_timeout,temp_current_thread);
  }
}


extern "C"  void
on_ompt_callback_parallel_end(
  ompt_data_t *parallel_data,
  ompt_data_t *encountering_task_data,
  int flags,
  const void *codeptr_ra)
{
  ompt_data_t *current_thread = ompt_get_thread_data();
  thread_info* temp_current_thread = (thread_info*) current_thread->ptr;
  pthread_mutex_lock(&lock_t);
  printf("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
  printf("//////////////////log data thread id%ld %d\n", temp_current_thread->id, thread_log_data.size());
  clock_gettime(CLOCK_MONOTONIC, &thread_log_data[temp_current_thread->id]->end->et);
  pthread_mutex_unlock(&lock_t);
  resetTimer(max_timeout,temp_current_thread);
  // if(temp_current_thread->id == 0)
   //sleep(10);
}

extern "C"  void
on_ompt_callback_thread_end(
  ompt_data_t *thread_data)
{
  thread_info* temp_current_thread = (thread_info*) thread_data->ptr;
  // if(temp_current_thread->id == 2)
  //  sleep(5);
  printf("+++++++++++++++++++++++log data thread id%ld %ld %d\n", temp_current_thread->id,omp_get_thread_num(), thread_log_data.size());
  // thread_info* temp = thread_timeout_map[thread_data->value];
  // thread_timeout_map.erase(thread_data->value);
}

#define register_callback_t(name, type)                       \
do{                                                           \
  type f_##name = &on_##name;                                 \
  if (ompt_set_callback(name, (ompt_callback_t)f_##name) ==   \
      ompt_set_never)                                         \
    printf("0: Could not register callback '" #name "'\n");   \
}while(0)

#define register_callback(name) register_callback_t(name, name##_t)

extern "C" void initializeTimeoutData(){
  if(logdata){
    
    parallel_begin = (timeout_node*) malloc(sizeof(timeout_node));
    thread_begin = (timeout_node*) malloc(sizeof(timeout_node));
    parallel_end = (timeout_node*) malloc(sizeof(timeout_node));

    //clock_gettime(CLOCK_MONOTONIC, &start_time);
    timespec exec_time;
    exec_time.tv_sec = 0;
    exec_time.tv_nsec = 1000*1000*1000; // 1 sec wcet to everything for now

    timespec testing_time;
    testing_time.tv_sec = 0;
    testing_time.tv_nsec = 1000; // 1 sec wcet to everything for now

    max_timeout.tv_sec = 10;
    max_timeout.tv_nsec = 1000*1000*1000;

    parallel_begin->wcet = exec_time;
    thread_begin->wcet = exec_time;
    parallel_end->wcet = exec_time;

    parallel_begin->parallel_region_id = parallel_begin_id;
    thread_begin->parallel_region_id = thread_begin_id;
    parallel_end->parallel_region_id = parallel_end_id;

    for(int i = 0 ; i < parallel_size; i++){

      // can have a parallel begin array here

      for(int j = 0 ; j < ast_size[i] ; j++){
        if(parallel_region[i][j].ref > omp_sections_ref){ //sections
          parallel_region[i][j].expected_execution = (timeout_node*) malloc(parallel_region[i][j].ref*sizeof(timeout_node)); //number of sections (this*maxvul other case)
          for(int k = 0 ; k < parallel_region[i][j].ref ; k++){
            parallel_region[i][j].expected_execution[k].wcet = exec_time;
            parallel_region[i][j].expected_execution[k].sub_region_id = j;
            parallel_region[i][j].expected_execution[k].parallel_region_id = i;
            parallel_region[i][j].expected_execution[k].sections_id = k;
          }
        }
        
        else if(parallel_region[i][j].ref == omp_single_ref) { // single  
          parallel_region[i][j].expected_execution = (timeout_node*) malloc(sizeof(timeout_node));  
          parallel_region[i][j].expected_execution->wcet = exec_time;
          parallel_region[i][j].expected_execution->parallel_region_id = i;
          parallel_region[i][j].expected_execution->sub_region_id = j;
          parallel_region[i][j].expected_execution->sections_id = -1;
        }

        else if(parallel_region[i][j].ref == omp_for_ref) { // omp for
          parallel_region[i][j].expected_execution = (timeout_node*) malloc(sizeof(timeout_node)); // *loop_split factor in the other case
          parallel_region[i][j].expected_execution->wcet = exec_time;
          parallel_region[i][j].expected_execution->parallel_region_id = i;
          parallel_region[i][j].expected_execution->sub_region_id = j;
          parallel_region[i][j].expected_execution->sections_id = -1;
        }

        j++;
      }
    }
  }

  else {
    // do nothing for now
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

  //register_callback(ompt_callback_parallel_begin);
  //register_callback(ompt_callback_parallel_end);
  register_callback(ompt_callback_thread_begin);
  register_callback(ompt_callback_thread_end);
  //register_callback(ompt_callback_work);

  bool flag = false;

  if(flag){
    ompt_test();
  }

  createTimer();

  pthread_mutex_init(&lock_t,NULL);
  clock_gettime(CLOCK_MONOTONIC, &start_time);

  printf("Checking intial\n");
  return 1; //success
}

extern "C" void ompt_finalize(ompt_data_t* data)
{
  clock_gettime(CLOCK_MONOTONIC, &end_time);
  printf("Logging\n\n\n\n\n\n\n");
  printf("%d\n", thread_log_data.size());
  for (const auto & [ key, value ] : thread_log_data) {
    printf("Thread id %d\n", key);
    while(value->front != value->end){
      printf("Parallel region: %d\n", value->front->parallel_region_id);
      printf("Sub region: %d\n", value->front->sub_region_id);
      timespec temp_time = get_elapsed_time(&start_time,&value->front->et);
      printf("%d seconds and %ld nanoseconds have elapsed!", temp_time.tv_sec, temp_time.tv_nsec);
      value->front = value->front->next;
    }
    printf("\n\n\n");
  }
  printf("Checking final\n");
}

extern "C" ompt_start_tool_result_t* ompt_start_tool(
  unsigned int omp_version,
  const char *runtime_version)
{
  clock_gettime(CLOCK_MONOTONIC, &start_time);
  static ompt_start_tool_result_t ompt_start_tool_result = {&ompt_initialize,&ompt_finalize,{.ptr=NULL}};
  return &ompt_start_tool_result;
}