# BE Traitement du Signal Temps Réel – Autotune (Phelma SICOM 2026-2027)

Implémentation en temps réel d'un effet d'autotune par synthèse additive, en C/C++ sous Linux, avec l'API audio [RtAudio](https://caml.music.mcgill.ca/~gary/rtaudio/) (v6.0.1).

Tout le code se trouve dans `duplex.cpp`, qui est compilé avec le Makefile des exemples RtAudio (dossier `tests/`).

---

## État d'avancement

| Partie | Questions | État |
|---|---|---|
| Getting started with RtAudio | Q1 – Q3 | ✅ Fait |
| Debugging toolbox – Lecture de fichier | Q4 – Q6 | ✅ Fait |
| Debugging toolbox – Écriture de fichier | Q7 – Q11 | ✅ Fait |
| Analyse du signal (f0, harmoniques) | Q12 – Q16 | ⏳ À faire |
| Synthèse additive | Q17 – Q18 | ⏳ À faire |
| Autotune | Q19 | ⏳ À faire |
| Fenêtres glissantes (buffer circulaire, overlap-add) | Q20 – Q22 | ⏳ À faire |

### Fonctionnalités actuelles
- Flux audio duplex en `double` (`RTAUDIO_FLOAT64`).
- Données partagées entre `main()` et le callback `inout()` via une structure `UserData`.
- Entrée au choix : **fichier binaire** (lu en boucle) ou **micro** (interrupteur `USE_FILE`).
- Traitement : pour l'instant, simple recopie de l'entrée vers la sortie.
- Enregistrement de l'entrée et de la sortie dans des buffers de dump, écrits sur le disque à la fin de l'exécution, au format binaire (`double`).
- Script Python de visualisation : tracé d'un fichier, ou comparaison entrée/sortie, avec export `.wav` et `.png`.

---

## Prérequis

- Linux (machines de l'école)
- Environnement conda `rtaudio` (compilation) et `pytorch` (tracés Python)
- Archive `rtaudio-6.0.1.tar`

---

## Compilation

```bash
# 1. Extraire et compiler la bibliothèque RtAudio
tar -xf rtaudio-6.0.1.tar
cd rtaudio-6.0.1
conda activate rtaudio
./configure
make all

# 2. Remplacer duplex.cpp par celui du projet, puis compiler
cp <chemin_du_projet>/duplex.cpp tests/duplex.cpp
cd tests
make duplex
```

> En cas de problème avec `conda activate`, lancer `conda init` (ou `source ~/.bashrc` puis `conda init`), puis réessayer.

---

## Configuration (en haut de `duplex.cpp`)

| Paramètre | Rôle | Valeur par défaut |
|---|---|---|
| `USE_FILE` | `1` = entrée depuis un fichier, `0` = entrée micro | `1` |
| `FILENAME` | Fichier binaire d'entrée (`double` 64 bits, mono, sans en-tête) | `fichiers_audio_test/mon_fichier.bin` |
| `DUMP_SECONDS` | Durée enregistrée dans les buffers de dump (s) | `10` |
| `DUMP_IN_FILENAME` | Fichier de sortie : signal d'entrée | `files/mon_fichier_q8_in.bin` |
| `DUMP_OUT_FILENAME` | Fichier de sortie : signal de sortie | `files/mon_fichier_q11_out.bin` |

Les chemins sont **relatifs au dossier depuis lequel on lance `./duplex`** (normalement `tests/`). Il faut recompiler (`make duplex`) après chaque modification.

---

## Exécution

```bash
cd rtaudio-6.0.1/tests
mkdir -p files                 # dossier de sortie des enregistrements (à créer une fois)
./duplex 1 48000               # 1 canal, fs = 48 kHz
```

Arguments : `./duplex N fs [iDevice] [oDevice] [iChannelOffset] [oChannelOffset]`
- `N` : nombre de canaux. Les fichiers de test sont mono, donc `1`.
- `fs` : fréquence d'échantillonnage. Avec une entrée fichier, utiliser **celle du fichier** (vérifiable avec `soxi fichier.wav`).

**Quitter avec `Entrée`** (et non `Ctrl+C`) : c'est à ce moment que les enregistrements sont écrits dans `files/`. Le terminal affiche alors :
```
480000 échantillons écrits dans files/..._in.bin
480000 échantillons écrits dans files/..._out.bin
```

> ⚠️ Au premier lancement, garder le casque loin des oreilles le temps de vérifier le niveau sonore.

### Dépannage audio
- Périphérique non reconnu : `pulseaudio --kill` puis `pulseaudio --start`, et vérifier avec `./audioprobe`.
- Son saccadé (underruns) : utiliser `fs = 48000`, augmenter `bufferFrames` (1024) ou `options.numberOfBuffers` (8).

---

## Visualisation des enregistrements

Le script `python/plot_dump.py` lit les fichiers binaires, les trace en fonction du temps, et écrit les `.wav` et `.png` correspondants. Tous les réglages se font **en haut du script** :

```python
MODE = 2          # 1 = file1 seul, 2 = comparaison file1 / file2
file1 = '../rtaudio-6.0.1/tests/files/test_01_in.bin'
file2 = '../rtaudio-6.0.1/tests/files/test_01_out.bin'
fs = 48000        # même valeur que pour ./duplex
label1, label2, titre, ylabel = ...
ECRIRE_WAV = True # False pour un signal non audio (ex : f0)
```

```bash
conda activate pytorch
python plot_dump.py
```

En `MODE = 2`, l'écart maximal `|file1 - file2|` est affiché dans le terminal. Tant que le traitement est une simple recopie, il vaut `0.000e+00`.

---

## Organisation des fichiers

```
.
├── README.md
├── duplex.cpp          # programme principal (à copier dans rtaudio-6.0.1/tests/)
├── python/
│   └── plot_dump.py    # visualisation des enregistrements
└── files/              # enregistrements (.bin, .wav) et figures (.png)
```

Convention de nommage des enregistrements : `<fichier_origine>_q<numéro>_<type>.bin` (ex : `test_01_q11_out.bin`).

---

## Structure du code (`duplex.cpp`)

| Élément | Rôle |
|---|---|
| `UserData` | Structure passée au callback : paramètres du flux, fichier d'entrée et tête de lecture, buffers de travail `in`/`out`, buffers de dump |
| `loadBinaryFile()` | Charge un fichier binaire de `double` en mémoire (Q4) |
| `write_buff_dump()` | Ajoute un buffer à la suite dans un buffer de dump (Q8) |
| `writeBinaryFile()` | Écrit un buffer de dump sur le disque (Q9) |
| `inout()` | Callback temps réel : remplissage de `in` (fichier en boucle ou micro), traitement `in → out`, envoi vers la carte son, enregistrement dans les dumps |
| `main()` | Chargement du fichier, allocations, ouverture/démarrage du flux, attente de `Entrée`, arrêt, écriture des dumps, libération de la mémoire |

Principe suivi : toutes les opérations lentes ou non déterministes (accès disque, allocations) sont faites dans `main()`, avant le démarrage ou après l'arrêt du flux. Le callback `inout()` ne fait que des copies et des calculs sur des données déjà en mémoire.

---

*Auteurs : [Nom 1], [Nom 2] – Binôme n° [X]*
