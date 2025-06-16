#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <alchemy/task.h>
#include <alchemy/timer.h>
#include <alchemy/sem.h>
#include <alchemy/mutex.h>

#define TASK_MODE T_JOINABLE
#define TASK_STKSZ 0

// Déclaration des sémaphores
RT_SEM start_sem;
RT_SEM distrib_done_sem;

#ifdef USE_MUTEX
RT_MUTEX resource_mutex;
#else
RT_SEM resource_sem;
#endif

// Descripteur de tâche
typedef struct task_descriptor {
  RT_TASK task;
  void (*task_function)(void*);
  RTIME period;
  RTIME duration;
  int priority;
  bool use_resource;
} task_descriptor;

// Renvoie le nom de la tâche courante (utile pour les logs)
char* rt_task_name(void) {
  static RT_TASK_INFO info;
  rt_task_inquire(NULL, &info);
  return info.name;
}

// Renvoie le temps écoulé depuis le démarrage du programme en millisecondes
float ms_time_since_start(void) {
  static RTIME init_time = 0;
  if (init_time == 0) init_time = rt_timer_read();
  return (rt_timer_read() - init_time) / 1000000.0;
}

// Acquisition du bus (mutex pour la ressource critique)
void acquire_resource(void) {
#ifdef USE_MUTEX
  rt_mutex_acquire(&resource_mutex, TM_INFINITE);
#else
  rt_sem_p(&resource_sem, TM_INFINITE);
#endif
}

// Libération du bus
void release_resource(void) {
#ifdef USE_MUTEX
  rt_mutex_release(&resource_mutex);
#else
  rt_sem_v(&resource_sem);
#endif
}

