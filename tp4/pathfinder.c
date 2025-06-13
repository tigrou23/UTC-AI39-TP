#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <alchemy/task.h>
#include <alchemy/timer.h>
#include <alchemy/sem.h>

#define TASK_MODE T_JOINABLE
#define TASK_STKSZ 0

RT_SEM start_sem;
RT_SEM resource_sem;
RT_SEM distrib_done_sem;

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

void acquire_resource(void) {
  rt_sem_p(&resource_sem, TM_INFINITE);
}

void release_resource(void) {
  rt_sem_v(&resource_sem);
}

void busy_wait(RTIME duration_ns) {
    struct threadobj_stat stat;
    RT_TASK_INFO info;

    if (rt_task_inquire(NULL, &info) != 0) {
        rt_printf("Error: cannot retrieve task info\n");
        return;
    }
	
    RTIME start_xtime = info.stat.xtime;
    RTIME current_xtime;

    do {
        rt_task_inquire(NULL, &info);
        current_xtime = info.stat.xtime;
    } while ((current_xtime - start_xtime) < duration_ns);
}

///////////////////////////////////////////////////////////
// Spécifique à DISTRIB_DONNEES avec signal de complétion
void distrib_donnees_task(void *cookie) {
    struct task_descriptor* params = (struct task_descriptor*)cookie;

    rt_printf("started task %s, period %ims, duration %ims, use resource %i\n",
              rt_task_name(), (int)(params->period / 1000000),
              (int)(params->duration / 1000000), params->use_resource);

    rt_sem_p(&start_sem, TM_INFINITE);

    while (1) {
        rt_task_wait_period(NULL);
        float start_time = ms_time_since_start();
        rt_printf("[%.3f ms] START %s\n", start_time, rt_task_name());

        if (params->use_resource) acquire_resource();
        busy_wait(params->duration);
        if (params->use_resource) release_resource();

        float end_time = ms_time_since_start();
        rt_printf("[%.3f ms] END %s\n", end_time, rt_task_name());

        rt_sem_v(&distrib_done_sem); // signal de complétion
    }
}

// Spécifique à ORDO_BUS avec vérification du signal
void ordo_bus_task(void *cookie) {
    struct task_descriptor* params = (struct task_descriptor*)cookie;

    rt_printf("started task %s, period %ims, duration %ims, use resource %i\n",
              rt_task_name(), (int)(params->period / 1000000),
              (int)(params->duration / 1000000), params->use_resource);

    rt_sem_p(&start_sem, TM_INFINITE);

    while (1) {
        rt_task_wait_period(NULL);

        // attente du sémaphore (doit avoir été donné par DISTRIB_DONNEES)
        if (rt_sem_p(&distrib_done_sem, TM_NONBLOCK) != 0) {
            rt_printf("RESET SYSTEM: DISTRIB_DONNEES NON EXECUTÉE\n");
            exit(EXIT_FAILURE);
        }

        float start_time = ms_time_since_start();
        rt_printf("[%.3f ms] START %s\n", start_time, rt_task_name());

        if (params->use_resource) acquire_resource();
        busy_wait(params->duration);
        if (params->use_resource) release_resource();

        float end_time = ms_time_since_start();
        rt_printf("[%.3f ms] END %s\n", end_time, rt_task_name());
    }
}

void rt_task_default(void *cookie) {
    struct task_descriptor* params = (struct task_descriptor*)cookie;

    rt_printf("started task %s, period %ims, duration %ims, use resource %i\n",
              rt_task_name(), (int)(params->period / 1000000),
              (int)(params->duration / 1000000), params->use_resource);

    rt_sem_p(&start_sem, TM_INFINITE);

    while (1) {
        rt_task_wait_period(NULL);

        float start_time = ms_time_since_start();
        rt_printf("[%.3f ms] START %s\n", start_time, rt_task_name());

        if (params->use_resource) acquire_resource();
        busy_wait(params->duration);
        if (params->use_resource) release_resource();

        float end_time = ms_time_since_start();
        rt_printf("[%.3f ms] END %s\n", end_time, rt_task_name());
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
  
  RTIME first_release_point = rt_timer_read() + 15000000;

    // Création des sémaphores
    if (rt_sem_create(&start_sem, "start_semaphore", 0, S_PRIO) != 0 ||
        rt_sem_create(&resource_sem, "bus_1553", 1, S_PRIO) != 0 ||
        rt_sem_create(&distrib_done_sem, "distrib_done_sem", 0, S_PRIO) != 0) {
        printf("error creating semaphores\n");
        return EXIT_FAILURE;
    }

  // Définir les tâches ici
  struct task_descriptor ORDO_BUS = {
    .task_function = rt_task,
    .period = 125000000,
    .duration = 25000000,
    .priority = 7,
    .use_resource = false
  };
  struct task_descriptor DISTRIB_DONNEES = {
    .task_function = rt_task,
    .period = 125000000,
    .duration = 25000000,
    .priority = 6,
    .use_resource = true
  };
  struct task_descriptor PILOTAGE = {
    .task_function = rt_task,
    .period = 250000000,
    .duration = 25000000,
    .priority = 5,
    .use_resource = true
  };
  struct task_descriptor RADIO = {
    .task_function = rt_task,
    .period = 250000000,
    .duration = 25000000,
    .priority = 4,
    .use_resource = false
  };
  struct task_descriptor CAMERA = {
    .task_function = rt_task,
    .period = 250000000,
    .duration = 25000000,
    .priority = 3,
    .use_resource = false
  }; 
  struct task_descriptor MESURES = {
    .task_function = rt_task,
    .period = 5000000000,
    .duration = 50000000,
    .priority = 2,
    .use_resource = true
  };
struct task_descriptor METEO = {
    .task_function = rt_task,
    .period = 5000000000,
    .duration = 40000000,
    .priority = 1,
    .use_resource = true
  };

  create_and_start_rt_task(&ORDO_BUS, first_release_point, "ORDO_BUS");
  create_and_start_rt_task(&DISTRIB_DONNEES, first_release_point, "DISTRIB_DONNEES");
  create_and_start_rt_task(&PILOTAGE, first_release_point, "PILOTAGE");
  create_and_start_rt_task(&RADIO, first_release_point, "RADIO");
  create_and_start_rt_task(&CAMERA, first_release_point, "CAMERA");
  create_and_start_rt_task(&MESURES, first_release_point, "MESURES");
  create_and_start_rt_task(&METEO, first_release_point, "METEO");

  if (rt_task_sleep_until(first_release_point) != 0) {
    printf("error first release point has already elapsed, increase it by %lldns\n", rt_timer_read() - first_release_point);
    return EXIT_FAILURE;
  }

  rt_printf("started main program at %.3fms\n", ms_time_since_start());

  rt_sem_broadcast(&start_sem); // Libère toutes les tâches


      // Boucle infinie pour maintenir le contexte actif
    while (1) {
        rt_task_sleep(1e9); // Attente 1 seconde
    }
    
  rt_sem_delete(&start_sem);


   
  return EXIT_SUCCESS;
}
