# Compte Rendu TP1 - Linux embarqué (MI11 / AI39 - Printemps 2025)

**Nom :** [Hugo Pereira](https://github.com/tigrou23) & Maher Zizouni

**UV :** AI39

**TP :** Linux embarqué - TP1

**Encadrant :** Guillaume Sanahuja

---

## Introduction Générale

Le TP1 de l’UV MI11 « Linux Embarqué » a pour objectif principal d’introduire les étudiants aux outils et méthodologies nécessaires au développement d’un système Linux embarqué. Il repose sur une machine virtuelle Debian configurée avec Yocto Project pour la génération d’images Linux à destination d’une carte Raspberry Pi 4 intégrée dans un Joy-Pi-Note.

Ce compte rendu détaille de manière exhaustive l’ensemble des manipulations réalisées, les problèmes rencontrés et leurs résolutions, les outils utilisés (notamment BitBake, opkg, et BusyBox), ainsi que les concepts techniques sous-jacents comme le boot réseau, la compilation croisée, la gestion du device tree ou encore la personnalisation du noyau Linux.

L’approche suivie est systémique : chaque question posée par l’énoncé du TP est replacée dans le contexte général du système embarqué, et enrichie par des observations, des commandes concrètes, des explications techniques et des réflexions personnelles.

---

## Travail Préalable

### Question 0.1 — Caractéristiques de la carte

La carte cible est un **Raspberry Pi 4 Model B** intégrée dans un Joy-Pi-Note. Il s’agit d’un ordinateur monocarte performant destiné initialement à des usages pédagogiques, mais aussi adapté à l’embarqué grâce à sa flexibilité.

* **Référence du processeur :** Broadcom BCM2711
* **Architecture CPU :** ARMv8-A 64 bits
* **Nombre de cœurs :** 4 (quad-core Cortex-A72)
* **Fréquence maximale :** 1,5 GHz
* **Jeu d’instructions :** ARMv8-A (AARCH64)
* **RAM :** 4 Go de LPDDR4-3200 (modèle de test)
* **GPU :** Broadcom VideoCore VI
* **Interfaces :** USB, HDMI, GPIO, UART, SPI, I2C, etc.

La carte offre une plateforme moderne et largement utilisée dans les contextes d’embarqué, avec une documentation abondante et une large compatibilité logicielle.

### Question 0.2 — Méthodes de stockage du système

**Moyens de stockage disponibles :**

1. **Carte microSD** : méthode la plus simple, utilisée en production. Le chargeur d’amorçage du Raspberry Pi est conçu pour y chercher un système de fichiers FAT contenant le noyau.
2. **Clé USB ou disque externe** : possible depuis les dernières versions du firmware du Pi, mais plus lent et moins fiable.
4. **Boot réseau (TFTP/NFS)** : méthode avancée utilisée dans ce TP. Nécessite un serveur DHCP pour fournir l’adresse IP, un serveur TFTP pour le noyau et le DTB, et un montage NFS ou copie locale du rootfs. Permet de modifier rapidement les fichiers côté développeur sans démonter la carte.

Cette méthode réduit les manipulations physiques et accélère le cycle développement/test.

---

## Exercice 1 : Prise en main de Yocto

### Objectif

Yocto Project permet de générer des images Linux embarquées personnalisées. Il s’appuie sur `bitbake`, un moteur de recettes qui compile, configure, assemble et empaquette les composants du système. Ce premier exercice consiste à découvrir les répertoires importants, adapter la configuration pour notre cible, puis compiler une image minimale (`core-image-base`).

### Question 1.1 — Analyse des répertoires

#### `/opt/mi11/poky/build/conf`

Contient les deux fichiers de configuration clés :

* `local.conf` : déclare les options globales de compilation : nom de la machine (`MACHINE`), type d'image, options de compilation, variables d’environnement, etc.
* `bblayers.conf` : contient la liste des « layers » Yocto à utiliser. Chaque layer fournit des recettes, des définitions de classes, des fichiers de configuration pour un matériel ou une distribution.

#### `/opt/mi11/meta-raspberrypi`

Layer officiel pour Raspberry Pi. Fournit des recettes spécifiques à cette famille de SoC (firmware, kernel, bootloader, device tree, etc.).

#### `/opt/mi11/meta-joypinote`

Layer créé spécifiquement pour notre matériel cible, qui est un Raspberry Pi 4 intégré dans un Joy-Pi-Note. Contient des fichiers de configuration, des recettes personnalisées, et probablement des définitions de device trees.

#### `/opt/mi11/meta-mi11`

Layer pédagogique propre à l’UV. Il peut contenir des paquets facultatifs, des scripts d’installation ou des configurations utiles pour les exercices.

### Question 1.2 — Modifications des fichiers de configuration

#### Dans `bblayers.conf`

Ajout des trois layers :

```bash
  /opt/mi11/meta-raspberrypi \
  /opt/mi11/meta-joypinote \
  /opt/mi11/meta-mi11 \
```

#### Dans `local.conf`

Définition de la machine cible :

```bash
MACHINE = "joypinote"
```

Cela permet à bitbake de sélectionner automatiquement les recettes, fichiers de configuration et options de compilation propres à cette carte.

### Question 1.3 — Compilation

#### Initialisation de l’environnement :

```bash
cd /opt/mi11/poky
source ../poky-dunfell-23.0.23/oe-init-build-env
```

#### Compilation :

```bash
bitbake core-image-base
```

Cette commande génère un système Linux minimal, avec shell, init, et utilitaires de base, en s’appuyant sur les couches et recettes définies.

#### Résultat dans `/opt/mi11/poky/build/tmp/deploy/images/joypinote` :

* `zImage` : noyau Linux
* `bcm2711-rpi-4-b.dtb` : device tree binaire
* `core-image-base-joypinote.tar.bz2` : système de fichiers racine
* `manifest`, `testdata.json`, `modules.tgz` : métadonnées, modules compilés, dépendances

#### Comparaison avec le linux de la VM :

`df -f` permet de comparer la taille des systèmes de fichiers. Par soucis de précision, on exclut du calcul les fichiers mi11 :

```bash
mi11linux@mi11linux:/opt/mi11/poky/build/tmp/deploy/images/joypinote$ du -sh /opt/mi11/
  9,0G    /opt/mi11/
```

* \~10 Go (interface graphique, paquets préinstallés, langages...)
* Yocto : \~20 Mo, système minimaliste, rapide à charger et parfaitement adapté aux contraintes de l’embarqué

La petite taille et la modularité sont les atouts majeurs de Yocto.

---

## Exercice 2 : Démarrage sur le noyau et le système de fichiers générés

### Objectif

Le but est ici de démarrer la cible Joy-Pi-Note via le réseau, à l’aide des fichiers générés dans l’exercice précédent. Cela permet de tester le noyau et le système de fichiers sans support physique, et de diagnostiquer la séquence de boot.

### Question 2.1 — Analyse de la séquence de démarrage

Le Raspberry Pi, au démarrage, recherche un fichier nommé `kernel.img` dans le dossier TFTP. S’il le trouve, il charge ce fichier comme noyau Linux. Ensuite, il utilise un fichier `bcm2711-rpi-4-b.dtb` pour connaître la topologie matérielle (device tree). Le noyau cherche alors un système de fichiers racine.

Si l’un de ces éléments est manquant :

* Pas de `kernel.img` → échec du démarrage
* Pas de `.dtb` → erreurs matérielles
* Pas de rootfs ou absence d’`init` → kernel panic

### Question 2.2 — Problème suivant après fourniture du noyau

Lorsque le noyau démarre mais qu’il ne trouve pas de système de fichiers avec un programme `/sbin/init` valide, un message d’erreur apparaît :

```
No init found. Try passing init= option to kernel. Panic - not syncing: Attempted to kill init!
```

### Question 2.3 — Actions nécessaires pour un boot fonctionnel

1. Copier `zImage` dans `/tftpboot/kernel.img`
2. Extraire `core-image-base-joypinote.tar.bz2` dans `/tftpboot/rootfs`
3. Vérifier la présence de `/sbin/init` (souvent un lien vers `busybox`)
4. Configurer `udhcpd` pour l’interface `ttyUSB0` afin que la cible obtienne une IP via DHCP

### Question 2.4 — Description complète du processus de boot

Le processus de démarrage observé est le suivant :

1. La carte reçoit une IP via DHCP
2. Elle télécharge le noyau via TFTP
3. Le noyau se charge en mémoire
4. Le device tree est chargé et interprété
5. Le rootfs est monté depuis `/tftpboot/rootfs`
6. Le programme `init` est lancé → il appelle `rcS`, qui initialise les périphériques et services de base
7. Un getty est lancé sur le terminal principal, fournissant un shell root sans mot de passe

### Question 2.5 — Adresse IP de la cible

* Trouvée par observation du fichier `udhcpd.leases`
* Vérifiée via `ifconfig` sur la cible : `192.168.0.49`

### Question 2.6 — Analyse du fichier `/proc/devices`

Ce fichier présente deux sections :

* `Character devices` : périphériques caractères (ex : tty, gpio, i2c...)
* `Block devices` : périphériques blocs (ex : disques, partitions...)

Chaque ligne comporte un **major number** utilisé dans `/dev` pour mapper un fichier de périphérique à un pilote noyau. Exemple :

```
204 ttyAMA
```

→ `/dev/ttyAMA0` est le port série UART utilisé pour la communication console.

### Question 2.7 — Particularité de `/bin`, `/sbin`, etc.

On observe que beaucoup d’outils sont en fait des liens symboliques vers `/bin/busybox`. Cela signifie que le système utilise **BusyBox**, un exécutable unique qui implémente un grand nombre de commandes Unix. Il s’agit d’une solution très utilisée dans les systèmes embarqués car elle réduit fortement l’espace disque nécessaire.

Exemple de commande :

```bash
ls -l /bin/ls
lrwxrwxrwx 1 root root 7 /bin/ls -> busybox
```

---

## Exercice 3 : Ajout de paquets

### Objectif

Cette section a pour but de montrer comment enrichir un système de fichiers Yocto minimaliste avec des paquets logiciels supplémentaires, en l’occurrence l’éditeur de texte `nano`. L’objectif est également de comprendre comment fonctionnent les formats de paquets `.ipk` et le gestionnaire de paquets `opkg`.

Dans un système embarqué, les paquets sont ajoutés soit directement dans l’image lors de la compilation (`IMAGE_INSTALL_append`), soit dynamiquement après le boot avec un gestionnaire comme `opkg`. Nous allons expérimenter cette deuxième approche.

### Question 3.1 — Organisation du dossier `/opt/mi11/poky/build/tmp/deploy/ipk`

Ce répertoire contient les paquets compilés sous forme de fichiers `.ipk`, format similaire aux `.deb` mais plus léger. L’arborescence est structurée par architecture cible :

* `cortexa7t2hf-neon-vfpv4/` : dossiers contenant les `.ipk` destinés à notre architecture ARM avec extensions VFP (Floating Point) et NEON.
* `all/` : paquets indépendants de l’architecture (scripts, configurations...)
* `Packages`, `Packages.gz` : fichiers d’index permettant à `opkg` de trouver les paquets et leurs métadonnées.

À l’intérieur du dossier `cortexa7t2hf-neon-vfpv4`, on trouve notamment :

* `nano_2.2.5-r4.0_cortexa7t2hf-neon-vfpv4.ipk` : paquet principal
* `libtic5`, `ncurses`, `ncurses-tools` : dépendances nécessaires au bon fonctionnement de `nano`

Ces fichiers peuvent être installés manuellement (via `opkg install fichier.ipk`) ou via un dépôt HTTP configuré.

### Question 3.2 — Paquets relatifs à `nano`

Le paquet `nano` dépend des bibliothèques liées au terminal, notamment :

* `libtic5` : Terminal Information Compiler (permet à ncurses de fonctionner)
* `ncurses` : bibliothèque de gestion des interfaces textuelles avancées
* `ncurses-tools` : utilitaires (reset, clear, etc.)

Ces dépendances doivent être installées dans le bon ordre ou via un gestionnaire de paquets capable de les résoudre automatiquement.

### Question 3.3 — Problème rencontré lors de l’installation manuelle

Tentative d’installation initiale :

```bash
opkg install nano_2.2.5-r4.0_cortexa7t2hf-neon-vfpv4.ipk
```

Erreur rencontrée :

```
 * Solver encountered 1 problem(s):
 * - nothing provides ncurses-tools needed by nano
```

Cela signifie que `opkg` ne parvient pas à résoudre les dépendances automatiquement car il n’a pas de dépôt configuré pour télécharger les fichiers manquants. C’est un comportement attendu dans un système de fichiers embarqué minimaliste.

### Question 3.4 — Mise en place d’un dépôt HTTP local

#### Étapes de configuration :

1. Copier les paquets `.ipk` dans un dossier exporté via HTTP (ex: `/srv/www/ipk/`)
2. Lancer un serveur HTTP (simplement avec `python3`) :

```bash
cd /opt/mi11/poky/build/tmp/deploy/ipk
python3 -m http.server 80
```

3. Éditer `/etc/opkg/opkg.conf` sur la cible pour ajouter :

```
src/gz cortexa7t2hf-neon-vfpv4 http://192.168.0.1/cortexa7t2hf-neon-vfpv4
src/gz joypinote http://192.168.0.1/joypinote
```

4. Regénérer les index de paquets pour permettre leur téléchargement :

```bash
bitbake package-index
```

5. Sur la cible :

```bash
opkg update
opkg install nano
```

#### Explication de ce qui se passe :

`opkg update` télécharge les fichiers `Packages.gz` depuis le dépôt HTTP, met à jour son cache local, puis `opkg install nano` télécharge toutes les dépendances manquantes et les installe dans l’ordre requis. Cette méthode reflète une configuration réaliste d’un système embarqué qui télécharge ses mises à jour à la volée.

#### Remarques :

* Le serveur HTTP est indispensable au bon fonctionnement de `opkg`.
* Il peut être automatisé dans un script pour des déploiements à grande échelle.
* Cette méthode permet d’avoir un système évolutif sans recompiler une nouvelle image à chaque fois.

---

## Exercice 4 : Compilation manuelle du noyau et gestion du matériel GPIO

### Objectif

Dans cet exercice, nous nous affranchissons de la chaîne Yocto pour recompiler manuellement le noyau Linux avec des options spécifiques. En particulier, nous allons :

* Compiler un noyau avec la configuration adaptée à notre carte (`joypinote_defconfig`),
* Activer le support des LEDs connectées par GPIO dans le noyau via `menuconfig`,
* Modifier le Device Tree pour décrire les LEDs,
* Déployer le nouveau noyau et vérifier son fonctionnement.

### Préparation de la compilation croisée

Les systèmes embarqués nécessitent la **compilation croisée**, car le processeur cible (ARM) est différent de celui de la machine hôte (x86\_64). Pour cela, nous utilisons la chaîne fournie par Yocto :

```bash
. /opt/poky/3.1.23/cortexa7thf-neon-vfpv4/environment-setup-cortexa7t2hf-neon-vfpv4-poky-linux-gnueabi
```

Ce script exporte les variables suivantes :

* `CC`, `CXX`, `LD` → versions ARM de GCC/LD
* `ARCH=arm`
* `CFLAGS`, `LDFLAGS`, `SYSROOT` → pour compiler contre les bons headers/libs
* `PATH` → ajout du répertoire contenant les outils croisés

### Configuration du noyau

Nous utilisons une configuration de base fournie par les mainteneurs :

```bash
make ARCH=arm joypinote_defconfig
```

Cela initialise le fichier `.config` avec les options correspondant à notre cible.

Ensuite, nous lançons le menu de configuration interactive :

```bash
make ARCH=arm menuconfig
```

Dans ce menu, nous activons :

```
Device Drivers  →
  [*] LED Support  →
    [*] LED Support for GPIO connected LEDs
```

Cela permet au noyau d’exposer les LEDs connectées aux broches GPIO via le sous-système `/sys/class/leds`.

### Modification du Device Tree

Le Device Tree décrit le matériel au noyau. Pour déclarer les LEDs, nous modifions ou ajoutons dans le fichier `arch/arm/boot/dts/bcm2711-rpi-4-b.dts` les nœuds suivants :

```dts
&leds {
    led1: led1 {
        label = "led1";
        gpios = <&gpio 5 GPIO_ACTIVE_LOW>;
    };

    led2: led2 {
        label = "led2";
        gpios = <&gpio 6 GPIO_ACTIVE_LOW>;
    };
};
```

Chaque nœud définit une LED connectée à une broche GPIO. Les paramètres sont :

* `label` : nom affiché dans `/sys/class/leds`
* `gpios` : lien vers le contrôleur GPIO et numéro de broche
* `GPIO_ACTIVE_LOW` : indique que la LED est active à l’état bas

### Compilation du noyau et des fichiers DTB

Nous lançons ensuite la compilation avec :

```bash
make -j6 ARCH=arm bzImage dtbs
```

Résultat :

* `arch/arm/boot/zImage` : noyau compressé
* `arch/arm/boot/dts/*.dtb` : Device Tree Blobs

Nous copions ensuite ces fichiers vers le serveur TFTP :

```bash
sudo cp arch/arm/boot/zImage /tftpboot/kernel.img
sudo cp arch/arm/boot/dts/bcm2711-rpi-4-b.dtb /tftpboot/
```

### Vérification au démarrage

Au redémarrage de la carte, le noyau mis à jour se charge. Nous validons :

1. Que la date de build du noyau est récente :

```bash
dmesg | grep Linux
```

2. Que les LEDs sont visibles :

```bash
ls /sys/class/leds
```

→ `led1`, `led2` doivent apparaître

3. Que nous pouvons les contrôler :

```bash
echo 1 > /sys/class/leds/led1/brightness
echo 0 > /sys/class/leds/led2/brightness
```

4. Que nous pouvons modifier leur `trigger` (comportement automatique) :

```bash
echo heartbeat > /sys/class/leds/led2/trigger
```

Cela fera clignoter la LED au rythme du cœur système (indicateur d’activité).

### Analyse critique

* Le sous-système `leds` de Linux est bien intégré, et son interaction avec le sysfs est immédiate et simple à manipuler.
* La compilation croisée permet un contrôle complet sur les options du noyau, mais demande rigueur et méthode (clean du `.config`, installation des dtbs...)
* L'utilisation du Device Tree est incontournable pour les cartes modernes : il remplace les anciens fichiers de configuration statique (platform data).

---

## Exercice 5 : Compilation et déploiement des modules du noyau

### Objectif

Dans cet exercice, nous allons compiler et installer les modules du noyau Linux manuellement, dans le but d’avoir un système modulaire. Cela permet de charger dynamiquement des fonctionnalités supplémentaires (drivers, systèmes de fichiers, etc.) sans avoir à recompiler l’intégralité du noyau.

Les modules sont des fichiers objets `.ko` (kernel object) qui peuvent être chargés à l’exécution via `insmod` ou `modprobe`. Ils sont souvent utilisés pour gérer des périphériques ou fonctionnalités facultatives.

### Compilation des modules

Une fois le noyau compilé avec `make bzImage`, on peut compiler les modules correspondants :

```bash
make ARCH=arm modules
```

Cette étape prend en compte les options activées dans le fichier `.config` avec l’attribut `M` (module). Elle génère de nombreux fichiers `.ko` dans les répertoires de `drivers/`, `fs/`, `net/`, etc.

### Installation des modules

Une fois la compilation terminée, on installe les modules dans le système de fichiers root embarqué à l’emplacement standard `/lib/modules/<version>/` :

```bash
make ARCH=arm INSTALL_MOD_PATH=/tftpboot/rootfs modules_install
```

Cette commande :

* Crée les répertoires `/lib/modules/<version>` dans le rootfs,
* Copie tous les fichiers `.ko`,
* Génère les fichiers `modules.dep`, `modules.alias`, etc., utilisés par `modprobe`

### Déploiement sur la cible

La prochaine fois que la carte cible boote avec ce rootfs, elle a accès à tous les modules :

* via chargement automatique (selon les triggers du noyau),
* ou manuellement :

```bash
modprobe nom_du_module
```

ou

```bash
insmod /lib/modules/5.4.0/extra/mon_module.ko
```

### Vérification sur la cible

1. Lister les modules disponibles :

```bash
find /lib/modules -name "*.ko"
```

2. Lister les modules actuellement chargés :

```bash
lsmod
```

3. Afficher les dépendances :

```bash
modinfo <nom_module>
```

4. Charger un module (ex : loop, dummy, gpio-leds...) :

```bash
modprobe dummy
```

5. Vérifier qu’il est bien actif :

```bash
lsmod | grep dummy
```

### Analyse technique

* L'utilisation de modules permet de réduire la taille du noyau principal (zImage), et d'ajouter dynamiquement des fonctionnalités.
* Cela facilite aussi le débogage et l’expérimentation sur des plateformes embarquées.
* Le `INSTALL_MOD_PATH` est crucial pour éviter d’installer les modules dans le rootfs de la machine hôte.

### Conclusion de l'exercice

Avec cette étape, nous avons désormais une chaîne complète :

* Compilation croisée du noyau et de ses modules
* Personnalisation de l’image rootfs (ajout de paquets, modules)
* Boot par le réseau avec un noyau et un système de fichiers maîtrisés de bout en bout

Nous disposons d’un environnement embarqué complet, modulaire, personnalisable, et réplicable pour les TPs suivants.

---

## Conclusion Générale du TP1

Ce TP a permis de mettre en œuvre les compétences fondamentales en développement Linux embarqué :

* Familiarisation avec Yocto Project, ses layers et ses mécanismes de build
* Compilation croisée d’un noyau Linux pour une cible ARM (Raspberry Pi 4)
* Compréhension et modification du Device Tree
* Utilisation du sysfs pour interagir avec le matériel (LEDs GPIO)
* Ajout de paquets utilisateur via un dépôt local
* Déploiement d’un rootfs complet avec modules noyau

Il a permis de comprendre la structure et les couches d’un système Linux embarqué, depuis la configuration jusqu’à la mise en service réseau d’un système fonctionnel. Les outils vus (bitbake, opkg, busybox, make, modprobe, etc.) sont essentiels à tout développeur système.

Ce premier TP pose donc les bases de l’autonomie dans le développement de solutions logicielles embarquées complexes, tant au niveau système que matériel.