// Simulation d'une exécution active (busy wait) pour une durée donnée
void busy_wait(RTIME duration_ns) {
  struct threadobj_stat stat;
  RT_TASK_INFO info;
  // On utilise xtime pour mesurer une durée d'exécution active sans dormir
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

// Tâche DISTRIB_DONNEES — exécute et signale à ORDO_BUS
void distrib_donnees_task(void *cookie) {
  struct task_descriptor* params = (struct task_descriptor*)cookie;
  // Affiche les paramètres de la tâche au démarrage pour le debug
  rt_printf("started task %s, period %ims, duration %ims, use resource %i\n",
            rt_task_name(), (int)(params->period / 1000000),
            (int)(params->duration / 1000000), params->use_resource);
  // Attend le signal de démarrage global avant de commencer l'exécution périodique
  rt_sem_p(&start_sem, TM_INFINITE);

  while (1) {
    // Attend la prochaine activation périodique
    rt_task_wait_period(NULL);

    if (params->use_resource) acquire_resource();
    float start_time = ms_time_since_start();
    rt_printf("[%.3f ms] START %s\n", start_time, rt_task_name());

    busy_wait(params->duration);

    float end_time = ms_time_since_start();
    rt_printf("[%.3f ms] END %s\n", end_time, rt_task_name());
    if (params->use_resource) release_resource();

    // Signale à ORDO_BUS que cette tâche a été exécutée correctement
    rt_sem_v(&distrib_done_sem); // Signale que la tâche s'est exécutée
  }
}

// Tâche ORDO_BUS — vérifie si DISTRIB_DONNEES a été exécutée
void ordo_bus_task(void *cookie) {
  struct task_descriptor* params = (struct task_descriptor*)cookie;
  // Affiche les paramètres de la tâche au démarrage pour le debug
  rt_printf("started task %s, period %ims, duration %ims, use resource %i\n",
            rt_task_name(), (int)(params->period / 1000000),
            (int)(params->duration / 1000000), params->use_resource);
  rt_sem_p(&start_sem, TM_INFINITE);

  while (1) {
    // Attente du prochain réveil périodique défini par la période de la tâche
    rt_task_wait_period(NULL);

    // Vérifie si DISTRIB_DONNEES s'est exécutée depuis la dernière période
    if (rt_sem_p(&distrib_done_sem, TM_NONBLOCK) == -EWOULDBLOCK) {
      rt_printf("RESET SYSTEM: DISTRIB_DONNEES NON EXECUTÉE\n");
      exit(EXIT_FAILURE); // Arrêt du programme
      // Arrêt immédiat du système car DISTRIB_DONNEES n’a pas été exécutée à temps
    }

    if (params->use_resource) acquire_resource();
    float start_time = ms_time_since_start();
    rt_printf("[%.3f ms] START %s\n", start_time, rt_task_name());

    busy_wait(params->duration);

    float end_time = ms_time_since_start();
    rt_printf("[%.3f ms] END %s\n", end_time, rt_task_name());
    if (params->use_resource) release_resource();
  }
}

// Tâche générique pour les autres tâches
void rt_task_default(void *cookie) {
  struct task_descriptor* params = (struct task_descriptor*)cookie;
  // Affiche les paramètres de la tâche au démarrage pour le debug
  rt_printf("started task %s, period %ims, duration %ims, use resource %i\n",
            rt_task_name(), (int)(params->period / 1000000),
            (int)(params->duration / 1000000), params->use_resource);
  // Synchronisation avec le démarrage global avant l'exécution
  rt_sem_p(&start_sem, TM_INFINITE);

  while (1) {
    // Attente du prochain réveil périodique défini par la période de la tâche
    rt_task_wait_period(NULL);
    if (params->use_resource) acquire_resource();

    float start_time = ms_time_since_start();
    rt_printf("[%.3f ms] START %s\n", start_time, rt_task_name());

    busy_wait(params->duration);

    float end_time = ms_time_since_start();
    rt_printf("[%.3f ms] END %s\n", end_time, rt_task_name());

    if (params->use_resource) release_resource();
  }
}

// Fonction pour créer et démarrer une tâche
int create_and_start_rt_task(struct task_descriptor* desc, RTIME first_release_point, char* name) {
  int status = rt_task_create(&desc->task, name, TASK_STKSZ, desc->priority, TASK_MODE);
  // Affichage d'une erreur si la création/la configuration/le lancement échoue
  if (status != 0) {
    printf("error creating task %s\n", name);
    return status;
  }

  status = rt_task_set_periodic(&desc->task, first_release_point, desc->period);
  // Affichage d'une erreur si la création/la configuration/le lancement échoue
  if (status != 0) {
    printf("error setting period on task %s\n", name);
    return status;
  }

  status = rt_task_start(&desc->task, desc->task_function, desc);
  // Affichage d'une erreur si la création/la configuration/le lancement échoue
  if (status != 0) {
    printf("error starting task %s\n", name);
  }
  return status;
}

// Fonction principale
int main(void) {
  RTIME first_release_point = rt_timer_read() + 15000000; // Délai avant démarrage

  // Création des sémaphores
  if (rt_sem_create(&start_sem, "start_semaphore", 0, S_PRIO) != 0 ||
      rt_mutex_create(&resource_mutex, "bus_1553") != 0 ||
      rt_sem_create(&distrib_done_sem, "distrib_done_sem", 1, S_PRIO) != 0) {
    printf("error creating semaphores\n");
    return EXIT_FAILURE;
  }

  // Déclaration des tâches
  struct task_descriptor ORDO_BUS = {.task_function = ordo_bus_task, .period = 125000000, .duration = 25000000, .priority = 7, .use_resource = false};
  struct task_descriptor DISTRIB_DONNEES = {.task_function = distrib_donnees_task, .period = 125000000, .duration = 25000000, .priority = 6, .use_resource = true};
  struct task_descriptor PILOTAGE = {.task_function = rt_task_default, .period = 250000000, .duration = 25000000, .priority = 5, .use_resource = true};
  struct task_descriptor RADIO = {.task_function = rt_task_default, .period = 250000000, .duration = 25000000, .priority = 4, .use_resource = false};
  struct task_descriptor CAMERA = {.task_function = rt_task_default, .period = 250000000, .duration = 25000000, .priority = 3, .use_resource = false};
  struct task_descriptor MESURES = {.task_function = rt_task_default, .period = 5000000000, .duration = 50000000, .priority = 2, .use_resource = true};
  struct task_descriptor METEO = {
    .task_function = rt_task_default,
    .period = 5000000000,
    .duration = 40000000, // Durée sûre : pas de dépassement
    .priority = 1,
    .use_resource = true
  };

  // Création et démarrage de toutes les tâches du système avec leurs paramètres respectifs
  create_and_start_rt_task(&ORDO_BUS, first_release_point, "ORDO_BUS");
  create_and_start_rt_task(&DISTRIB_DONNEES, first_release_point, "DISTRIB_DONNEES");
  create_and_start_rt_task(&PILOTAGE, first_release_point, "PILOTAGE");
  create_and_start_rt_task(&RADIO, first_release_point, "RADIO");
  create_and_start_rt_task(&CAMERA, first_release_point, "CAMERA");
  create_and_start_rt_task(&MESURES, first_release_point, "MESURES");
  create_and_start_rt_task(&METEO, first_release_point, "METEO");

  // Attente du moment de première libération
  if (rt_task_sleep_until(first_release_point) != 0) {
    printf("error first release point has already elapsed, increase it by %lldns\n", rt_timer_read() - first_release_point);
    return EXIT_FAILURE;
  }

  rt_printf("started main program at %.3fms\n", ms_time_since_start());

  rt_sem_broadcast(&start_sem); // Libère toutes les tâches simultanément

  // Boucle infinie pour maintenir le programme actif
  while (1) {
    rt_task_sleep(1e9); // Attente de 1 seconde
  }

  // Nettoyage (jamais atteint ici)
  rt_sem_delete(&start_sem);
  rt_mutex_delete(&resource_mutex);

  return EXIT_SUCCESS;
}