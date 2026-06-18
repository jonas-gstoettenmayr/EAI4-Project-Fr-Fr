# Getting once the data for the multihead and once for the 7 class part.

import os
import numpy as np
from PIL import Image

# helpers


def _iter_bmps(folder: str):
    """Yield (filepath,) for every .bmp in *folder* (non-recursive)."""
    for fname in sorted(os.listdir(folder)):
        if fname.lower().endswith(".bmp"):
            yield os.path.join(folder, fname)


CONDITION_NAMES = {0: "without", 1: "with"}
GESTURE_NAMES   = {0: "rock",  1: "paper", 2: "sissors", 3: "gay"}



# multi-head loader

def load_multiclasses_paths(root: str):
    """
    Returns file paths and labels instead of loaded images.
    Images are loaded lazily during training.
    """
    paths, cond_labels, gest_labels = [], [], []

    for c_idx, condition in CONDITION_NAMES.items():
        for g_idx, gesture in GESTURE_NAMES.items():
            folder = os.path.join(root, condition, gesture)
            if not os.path.isdir(folder):
                print(f"[WARNING] folder not found, skipping: {folder}")
                continue
            for fpath in _iter_bmps(folder):
                paths.append(fpath)
                cond_labels.append(c_idx)
                gest_labels.append(g_idx)

    return (
        np.array(paths),
        np.array(cond_labels, dtype=np.uint8),
        np.array(gest_labels, dtype=np.uint8),
    )


# label file writer

def write_label_files(out_dir: str = "."):
    """
    Write human-readable label mapping files so you always know what is what.

    Creates
    labels_condition.txt -> index -> condition name  (multihead)
    labels_gesture.txt -> index -> gesture name (multihead)
    """
    os.makedirs(out_dir, exist_ok=True)

    # condition
    path = os.path.join(out_dir, "labels_condition.txt")
    with open(path, "w") as f:
        f.write("# Condition label mapping (load_multiview -> y_condition)\n")
        f.write("# index : name\n")
        for idx in sorted(CONDITION_NAMES):
            f.write(f"{idx} : {CONDITION_NAMES[idx]}\n")
    print(f"Written -> {path}")

    # gesture
    path = os.path.join(out_dir, "labels_gesture.txt")
    with open(path, "w") as f:
        f.write("# Gesture label mapping (load_multiview -> y_gesture)\n")
        f.write("# index : name\n")
        for idx in sorted(GESTURE_NAMES):
            f.write(f"{idx} : {GESTURE_NAMES[idx]}\n")
    print(f"Written -> {path}")