# Compte Rendu TP2 - Linux embarqué (MI11 / AI39 - Printemps 2025)

---
# Retours du professeur (Note : 17.7/20)

Q3.1 : il y a plusieurs types d’événements

Q3.2 : comment savez vous qu’il faut chercher le fichier input.h ? Le point de départ est de trouver ou est la struct input_event

Q3.3 : attention : « La boucle est réactive et ne fait aucune attente bloquante, donc idéal pour systèmes embarqués temps réel. » n’est pas juste. Le read sur le clavier est bien bloquant

Q4.2 : il ne s’agit pas que de la charge (à chaque fois le cpu est à 100%), mais du nombre de tâches en concurrences

Q5.2 : les caractéristiques sont données par evtest (valuers min, max, etc)

> Très bon CR, complet et détaillé
---

[Sujet du TP 2](./TP2_sujet.pdf)

**Nom :** [Hugo Pereira](https://github.com/tigrou23) & Maher Zizouni

**UV :** AI39

**TP :** Linux embarqué - TP2

**Encadrant :** Guillaume Sanahuja

---

## Introduction Générale

Ce second TP poursuit la familiarisation avec un système Linux embarqué, en s'appuyant sur une cible physique (Joy-Pi Note) équipée d'un SoC ARM. Il met en œuvre des compétences clés autour de la **chaîne de compilation croisée** et de l’**exécution de binaires natifs sur la cible ARM**.

---

## 🧰 Exercice 1 — Hello World et compilation croisée

### 🔎 1.1 — Compilation native (sur la machine hôte)

```bash
$ gcc hello.c -o hello
$ file hello
```

**Sortie typique :**

```
hello: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV), dynamically linked...
```

On observe que le fichier exécutable est :

* un binaire **ELF 64 bits**,
* compilé pour **l’architecture x86-64** (le processeur de l’ordinateur hôte),
* **lié dynamiquement** contre les bibliothèques présentes sur l’hôte.

➡️ Ce fichier **ne peut pas être exécuté sur la cible** (Joy-Pi Note), qui utilise un processeur ARM. Si on tente de l’exécuter sur la cible, le système retournera :

```
-bash: ./hello: cannot execute binary file: Exec format error
```

---

### 🛠️ 1.2 — Cross-compilation : préparation de l’environnement

Avant de pouvoir utiliser la variable `$CC` (fournie par la chaîne de cross-compilation Yocto), il faut **préparer l’environnement d’exécution** en important les bonnes variables d’environnement :

```bash
$ source /opt/poky/3.1.23/cortexa7thf-neon-vfpv4/environment-setup-cortexa7t2hf-neon-vfpv4-poky-linux-gnueabi
```

✅ Cette commande :

* initialise `$CC`, `$LD`, `$AR`, `$PKG_CONFIG_PATH`, etc.,
* pointe vers les bons outils pour générer des exécutables ARM compatibles,
* évite d’avoir à spécifier manuellement le compilateur (`arm-poky-linux-gnueabi-gcc` ici).

---

### 🧪 1.3 — Compilation croisée avec `$CC`

Toujours depuis l’hôte :

```bash
$ $CC hello.c -o hello
$ file hello
```

**Sortie attendue :**

```
hello: ELF 32-bit LSB pie executable, ARM, EABI5 version 1 (SYSV), dynamically linked, interpreter /lib/ld-linux-armhf.so.3...
```

✅ Le binaire est maintenant :

* un exécutable **ARM 32 bits**,
* destiné à un noyau **EABI5**,
* interprété par `/lib/ld-linux-armhf.so.3`,
* et compatible avec le noyau Linux de la Joy-Pi Note.

---

### 💾 1.4 — Code source `hello.c`

```c
#include <stdio.h>

int main(void) {
    printf("Hello World !\n");
    return 0;
}
```

Une fois compilé avec `$CC`, on copie l'exécutable sur la cible (par exemple dans `/home/root/tp2/`) via `scp`, clé USB ou montage du `rootfs` :

```bash
$ scp hello root@joypinote:/home/root/tp2/
```

Puis, sur la cible :

```bash
root@joypinote:~/tp2# ./hello
Hello World !
```

✅ L’exécution est réussie sur la cible ARM. On constate que :

* la compilation croisée fonctionne correctement,
* les binaires générés sont adaptés au noyau et à l’architecture matérielle,
* la chaîne Yocto a bien fourni tous les outils nécessaires à cette compilation.

---

### 📚 Récapitulatif de l’exercice 1

| Étape               | Objectif                       | Commandes                           | Résultat                         |
| ------------------- | ------------------------------ | ----------------------------------- | -------------------------------- |
| Compilation locale  | Compiler `hello.c` pour x86-64 | `gcc hello.c -o hello`              | Binaire incompatible avec ARM    |
| Setup Yocto         | Charger les outils croisés ARM | `source /opt/poky/.../env-setup...` | Variables d’environnement prêtes |
| Compilation croisée | Générer un binaire ARM         | `$CC hello.c -o hello`              | Exécutable ELF 32-bit ARM        |
| Exécution           | Tester le binaire sur la cible | `./hello`                           | `Hello World !` affiché          |

---

## 💡 Exercice 2 — Contrôle des LED via le sysfs

Dans cet exercice, on exploite l'interface `sysfs` pour contrôler deux LED physiques connectées au SoC de la cible.

---

### 🔎 2.1 | 2.2 — Contrôle manuel des LED via le shell

Les LEDs exposées par le Device Tree sont accessibles via des fichiers virtuels :

```bash
/sys/class/leds/led2/brightness
/sys/class/leds/led3/brightness
```

Chaque fichier peut être **lu** ou **écrit** pour obtenir ou modifier l’état de la LED :

```bash
# Éteindre led2
echo 0 > /sys/class/leds/led2/brightness

# Allumer led2
echo 1 > /sys/class/leds/led2/brightness

# Idem pour led3
echo 0 > /sys/class/leds/led3/brightness
echo 1 > /sys/class/leds/led3/brightness
```

✅ Cela confirme que la **LED devient un simple fichier manipulable** par `echo` ou via des appels `open()` / `write()` en C.

---

### 🧾 2.3 — Programme `led.c` : clignotement des LED

Le programme suivant alterne l'état des deux LEDs toutes les secondes :

```c
#include <fcntl.h>     // pour open()
#include <stdio.h>     // pour perror(), printf()
#include <string.h>    // pour strlen()
#include <unistd.h>    // pour write(), sleep(), close()

// Définition des chemins vers les fichiers brightness des deux LED
#define LED0 "/sys/class/leds/led2/brightness"
#define LED1 "/sys/class/leds/led3/brightness"

// Fonction utilitaire pour écrire une valeur dans un descripteur de LED
static int write_led(int fd, const char *value)
{
    // Écrit la chaîne 'value' (ex : "1" ou "0") dans le fichier représenté par fd
    return write(fd, value, strlen(value));
}

int main(void)
{
    // Ouverture du fichier brightness de la LED 2 en écriture
    int fd0 = open(LED0, O_WRONLY);
    if (fd0 < 0) {
        // Affiche une erreur si l'ouverture a échoué
        perror("open fd0");
        return 1;
    }

    // Ouverture du fichier brightness de la LED 3 en écriture
    int fd1 = open(LED1, O_WRONLY);
    if (fd1 < 0) {
        perror("open fd1");
        return 1;
    }

    int state = 0; // Variable pour alterner l'état des LED (0 ou 1)

    // Boucle infinie pour clignoter les LED alternativement
    while (1) {
        if (state) {
            // État 1 : LED2 éteinte, LED3 allumée
            write_led(fd0, "0");
            write_led(fd1, "1");
        } else {
            // État 0 : LED2 allumée, LED3 éteinte
            write_led(fd0, "1");
            write_led(fd1, "0");
        }

        // Inversion de l'état pour la prochaine itération (0 devient 1, 1 devient 0)
        state ^= 1;

        // Pause d'une seconde entre chaque alternance
        sleep(1);
    }

    // Fermeture des fichiers (jamais atteinte ici car boucle infinie)
    close(fd0);
    close(fd1);

    return 0;
}
```

---

### ⚙️ Compilation croisée et déploiement

#### 1. Préparation :

```bash
$ source /opt/poky/3.1.23/cortexa7thf-neon-vfpv4/environment-setup-cortexa7t2hf-neon-vfpv4-poky-linux-gnueabi
```

#### 2. Compilation :

```bash
$ $CC led.c -o led
```

#### 3. Déploiement sur la cible :

```bash
$ scp led root@joypinote:/home/root/tp2/
```

#### 4. Exécution sur la cible :

```bash
root@joypinote:~/tp2# ./led
```

✅ Les LED `led2` et `led3` clignotent **en alternance toutes les secondes**, prouvant que la manipulation en C via `sysfs` fonctionne parfaitement.

---

### 🧠 Conclusion sur l'exercice 2

* Le sysfs permet de manipuler simplement des GPIO exposés par le noyau Linux.
* Aucune bibliothèque externe n’est nécessaire pour interagir avec les LED.
* Cette approche est **portable** et **rapide à tester** : tout ce qui est « fichier » dans `/sys/class/leds/` est modifiable avec des outils standards (shell, C standard).
* Ce programme servira de base aux exercices suivants, où les LED seront contrôlées via des **événements matériels** (boutons, joystick...).

---

## 🔘 Exercice 3 — Boutons poussoirs : lecture d’événements et contrôle de LED

### 3.1 — Lecture des événements clavier avec `evtest`

Pour comprendre comment sont perçus les appuis sur les boutons, on utilise l’outil `evtest`, qui permet de lire les événements bruts émis par le noyau :

```bash
sudo evtest /dev/input/event0
```

Sortie typique :

```
Event: time 1716725404.337071, type 1 (EV_KEY), code 2 (KEY_1), value 1
Event: time 1716725404.573142, type 1 (EV_KEY), code 2 (KEY_1), value 0
```

| Champ     | Signification                                 |
| --------- | --------------------------------------------- |
| type = 1  | Type EV\_KEY : événement clavier/bouton       |
| code = 2  | Code du bouton (KEY\_1 par exemple)           |
| value = 1 | Bouton appuyé (1), relâché (0), ou répété (2) |

✅ Cela confirme que chaque bouton envoie un événement clair, structuré, accessible via un simple `read()` sur le fichier spécial `/dev/input/event0`.

---

### 3.2 — Localisation de la structure `input_event`

La structure utilisée pour lire ces événements est `struct input_event`, définie dans `<linux/input.h>`. On peut la localiser dans le sysroot de la cible Yocto avec :

```bash
find $OECORE_TARGET_SYSROOT/usr/include -name input.h | xargs grep -n "struct input_event"
```

Extrait :

```c
struct input_event {
    struct timeval time;
    unsigned short type;
    unsigned short code;
    unsigned int value;
};
```

Elle permet de lire les événements bruts générés par les boutons.

---

### 3.3 — Implémentation en C : `buttons_toggle.c`

Ce programme intercepte les événements d’appui sur les boutons 0 et 1 (KEY\_0 et KEY\_1) et allume/éteint alternativement deux LED physiques (`led2` et `led3`).

```c
#include <fcntl.h>           // Pour open()
#include <linux/input.h>     // Pour struct input_event et les constantes d'événements (KEY_0, etc.)
#include <stdio.h>           // Pour printf(), perror()
#include <stdlib.h>          // Pour EXIT_FAILURE
#include <string.h>          // Pour strlen()
#include <unistd.h>          // Pour read(), write(), close()

// Chemin vers le fichier d'entrée des boutons
#define DEV_INPUT "/dev/input/event0"

// Chemins vers les fichiers brightness des LED (LED2 et LED3)
#define LED0 "/sys/class/leds/led2/brightness"
#define LED1 "/sys/class/leds/led3/brightness"

// Fonction utilitaire pour allumer ou éteindre une LED
static int write_led(const char *path, int on) {
    int fd = open(path, O_WRONLY); // Ouvre le fichier brightness en écriture
    if (fd < 0) return -1;         // Retourne une erreur si échec
    const char *val = on ? "1" : "0";  // "1" pour allumer, "0" pour éteindre
    write(fd, val, 1);             // Écrit un octet ("0" ou "1")
    close(fd);                     // Ferme le fichier
    return 0;
}

int main(void) {
    // Ouverture du fichier d'entrée (event0) en lecture seule
    int fd = open(DEV_INPUT, O_RDONLY);
    if (fd < 0) {
        perror("open event");
        return EXIT_FAILURE;
    }

    // Variables pour suivre l’état courant des LED
    int led0_state = 0;
    int led1_state = 0;

    // Mise à l’état initial : toutes les LED éteintes
    write_led(LED0, 0);
    write_led(LED1, 0);

    // Structure contenant les événements lus depuis le périphérique d’entrée
    struct input_event ev;

    puts("Appuyez sur les boutons KEY_0 ou KEY_1 pour allumer/éteindre les LED correspondantes\n(Ctrl-C pour quitter)");

    // Boucle principale : attend des événements sur /dev/input/event0
    while (read(fd, &ev, sizeof(ev)) == sizeof(ev)) {
        // On ne s'intéresse qu'aux événements de type EV_KEY et à leur activation (value == 1)
        if (ev.type != EV_KEY || ev.value != 1) continue;

        switch (ev.code) {
            case KEY_0:
                // Inversion de l’état de LED2
                led0_state ^= 1;
                write_led(LED0, led0_state);
                printf("KEY_0 press → LED2 %s\n", led0_state ? "ON" : "OFF");
                break;

            case KEY_1:
                // Inversion de l’état de LED3
                led1_state ^= 1;
                write_led(LED1, led1_state);
                printf("KEY_1 press → LED3 %s\n", led1_state ? "ON" : "OFF");
                break;
        }
    }

    close(fd); // On ferme le descripteur d’entrée si la boucle se termine (ex: Ctrl-C)
    return EXIT_SUCCESS;
}
```

---

### ⚙️ Compilation croisée et test sur la cible

```bash
$ source /opt/mi11/poky/3.1.23/cortexa7thf-neon-vfpv4/environment-setup-cortexa7t2hf-neon-vfpv4-poky-linux-gnueabi
$ $CC buttons_toggle.c -o buttons
$ scp buttons root@joypinote:/home/root/tp2/
```

Puis côté cible Joy-Pi Note :

```bash
root@joypinote:~/tp2# ./buttons
Appuyez sur les boutons KEY_0 ou KEY_1 pour allumer/éteindre les LED correspondantes
```

✅ Fonctionnement attendu :

* Un appui sur KEY\_0 allume/éteint `led2`.
* Un appui sur KEY\_1 allume/éteint `led3`.

---

### 📌 Analyse

* Le programme montre l’usage d’un périphérique `eventX` avec `read()`.
* La boucle est réactive et ne fait aucune attente bloquante, donc **idéal pour systèmes embarqués temps réel**.
* L’état logiciel des LED est maintenu localement (dans les variables `led0_state` et `led1_state`).
* Le programme est facilement extensible à d’autres boutons ou à des logiques plus complexes (séquences, clignotement, gestion par interruption, etc.).

---

## ⏱️ Exercice 4 — Latence des temporisations avec nanosleep()

Cet exercice a pour but de mesurer l'évolution de la latence réelle d'une tâche périodique simple (1 ms de temporisation) lorsque le système est soumis à une charge CPU croissante. L'exercice permet d'évaluer la prédictibilité des temps de réveil d'une tâche sous différents niveaux de stress processeur, en utilisant des appels à `nanosleep()` et des horloges à haute précision via `clock_gettime()`.

---

### 📊 Question 4.1 — Temps totaux sans et avec charge CPU

### ⚙️ Description du programme de base

Le programme de base utilise une boucle de 10 000 itérations avec `nanosleep()` pour une pause de 1 ms par itération. Le temps total est mesuré à l'aide de `clock_gettime()` avec l'horloge `CLOCK_REALTIME`.

### ✅ Compilation (cross-compilation Poky)

```bash
$ source /opt/poky/3.1.23/cortexa7thf-neon-vfpv4/environment-setup-cortexa7t2hf-neon-vfpv4-poky-linux-gnueabi
$ $CC latency.c -o latency
```

### ⚖️ Exécution

```bash
$ ./latency
```

### 📊 Résultats (temps total)

| Commande CPU       | Temps total (ms) |
| ------------------ | ---------------- |
| Sans stress        | ≈10 680 ms       |
| `stress --cpu 100` | ≈13 370 ms       |
| `stress --cpu 200` | ≈26 300 ms       |

### 🤔 Interprétation

* Sous charge CPU, le temps total augmente nettement.
* Cela montre que la planification du noyau est moins réactive, dû au nombre important de threads actifs concurrents.
* Les interruptions de timer sont retardées, ce qui rend `nanosleep()` imprécis.

---

### 📊 Question 4.2 — Analyse des latences min / max / moyenne

### ✍️ Description de l'amélioration

Le programme a été modifié pour mesurer, à chaque itération, la différence entre la durée observée du `nanosleep()` et la durée demandée (1 ms). Cela permet de calculer une **latence effective**.

On conserve :

* La latence minimale (`min`)
* La latence maximale (`max`)
* La moyenne (à partir de la somme cumulée `sum` divisée par `ITERATIONS`)

### 📊 Résultats obtenus

| Charge CPU         | Latence min (us) | Latence max (us) | Latence moyenne (us) |
| ------------------ | ---------------- | ---------------- | -------------------- |
| Sans stress        | 61               | 176              | 66.9                 |
| `stress --cpu 100` | 21               | 69 418           | 335.8                |
| `stress --cpu 200` | 26               | 404 738          | 1 631.4              |

### 📝 Analyse et conclusions

* Plus la charge CPU augmente, plus la latence max explose.
* La moyenne augmente elle aussi de façon significative.
* Sous forte charge, le réveil d'une tâche planifiée par `nanosleep()` devient imprévisible.

### 🔧 Solutions potentielles pour réduire les latences

* Utiliser un ordonnanceur temps réel via `sched_setscheduler()` (ex : `SCHED_FIFO`).
* Fixer l'affinité CPU avec `sched_setaffinity()` pour éviter la migration de processus.
* Utiliser `CLOCK_MONOTONIC_RAW` pour éviter les sauts d'horloge réglés par NTP.
* Dédier un coeur à la tâche ?
* Utiliser des timers matériels ou des mécanismes comme `timerfd`.

---

### 📄 Question 4.3 — Code source complet

```c
#define _GNU_SOURCE          // Active certaines extensions POSIX/GNU (utile ici pour clock_gettime)
#include <stdio.h>           // Pour printf()
#include <stdlib.h>          // Pour EXIT_SUCCESS, EXIT_FAILURE
#include <time.h>            // Pour struct timespec, clock_gettime(), nanosleep()
#include <stdint.h>          // Pour les entiers de taille fixe (uint64_t)
#include <unistd.h>          // Pour sleep() (non utilisé ici) et nanosleep()

// Nombre d'itérations de la boucle de test
#define ITERATIONS 10000

// Nombre de nanosecondes dans une milliseconde
#define NS_PER_MS  1000000L

// Fonction utilitaire : retourne le temps actuel en nanosecondes depuis l'époque Unix
static inline uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);  // Récupère l'heure système
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;  // Conversion en nanosecondes
}

int main(void)
{
    // Demande de pause de 1 milliseconde
    struct timespec req = { .tv_sec = 0, .tv_nsec = NS_PER_MS };

    // Initialisation des variables de latence
    uint64_t min = UINT64_MAX;  // Latence minimale (initialisée au max)
    uint64_t max = 0;           // Latence maximale
    uint64_t sum = 0;           // Somme des latences pour calculer la moyenne

    // Heure de début du test complet
    uint64_t t_start = now_ns();

    // Boucle de test sur ITERATIONS (10 000) itérations
    for (int i = 0; i < ITERATIONS; ++i) {
        uint64_t before = now_ns();        // Timestamp avant l'attente
        nanosleep(&req, NULL);             // Pause de 1 ms
        uint64_t after = now_ns();         // Timestamp après l'attente

        // Calcul de la latence réelle = (durée observée) - (durée demandée)
        uint64_t latency = after - before - NS_PER_MS;

        // Mise à jour des statistiques
        if (latency < min) min = latency;
        if (latency > max) max = latency;
        sum += latency;
    }

    // Heure de fin du test
    uint64_t t_end = now_ns();

    // Conversion du temps total en millisecondes
    double total_ms = (t_end - t_start) / 1e6;

    // Moyenne des latences en microsecondes
    double avg_us   = (sum / ITERATIONS) / 1e3;

    // Affichage des résultats
    printf("=== Résultats ===\n");
    printf("Temps total : %.2f ms\n", total_ms);
    printf("Latence min : %llu µs\n", (unsigned long long)(min / 1000));
    printf("Latence max : %llu µs\n", (unsigned long long)(max / 1000));
    printf("Latence moy : %.1f µs\n", avg_us);

    return 0;
}
```

---

### 🔬 Pistes de prolongement

* Automatiser les mesures avec un script bash qui teste plusieurs niveaux de stress (`stress --cpu N` avec N variant).
* Générer un histogramme des latences avec gnuplot ou Python matplotlib.
* Implémenter une version avec `timerfd_create()` ou `clock_nanosleep()` pour comparer.
* Tester sur un système Xenomai avec `rt_task_sleep()` pour observer le gain de précision.

---

## 🎮 Exercice 5 — Joystick + LCD

Dans cet exercice, nous interagissons avec deux périphériques accessibles via le système de fichiers :

* **Le joystick** via `/dev/input/event1` (événements `EV_ABS` pour les axes X/Y),
* **L’écran LCD** via `/dev/lcd` (interface d’écriture simple).

---

### 🔍 5.1 Inspection des périphériques d’entrée

```bash
# Liste des périphériques d'entrée
cat /proc/bus/input/devices
```

Extrait obtenu sur la cible :

```
I: Bus=0019 Vendor=0001 Product=0001 Version=0100
N: Name="joypinote_keypad"
H: Handlers=kbd event0
...

I: Bus=0019 Vendor=0000 Product=0000 Version=0000
N: Name="joypinote_joystick"
H: Handlers=js0 event1
```

👉 Le joystick est donc accessible via `/dev/input/event1`.

---

### 🌎 Question 5.2 — Caractéristiques du joystick

* Il s'agit d'un **périphérique d'entrée analogique**, comme les boutons, mais générant des événements `EV_ABS` pour les mouvements sur les axes `ABS_X` (horizontal) et `ABS_Y` (vertical).
* Il transmet des données via des structures `input_event`, lues avec `read()`.

---

### 📃 LCD : test d'écriture depuis la ligne de commande

Le périphérique LCD est accessible via :

```bash
/dev/lcd
```

On peut tester son fonctionnement avec :

```bash
echo "Hello" > /dev/lcd
```

Si l'affichage réagit, le lien avec le LCD fonctionne.

---

### 📄 Question 5.3 — Commande utilisée pour écrire sur le LCD

```bash
echo "Texte test" > /dev/lcd
```

Cela permet d'afficher du texte statique sur l'écran.

---

## 📑 Question 5.4 — Code source complet `joylcd.c`

```c
#define _GNU_SOURCE               // Active certaines extensions POSIX/GNU (par ex. pour open())
#include <fcntl.h>               // Pour open(), O_WRONLY, O_RDONLY
#include <linux/input.h>         // Pour struct input_event et les codes d'événements (ABS_X, etc.)
#include <stdio.h>               // Pour printf(), perror(), snprintf()
#include <stdlib.h>              // Pour EXIT_SUCCESS, EXIT_FAILURE
#include <string.h>              // Pour strlen()
#include <unistd.h>              // Pour read(), write(), close()

// Définit le chemin du périphérique d’entrée joystick
#define DEV_JOY "/dev/input/event1"

// Définit le fichier spécial correspondant à l'écran LCD
#define DEV_LCD "/dev/lcd"

// Fonction utilitaire qui affiche les coordonnées X/Y sur le LCD
static void lcd_print(int fd, int x, int y)
{
    char buf[64];
    // Met en forme les coordonnées en texte, avec un alignement sur 3 caractères
    snprintf(buf, sizeof(buf), "X = %-3d\nY = %-3d\n", x, y);
    // Écrit le texte sur l’écran LCD (via le descripteur ouvert en écriture)
    write(fd, buf, strlen(buf));
}

int main(void)
{
    // Ouverture du fichier LCD en écriture seule
    int fd_lcd = open(DEV_LCD, O_WRONLY);
    if (fd_lcd < 0) {
        perror("open lcd");             // Affiche l’erreur si échec
        return EXIT_FAILURE;
    }

    // Ouverture du périphérique joystick en lecture seule
    int fd_joy = open(DEV_JOY, O_RDONLY);
    if (fd_joy < 0) {
        perror("open joystick");        // Affiche l’erreur si échec
        close(fd_lcd);                  // Ferme le LCD déjà ouvert
        return EXIT_FAILURE;
    }

    struct input_event ev;              // Structure contenant un événement du joystick
    int x = 0, y = 0;                   // Coordonnées X/Y courantes (initialisées à 0)

    // Boucle infinie : lit les événements du joystick en continu
    while (read(fd_joy, &ev, sizeof(ev)) == sizeof(ev)) {
        // On ne s’intéresse qu’aux événements de type EV_ABS (axes analogiques)
        if (ev.type != EV_ABS) continue;

        // Si l’axe X a changé → on met à jour la valeur de x
        if (ev.code == ABS_X && ev.value != x)
            x = ev.value;

        // Si l’axe Y a changé → on met à jour la valeur de y
        else if (ev.code == ABS_Y && ev.value != y)
            y = ev.value;

        // Si aucun des deux n’a changé → on ignore
        else
            continue;

        // Affichage des nouvelles coordonnées sur le LCD
        lcd_print(fd_lcd, x, y);
    }

    // Fermeture des descripteurs de fichiers
    close(fd_joy);
    close(fd_lcd);
    return EXIT_SUCCESS;
}
```

---

### ⚙️ Compilation et exécution

```bash
$ source /opt/poky/3.1.23/cortexa7thf-neon-vfpv4/environment-setup-cortexa7t2hf-neon-vfpv4-poky-linux
$ $CC joylcd.c -o joylcd
$ scp joylcd root@joypinote:/home/root/tp2/
$ ssh root@joypinote
root@joypinote:~/tp2# ./joylcd
```

---

#### ❓ Que fait le programme `joylcd` ?

Il affiche en **temps réel** les coordonnées `X` et `Y` du joystick sur l’écran LCD. À chaque modification de position du joystick, une ligne comme :

```
X = 518
Y = 504
```

sera affichée.

#### ❓ Quels types d’événements sont capturés ?

Uniquement les événements `EV_ABS` qui concernent les mouvements continus (analogiques). Les deux codes pris en compte sont :

* `ABS_X` → axe horizontal
* `ABS_Y` → axe vertical

#### ❓ Pourquoi faut-il filtrer les événements ?

Car le périphérique génère aussi d'autres types d’événements (synchronisation, pressions bouton, etc.) qu’il ne faut pas interpréter pour ne pas surcharger inutilement l’affichage ou provoquer des comportements erronés.

#### ❓ Pourquoi on utilise `!=` pour détecter un changement de valeur ?

Pour éviter d’écrire la même valeur plusieurs fois d’affilée sur l’écran. Cela :

* allège la charge CPU,
* réduit l’usure de l’écran (si applicable),
* améliore la lisibilité.

#### ❓ Pourquoi cette gestion directe est-elle acceptable ici ?

Dans un système embarqué comme celui-ci :

* l’interface LCD est accessible via un fichier,
* la fréquence de mise à jour reste faible (quelques fois par seconde),
* les besoins temps réel sont souples → une approche simple et synchrone est suffisante.

---

### 📚 Analyse pédagogique

Cet exercice démontre :

* L’interfaçage direct avec des périphériques `input` Linux,
* La lecture d’événements `EV_ABS` via `read()`,
* Le traitement et l’affichage d’informations en temps réel sur un **écran LCD embarqué**.

💡 Prolongement possible :

* Afficher un curseur qui se déplace sur l’écran,
* Ajouter un recalibrage dynamique,
* Combiner joystick et boutons pour construire un mini menu embarqué interactif.

---

## 🧩 Conclusion générale

Ce TP a permis d’approfondir la maîtrise du développement embarqué sur une plateforme ARM équipée de Linux. Grâce à l’étude de périphériques simples mais variés (LEDs, boutons, joystick, écran LCD), nous avons pu explorer :

* la compilation croisée,
* la communication avec des périphériques via le `sysfs` ou les fichiers de type `/dev/input`,
* l'écriture de programmes réactifs à des événements matériels,
* la manipulation de l’affichage sur un LCD embarqué en temps réel.

Cette approche concrète met en évidence la richesse de l’environnement Linux embarqué, tout en soulignant l’importance de la rigueur dans la gestion des entrées/sorties système.