#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <alchemy/task.h>
#include <alchemy/timer.h>
#include <alchemy/sem.h>

#define TASK_MODE T_JOINABLE
#define TASK_STKSZ 0

RT_SEM start_sem;

typedef struct task_descriptor{
  RT_TASK task;
  void (*task_function)(void*);
  RTIME period;
  RTIME duration;
  int priority;
  bool use_resource;
} task_descriptor;

///////////////////////////////////////////////////////////
char* rt_task_name(void) {
  static RT_TASK_INFO info;
  rt_task_inquire(NULL,&info);

  return info.name;
}

///////////////////////////////////////////////////////////
float ms_time_since_start(void) {
  static RTIME init_time=0;
	
  if(init_time==0) init_time=rt_timer_read();

  return (rt_timer_read()-init_time)/1000000.;
}

///////////////////////////////////////////////////////////
void acquire_resource(void) {

}

///////////////////////////////////////////////////////////
void release_resource(void) {

}

///////////////////////////////////////////////////////////
void busy_wait(RTIME time) { 

}

///////////////////////////////////////////////////////////
void rt_task(void *cookie) { 
    struct task_descriptor* params=(struct task_descriptor*)cookie;

    rt_printf("started task %s, period %ims, duration %ims, use resource %i\n",rt_task_name(),(int)(params->period/1000000),(int)(params->duration/1000000),params->use_resource);
    rt_sem_p(&start_sem,TM_INFINITE);

    while(1) {
        rt_task_wait_period(NULL);
        if(params->use_resource) acquire_resource();
        rt_printf("doing %s\n",rt_task_name());
        busy_wait(params->duration);
        rt_printf("doing %s ok\n",rt_task_name());
        if(params->use_resource) release_resource();
       
    }
}

///////////////////////////////////////////////////////////
int create_and_start_rt_task(struct task_descriptor* desc,RTIME first_release_point,char* name) {
    int status=rt_task_create(&desc->task,name,TASK_STKSZ,desc->priority,TASK_MODE);
    if(status!=0) {
        printf("error creating task %s\n",name);
        return status;
    }

    status=rt_task_set_periodic(&desc->task,first_release_point,desc->period);
    if(status!=0) {
        printf("error setting period on task %s\n",name);
        return status;
    }

    status=rt_task_start(&desc->task,desc->task_function,desc);
    if(status!=0) {
        printf("error starting task %s\n",name);
    }
    return status;
}

///////////////////////////////////////////////////////////
int main(void) {  
  
  RTIME first_release_point=rt_timer_read()+15000000;

  if(rt_sem_create(&start_sem,"start_semaphore",0,S_PRIO)!=0) {
    printf("error creating start_semaphore\n");
    return EXIT_FAILURE;
  }
    
  if(rt_task_sleep_until(first_release_point)!=0)	{
    printf("error first release point has already elapsed, increase it by %lldns\n",rt_timer_read()-first_release_point);
	  return EXIT_FAILURE;
	}

  rt_printf("started main program at %.3fms\n",ms_time_since_start());
    
  rt_sem_delete(&start_sem);
   
  return  EXIT_SUCCESS;
}
