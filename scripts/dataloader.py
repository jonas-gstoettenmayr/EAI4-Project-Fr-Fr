# Getting once the data for the multihead and once for the 7 class part.

import os
import numpy as np
from PIL import Image

# helpers

def _load_bmp(path: str) -> np.ndarray:
    """Load a .bmp file and return a uint8 array normalised to [0, 1]."""
    img = Image.open(path)
    return np.array(img, dtype=np.uint8)


def _iter_bmps(folder: str):
    """Yield (filepath,) for every .bmp in *folder* (non-recursive)."""
    for fname in sorted(os.listdir(folder)):
        if fname.lower().endswith(".bmp"):
            yield os.path.join(folder, fname)


CONDITION_NAMES = {0: "without", 1: "with"}
GESTURE_NAMES   = {0: "rock",  1: "paper", 2: "sissors", 3: "gay"}

# 7-class loader

# Map (condition, gesture) to the class index
CLASS_MAP_7 = {
    ("without", "rock"):    0,
    ("without", "paper"):   1,
    ("without", "sissors"): 2,
    ("with",    "rock"):    3,
    ("with",    "paper"):   4,
    ("with",    "sissors"): 5,
    ("without", "gay"):     6,
    ("with",    "gay"):     6,
}

CLASS_NAMES_7 = {
    0: "without_rock",
    1: "without_paper",
    2: "without_sissors",
    3: "with_rock",
    4: "with_paper",
    5: "with_sissors",
    6: "gay",
}


def load_7class(root: str):
    """
    Load all .bmp images and return (X, y) for a 7-class problem.
    """
    images, labels = [], []

    conditions = CONDITION_NAMES
    gestures   = GESTURE_NAMES

    for condition in conditions:
        for gesture in gestures:
            folder = os.path.join(root, condition, gesture)
            if not os.path.isdir(folder):
                print(f"[WARNING] folder not found, skipping: {folder}")
                continue

            class_idx = CLASS_MAP_7[(condition, gesture)]

            for fpath in _iter_bmps(folder):
                images.append(_load_bmp(fpath))
                labels.append(class_idx)

    X = np.stack(images, axis=0)
    y = np.array(labels, dtype=np.int64)
    return X, y


# multi-head loader



def load_multiclasses_paths(root: str):
    """
    Returns file paths and labels instead of loaded images.
    Images are loaded lazily by tf.data during training.
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
    labels_7class.txt -> index -> class name for the 7-class loader
    labels_condition.txt -> index -> condition name  (multihead)
    labels_gesture.txt -> index -> gesture name (multihead)
    """
    os.makedirs(out_dir, exist_ok=True)

    # 7-class
    path = os.path.join(out_dir, "labels_7class.txt")
    with open(path, "w") as f:
        f.write("# 7-class label mapping (load_7class)\n")
        f.write("# index : name\n")
        for idx in sorted(CLASS_NAMES_7):
            f.write(f"{idx} : {CLASS_NAMES_7[idx]}\n")
    print(f"Written -> {path}")

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


# test

if __name__ == "__main__":
    import sys

    root = os.path.join("Data_collection", sys.argv[1] if len(sys.argv) > 1 else "augmented")
    print(root)

    # write label files next to this script
    write_label_files(out_dir=os.path.dirname(os.path.abspath(__file__)))

    print("\n7-class loader")
    X7, y7 = load_7class(root)
    print(f"X shape : {X7.shape}")
    print(f"y shape : {y7.shape}")
    for idx, name in sorted(CLASS_NAMES_7.items()):
        print(f"  class {idx} ({name:15s}) : {(y7 == idx).sum()} samples")