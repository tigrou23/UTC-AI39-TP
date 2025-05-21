# Compte Rendu TP1 - Linux embarqué (MI11 / AI39 - Printemps 2025)

**Nom :** Hugo Pereira & Maher Zizouni
**UV :** AI39
**TP :** Linux embarqué - TP1
**Encadrant :** Guillaume Sanahuja

---

## Préambule

Ce premier TP du module a pour objectif de familiariser l’étudiant avec un environnement de développement Linux embarqué complet, centré sur l’utilisation de la distribution Yocto pour une cible matérielle de type Raspberry Pi 4 enfermée dans un Joy-Pi-Note. Il met l’accent sur les pratiques concrètes de compilation, de configuration, de déploiement, et d’observation du comportement du noyau Linux et du système de fichiers embarqués.

L’intégralité des manipulations s’effectue à l’aide d’une machine virtuelle fournie par l’encadrant, et d’un environnement réseau pré-configuré. Ce compte rendu documente, étape par étape, les actions réalisées, les problèmes rencontrés, les solutions mises en œuvre ainsi que les apprentissages techniques associés.

---

## Travail Préalable

### Question 0.1 — Caractéristiques de la carte

La carte utilisée est un Raspberry Pi 4, intégré dans un Joy-Pi-Note. Ses caractéristiques principales sont les suivantes :

* **Référence processeur** : Broadcom BCM2711
* **Architecture** : ARMv8-A (Cortex-A72) 64 bits
* **Fréquence** : 1.5 GHz (quad-core)
* **Mémoire vive** : 4 Go LPDDR4 (selon le modèle)

Ces caractéristiques en font une plateforme adaptée aux environnements embarqués avec des contraintes modérées de performance.

### Question 0.2 — Stockage du noyau et du système de fichiers

Plusieurs méthodes peuvent être employées pour stocker les fichiers nécessaires au démarrage d’un système Linux embarqué :

* **Carte microSD** : solution classique et simple à utiliser.
* **Clé USB ou périphérique de stockage externe** : utilisé notamment pour le débogage.
* **Boot réseau (TFTP/NFS)** : utilisé ici ; permet une grande souplesse de mise à jour et de configuration sans intervention physique sur la cible.

---

## Exercice 1 : Prise en main de Yocto

### Objectif

L’objectif est de comprendre la structure d’un environnement Yocto, de configurer la compilation pour une cible spécifique (Joy-Pi-Note), puis de générer les binaires nécessaires (noyau, rootfs, device tree, etc.).

### Question 1.1 — Analyse des dossiers

* `/opt/mi11/poky/build/conf` :

  * `local.conf` : paramètres de compilation comme `MACHINE`, `IMAGE_FSTYPES`, etc.
  * `bblayers.conf` : liste des layers utilisés dans la compilation.

* `/opt/mi11/meta-raspberrypi` : fournit le support Yocto pour Raspberry Pi (recettes BSP, kernels, device trees, etc.)

* `/opt/mi11/meta-joypinote` : ajoute les spécificités matérielles du Joy-Pi-Note.

* `/opt/mi11/meta-mi11` : contient les recettes spécifiques à l’UV MI11 (paquets, configurations personnalisées).

### Question 1.2 — Modifications apportées

* Ajout des trois couches dans `bblayers.conf`.
* Spécification de la machine cible dans `local.conf` :

```bash
MACHINE = "joypinote"
```

Ces modifications permettent de compiler des images adaptées à notre cible matérielle.

### Question 1.3 — Résultat de la compilation

* Commande :

```bash
bitbake core-image-base
```

* Fichiers générés dans `/opt/mi11/poky/build/tmp/deploy/images/joypinote` :

  * `zImage` : image compressée du noyau Linux
  * `.dtb` : device tree binaire (décrit le matériel)
  * `.tar.bz2` : système de fichiers compressé

* Taille de l’image rootfs : environ 19,5 Mo. Comparée à une Ubuntu Desktop (plusieurs Go), l’image Yocto est minimaliste, optimisée et construite sur mesure.

---

## Exercice 2 : Boot réseau de la cible

### Question 2.1 à 2.3 — Configuration de boot

* DHCP modifié pour utiliser l’interface `ttyUSB0` (convertisseur série/USB).
* Copie de `zImage` dans `/tftpboot/kernel.img`.
* Extraction du rootfs dans `/tftpboot/rootfs`.

Commandes utilisées :

