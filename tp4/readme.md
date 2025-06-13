
## Question 4 — `rt_task_name`, `RT_TASK_INFO` et `threadobj_stat`

TODOOOOOOOOO



# Compte Rendu TP4 - Analyse Temps Réel sous Xenomai (Pathfinder) (MI11 / AI39 - Printemps 2025)

> [Ce compte rendu a été converti de notre readme (en markdown) en PDF. Nous vous conseillons de visionner notre rapport sur ce lien](https://github.com/tigrou23/UTC-AI39-TP/tree/main/tp4)

**Nom :** [Hugo Pereira](https://github.com/tigrou23) & Maher Zizouni

**UV :** AI39

**TP :** Xenomai - TP4

**Encadrant :** Guillaume Sanahuja

---

## Question 1 : Analyse des structures et fonctions de base

### Structure `task_descriptor`

La structure `task_descriptor` permet de définir tous les paramètres nécessaires à la création d'une tâche temps réel :

```c
typedef struct task_descriptor {
  RT_TASK task;                   // Structure de tâche Xenomai
  void (*task_function)(void*);   // Pointeur vers la fonction exécutée
  RTIME period;                   // Périodicité de la tâche
  RTIME duration;                 // Durée d'exécution simulée
  int priority;                   // Priorité Alchemy (1 = basse, 99 = haute)
  bool use_resource;              // Booléen : la tâche accède-t-elle au bus 1553 ?
} task_descriptor;
```

### Fonction `create_and_start_rt_task`

Cette fonction encapsule le processus de création et lancement d'une tâche temps réel avec Xenomai :

```c
int create_and_start_rt_task(struct task_descriptor* desc, RTIME first_release_point, char* name);
```

**Comportement** :

1. `rt_task_create(...)` : crée la tâche avec une priorité et un nom donnés.
2. `rt_task_set_periodic(...)` : spécifie la première activation et la périodicité.
3. `rt_task_start(...)` : démarre la tâche avec sa fonction et ses paramètres.

### Fonction `rt_task(void*)`

Cette fonction est celle qui est exécutée par défaut pour chaque tâche. Elle :

* Attend un signal du sémaphore `start_sem` avant d'exécuter la boucle périodique.
* Si la tâche utilise le bus 1553 (`use_resource == true`), elle appelle `acquire_resource()` et `release_resource()`.
* Exécute un `busy_wait()` simulant la charge CPU sur une durée précise.

**Avantage** : permet de définir des comportements génériques pour les tâches, qui peuvent être spécialisés ensuite.

---

## Question 2 — Utilité de `first_release_point` et `start_sem`

### `first_release_point`

Cette variable est un **repère temporel global** permettant de synchroniser **le démarrage périodique de toutes les tâches**.

#### Dans le code :

```c
RTIME first_release_point = rt_timer_read() + 15000000;
```

#### Rôle :

* Elle indique **le moment précis (en ns)** à partir duquel toutes les tâches vont commencer leur cycle périodique.
* Assure que **toutes les tâches démarrent simultanément** (ou avec un décalage prévu).
* Permet de :

  * tester le système dans un environnement bien contrôlé,
  * produire des **chronogrammes cohérents**,
  * éviter les artefacts dus à un démarrage asynchrone.

---

### `start_sem`

`start_sem` est un **sémaphore de synchronisation**, initialisé à 0, utilisé pour **bloquer les tâches secondaires** jusqu'à ce que le `main` les libère.

#### Dans le code :

```c
RT_SEM start_sem;
rt_sem_create(&start_sem,"start_semaphore",0,S_PRIO);
```

Chaque tâche secondaire appelle :

```c
rt_sem_p(&start_sem,TM_INFINITE);
```

#### Rôle :

* Synchroniser le démarrage réel des tâches **après leur création** et la définition de leur périodicité.
* Évite qu’une tâche commence **avant que toutes les autres soient prêtes**.

#### Libération :

```c
rt_sem_broadcast(&start_sem);
```

* Le `main` peut appeler `rt_sem_broadcast()` pour débloquer toutes les tâches **en même temps**.

---

Ces deux mécanismes (`first_release_point` et `start_sem`) sont donc cruciaux pour garantir une **expérimentation contrôlée, reproductible et ordonnée** du comportement temps réel du système.

---

## Question 3 — Étapes dans `main()` pour créer et synchroniser plusieurs tâches

### Étapes à réaliser dans `main()`

1. **Définir le point de synchronisation global** :

   ```c
   RTIME first_release_point = rt_timer_read() + 15000000;
   ```

   > Donne aux tâches une base de temps commune, évite le démarrage immédiat désynchronisé.

2. **Créer un sémaphore global** :

   ```c
   rt_sem_create(&start_sem, "start_semaphore", 0, S_PRIO);
   ```

   > Permet de bloquer toutes les tâches avant le démarrage effectif, pour assurer un démarrage simultané.

3. **Initialiser les structures des tâches** (`task_descriptor`) avec :

   * `period` : période d’activation
   * `duration` : durée du `busy_wait`
   * `priority` : priorité Xenomai (plus petit = plus prioritaire)
   * `use_resource` : booléen indiquant s’il faut simuler l’usage d’une ressource critique

4. **Créer et lancer chaque tâche avec `create_and_start_rt_task()`**

   ```c
   create_and_start_rt_task(&desc1, first_release_point, "TASK1");
   create_and_start_rt_task(&desc2, first_release_point, "TASK2");
   ...
   ```

   > Chaque tâche est :
   >
   > * Créée (`rt_task_create`)
   > * Planifiée périodiquement (`rt_task_set_periodic`)
   > * Lancée (`rt_task_start`)

5. **Synchroniser avec `rt_task_sleep_until(first_release_point)`**

   ```c
   rt_task_sleep_until(first_release_point);
   ```

   > Le `main()` attend jusqu’à ce que toutes les tâches soient prêtes à démarrer.

6. **Libérer toutes les tâches simultanément** avec un `broadcast` :

   ```c
   rt_sem_broadcast(&start_sem);
   ```

   > Toutes les tâches appelant `rt_sem_p()` se débloquent et démarrent en même temps.

7. **Terminer proprement** :

   ```c
   rt_sem_delete(&start_sem);
   return EXIT_SUCCESS;
   ```

### Ajout de la tâche : ORDO\_BUS

```c
struct task_descriptor ORDO_BUS = {
  .task_function = rt_task,
  .period = 125000000,         // 125ms
  .duration = 2500000,         // 25ms
  .priority = 27,
  .use_resource = false
};

create_and_start_rt_task(&ORDO_BUS, first_release_point, "ORDO_BUS");
```
---

## Question 4 — `rt_task_name`, `RT_TASK_INFO` et `threadobj_stat`

TODOOOOOOOOO

---

## Question 5

### Simulation précise avec `xtime`

On utilise le champ `xtime` ("execution time") de la structure `threadobj_stat` pour garantir que le `busy_wait()` correspond bien à **du temps CPU consommé effectif**, même en cas de préemption.

### 🔍 Détails techniques

`RT_TASK_INFO` est une structure retournée par `rt_task_inquire()` :

```c
struct RT_TASK_INFO {
    int prio;                      // priorité de la tâche
    struct threadobj_stat stat;   // structure contenant xtime
    char name[XNOBJECT_NAME_LEN]; // nom de la tâche
    pid_t pid;                    // PID associé à la tâche
};
```

La structure `threadobj_stat`, issue de `include/alchemy/task.h`, contient notamment le champ :

```c
RTIME xtime; // temps CPU cumulé utilisé par la tâche
```

### Implémentation

```c
#include <alchemy/task.h>

void busy_wait(RTIME duration_ns) {
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
```
---

## Résultat observé (Question 6)

### Données d’exécution

```
started task ORDO_BUS, period 125ms, duration 25ms, use resource 0
started main program at 0.000ms
[0.033 ms] START ORDO_BUS
[25.048 ms] END ORDO_BUS
[124.970 ms] START ORDO_BUS
[149.994 ms] END ORDO_BUS
[249.968 ms] START ORDO_BUS
[274.986 ms] END ORDO_BUS
[374.967 ms] START ORDO_BUS
[399.985 ms] END ORDO_BUS
[499.968 ms] START ORDO_BUS
[524.985 ms] END ORDO_BUS
[624.967 ms] START ORDO_BUS
[649.987 ms] END ORDO_BUS
[749.968 ms] START ORDO_BUS
[774.986 ms] END ORDO_BUS
```

### Analyse temporelle

* Le **périodicité de 125ms** est respectée avec un excellent degré de précision (écarts < 0.05 ms).
* Le **temps d'exécution de 25ms** est atteint quasiment exactement à chaque fois.
* Le **démarrage** de la première tâche correspond bien à un `first_release_point` global et à un `rt_sem_p()`.

---

## Question 7 : Ajout de tâches avec accès concurrent à une ressource critique (bus 1553)

Pour gérer l’accès au **bus 1553** (ressource critique), on utilise un **sémaphore binaire** Xenomai :

```c
RT_SEM resource_sem;
```

### 🧩 Initialisation du sémaphore

```c
rt_sem_create(&resource_sem, "bus_1553", 1, S_PRIO);
```

* Valeur initiale : `1` → la ressource est disponible
* Type : `S_PRIO` → prioritaire, pour éviter les inversions de priorité

### Accès à la ressource

```c
void acquire_resource(void) {
  rt_sem_p(&resource_sem, TM_INFINITE);
}

void release_resource(void) {
  rt_sem_v(&resource_sem);
}
```

### Tests fonctionnels

* Plusieurs tâches peuvent maintenant demander à accéder à la **même ressource critique**.
* Les accès sont **mutuellement exclusifs** grâce au sémaphore.
* L’enchaînement des tâches dépend de la priorité et de la disponibilité de la ressource.

### Étapes ajoutées dans `main()`

* Création du `resource_sem`
* Définition et lancement des tâches supplémentaires accédant au bus avec `use_resource = true`




