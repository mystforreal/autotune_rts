import numpy as np
from matplotlib import pyplot as plt
from scipy.io.wavfile import write

# =====================================================================
# ---------------------- PARAMETRES A MODIFIER ------------------------
# =====================================================================

# Mode d'affichage :
#   1 = un seul fichier (file1)
#   2 = comparaison file1 (entree) / file2 (sortie) sur un meme graphique
MODE = 2

# Fichiers a lire (file2 n'est utilise qu'en MODE 2)
file1 = '../rtaudio-6.0.1/tests/files/test_01.bin'
file2 = '../rtaudio-6.0.1/tests/files/test_01_out.bin'

# Frequence d'echantillonnage : LA MEME que celle passee a ./duplex
fs = 48000

# Textes de la figure
label1 = 'Entrée'
label2 = 'Sortie'
titre  = 'Signal enregistré : test_01'
ylabel = 'Amplitude'

# =====================================================================


def charger(nom):
    """Lit un fichier binaire de doubles et ecrit le .wav correspondant."""
    x = np.fromfile(nom, dtype=np.double)
    print(f"{nom} : {len(x)} echantillons, soit {len(x) / fs:.2f} s")
    write(nom.replace('.bin', '.wav'), fs, x.astype(np.float32))
    return x


# ---- Lecture ---------------------------------------------------------
x1 = charger(file1)
t1 = np.arange(len(x1)) / fs
if MODE == 2:
    x2 = charger(file2)
    t2 = np.arange(len(x2)) / fs

# ---- Trace -----------------------------------------------------------
plt.figure(figsize=(10, 4))

if MODE == 1:
    # Un seul fichier
    plt.plot(t1, x1, linewidth=0.8, label=label1)
    nom_png = file1.replace('.bin', '.png')

else:
    # Comparaison : entree en trait plein, sortie en pointilles par-dessus
    plt.plot(t1, x1, linewidth=1.5, color='tab:blue', label=label1)
    plt.plot(t2, x2, linewidth=1.0, color='tab:orange', label=label2)

    # Echelle verticale commune et symetrique
    ymax = 1.1 * max(np.max(np.abs(x1)), np.max(np.abs(x2)))
    if ymax > 0:
        plt.ylim(-ymax, ymax)

    nom_png = file2.replace('.bin', '_compare.png')

plt.xlabel('Temps (s)')
plt.ylabel(ylabel)
plt.title(titre)
plt.legend(loc='upper right')
plt.grid()
plt.tight_layout()
plt.savefig(nom_png, dpi=150)
plt.show()