```bash
sudo tar -xf core-image-base-joypinote.tar.bz2 -C /tftpboot/rootfs
```

Sans le rootfs, le noyau boot mais échoue à monter le système de fichiers. Une fois extrait, le prompt root s’affiche correctement.

### Question 2.4 à 2.7 — Observations après boot

* Adresse IP de la cible : obtenue via DHCP ou commande `ip a`
* `/proc/devices` : liste des périphériques caractères et blocs avec leur numéro majeur
* `/dev/` contient les entrées de périphériques ; `/dev/ttyAMA0` est lié au port série utilisé
* `busybox` regroupe les outils Unix dans un binaire unique, ce qui réduit drastiquement l’espace disque utilisé par les utilitaires système.

---

## Exercice 3 : Ajout de paquets

### Compilation et installation de `nano`

```bash
bitbake nano
sudo cp nano_2.2.5-r4.0_cortexa7t2hf-neon-vfpv4.ipk /tftpboot/rootfs/
```

Sur la cible :

```bash
opkg install /nano_2.2.5-r4.0_cortexa7t2hf-neon-vfpv4.ipk
```

Erreur : dépendances manquantes (`ncurses-tools`).

### Mise en place d’un dépôt local

* Modification de `/etc/opkg/opkg.conf` :

```conf
src/gz cortexa7t2hf-neon-vfpv4 http://192.168.0.1/cortexa7t2hf-neon-vfpv4
src/gz joypinote http://192.168.0.1/joypinote
```

* Reconstruction de l’index :

```bash
bitbake package-index
```

* Démarrage d’un serveur HTTP :

```bash
cd /opt/mi11/poky/build/tmp/deploy/ipk
python3 -m http.server 80
```

Ensuite sur la cible :

```bash
opkg update
opkg install nano
```

Installation réussie avec résolution automatique des dépendances.

---

## Exercice 4 : Compilation manuelle du noyau

### Préparation de l’environnement croisé

```bash
. /opt/poky/3.1.23/cortexa7thf-neon-vfpv4/environment-setup-cortexa7t2hf-neon-vfpv4-poky-linux-gnueabi
```

* Cette commande définit le chemin du compilateur croisé (ex : `arm-poky-linux-gnueabi-gcc`) et les options d’architecture (`--sysroot`, `CC`, etc.)

### Configuration et personnalisation

```bash
make ARCH=arm joypinote_defconfig
make ARCH=arm menuconfig
```

Activation :

```text
Device Drivers > LED Support > LED support for GPIO connected LEDs
```

### Device Tree

Ajout dans `arch/arm/boot/dts/bcm2711-rpi-4-b.dts` :

```dts
led1 {
    label = "led1";
    gpios = <&gpio 5 GPIO_ACTIVE_LOW>;
};
led2 {
    label = "led2";
    gpios = <&gpio 6 GPIO_ACTIVE_LOW>;
};
```

### Compilation et déploiement

```bash
make -j6 bzImage dtbs
cp arch/arm/boot/zImage /tftpboot/kernel.img
cp arch/arm/boot/dts/bcm2711-rpi-4-b.dtb /tftpboot/
```

### Vérification

* Via `dmesg`, on peut voir la version du noyau.
* Les LEDs apparaissent dans `/sys/class/leds/`.

Manipulations :

```bash
echo 1 > /sys/class/leds/led1/brightness
echo heartbeat > /sys/class/leds/led2/trigger
```

---

## Exercice 5 : Compilation des modules noyau

### Étapes

```bash
make ARCH=arm modules
make ARCH=arm INSTALL_MOD_PATH=/tftpboot/rootfs modules_install
```

Cette commande installe les modules compilés directement dans le système de fichiers root, permettant leur chargement dynamique au démarrage ou à la demande (via `modprobe`).

---

## Conclusion

Ce TP a permis d'acquérir une vision complète de la chaîne de développement Linux embarqué. En particulier, il a permis de :

* Comprendre l’organisation de Yocto et l’utilisation des layers
* Compiler des images rootfs et des noyaux adaptés à une cible matérielle
* Effectuer un boot réseau complet
* Déboguer et enrichir un système Linux minimal
* Utiliser une chaîne de compilation croisée et personnaliser un noyau

Les compétences acquises sont fondamentales pour la suite du module, notamment l’interfaçage matériel (GPIO, LED), l’optimisation du système et le développement bas niveau.
