# 💻 Linux Embarqué — TPs (UTC AI39 - P25)

Bienvenue sur mon dépôt GitHub dédié aux **Travaux Pratiques de l’UV MI11 / AI39 : Linux Embarqué**, suivie au semestre de printemps 2025 à l’[Université de Technologie de Compiègne](https://www.utc.fr).

Ce projet a pour but de documenter de manière claire et approfondie les différents TP autour de la construction d’un système Linux embarqué sur architecture ARM, avec comme cible principale un **Joy-Pi-Note (Raspberry Pi 4)**.

---

## 🎯 Objectifs du projet

* Comprendre les étapes de construction d’une distribution Linux embarquée
* Utiliser **Yocto Project** pour générer images, noyaux et rootfs personnalisés
* Maîtriser la **compilation croisée** pour ARM avec une chaîne poky
* Manipuler le **Device Tree**, le **boot TFTP**, le **DHCP**, et le **NFS**
* Installer et utiliser des outils embarqués : `opkg`, `busybox`, `modprobe`, etc.
* Expérimenter avec le noyau Linux : compilation, modules, gestion GPIO/LED

Chaque TP est documenté avec un `readme.md` complet, des fichiers sources et scripts utilisés en cours de manipulation.

---

## 📂 Organisation du dépôt

```bash
.
├── tp[x]/
│   ├── readme.md         # Compte rendu complet du TP[x]
│   ├── TP[x]_sujet.pdf   # Sujet du TP[x]
│   └── ressources/       # Ressources utilisées (optionnel)
```

---

## 🧪 Liste des TP

| TP           | Thème principal                                                                               | Statut      |
|--------------|-----------------------------------------------------------------------------------------------|-------------|
| [TP1](./tp1) | Yocto, boot réseau, noyau, rootfs, GPIO, modules                                              | ✅ Terminé   |
| [TP2](./tp2) | Périphériques d'entrée/sortie sous Linux embarqué (LED, boutons, joystick, LCD)               | ✅ Terminé   |
| [TP3](./tp3) | Programmation temps réel avec Xenomai, tâches, sémaphores, synchronisation, mesure de latence | ✅ Terminé   |
| [TP4](./tp4) | Mission Pathfinder : synchronisation de tâches critiques, surveillance, inversion de priorité | ✅ Terminé   |
| TP5          | *(à venir)*                                                                                   | 🔜 En cours |
---

## 🛠 Prérequis techniques

* OS hôte : machine virtuelle ou physique sous Linux (Debian, Ubuntu, etc.)
* Chaîne de compilation poky (Yocto Dunfell 3.1)
* Serveur **TFTP** + **DHCP** fonctionnels
* Connexion série (USB/UART) avec le Joy-Pi-Note
* Environnement bash, make, python3, tar, etc.

---

## 👨‍💻 Auteur

[Hugo Pereira](https://github.com/tigrou23) & Maher Zizouni

Passionnés par le système Linux, le développement embarqué, et l’optimisation bas-niveau.
Toujours curieux d’aller voir ce qu’il y a "sous le capot".

📫 [hugo.pereira@etu.utc.fr](mailto:hugo.pereira@etu.utc.fr)

📫 [maher.zizouni@etu.utc.fr](mailto:maher.zizouni@etu.utc.fr)

---

> *“Intelligence is the ability to avoid doing work, yet getting the work done.” — Linus Torvalds*
