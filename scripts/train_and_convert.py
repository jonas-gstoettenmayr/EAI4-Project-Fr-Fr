#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import os
import shutil
from pathlib import Path
from typing import Any
import sys
from sklearn.model_selection import train_test_split
from sklearn.metrics import confusion_matrix, ConfusionMatrixDisplay
import matplotlib.pyplot as plt

os.environ.setdefault("TF_USE_LEGACY_KERAS", "1")

import numpy as np
import tensorflow as tf
from dataloader import load_multiclasses_paths, CONDITION_NAMES, GESTURE_NAMES

tf.keras.utils.set_random_seed(3407)


def require_tfmot():
    try:
        import tf_keras  # noqa: F401
        from tensorflow_model_optimization.clustering import keras as clustering
    except ImportError as exc:
        raise RuntimeError(
            "TFMOT clustering requires compatible tensorflow, tf-keras, "
            "tensorflow-model-optimization, and numpy versions."
        ) from exc
    return clustering


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Train MNIST baseline/compression variants, one student KD model, one K-Means example, and one final INT8 student."
    )
    parser.add_argument("--artifacts-dir", default="artifacts")
    parser.add_argument("--epochs", type=int, default=200)
    parser.add_argument("--batch-size", type=int, default=64)
    parser.add_argument("--validation-split", type=float, default=0.20)
    parser.add_argument("--test-split", type=float, default=0.25)
    parser.add_argument("--representative-samples", type=int, default=256)
    parser.add_argument("--tflite-eval-samples", type=int, default=0)
    parser.add_argument("--early-stopping-patience", type=int, default=20)
    parser.add_argument("--lr-reduce-patience", type=int, default=4)
    parser.add_argument("--lr-reduce-factor", type=float, default=0.5)
    parser.add_argument("--min-lr", type=float, default=1e-7)
    return parser.parse_args()

# Plots

def save_training_history(history, filename):
    h = history.history
    train_keys = [k for k in h.keys() if not k.startswith("val_")]
    fig, axes = plt.subplots(1, 2, figsize=(12, 5))

    # Losses
    for k in train_keys:
        if "loss" in k:
            axes[0].plot(h[k], label=k)

            val_k = f"val_{k}"
            if val_k in h:
                axes[0].plot(h[val_k], "--", label=val_k)

    axes[0].set_title("Loss")
    axes[0].legend()

    # Accuracies
    for k in train_keys:
        if "accuracy" in k:
            axes[1].plot(h[k], label=k)

            val_k = f"val_{k}"
            if val_k in h:
                axes[1].plot(h[val_k], "--", label=val_k)

    axes[1].set_title("Accuracy")
    axes[1].legend()

    fig.tight_layout()
    fig.savefig(filename, dpi=300)
    plt.close(fig)


def save_confusion_matrices(model, dataset, artifacts_dir):

    all_inputs = []
    y_gesture_true = []
    y_condition_true = []

    for inputs, labels in dataset:          # single pass
        all_inputs.append(inputs.numpy())
        yg, yc = labels
        y_gesture_true.extend(yg.numpy())
        y_condition_true.extend(yc.numpy())
        # print(inputs.numpy())

    all_inputs = np.concatenate(all_inputs, axis=0)

    # predict on the same inputs whose labels you just collected
    gesture_probs, condition_probs = model.predict(all_inputs, verbose=0)

    y_gesture_pred = np.argmax(gesture_probs, axis=1)

    y_condition_pred = (
        condition_probs.reshape(-1) >= 0.5
    ).astype(np.float32)


    # print(type(labels))
    # print(labels)

    # print(yg.shape, yc.shape)

    gesture_names = [
        GESTURE_NAMES[i]
        for i in sorted(GESTURE_NAMES)
    ]

    condition_names = [
        CONDITION_NAMES[i]
        for i in sorted(CONDITION_NAMES)
    ]
    # print(gesture_probs)
    # print(np.unique(np.argmax(gesture_probs, axis=1), return_counts=True))
    # Gesture matrix
    cm = confusion_matrix(y_gesture_true, y_gesture_pred)

    fig, ax = plt.subplots(figsize=(8, 6))
    ConfusionMatrixDisplay(
        confusion_matrix=cm,
        display_labels=gesture_names
    ).plot(ax=ax)

    fig.savefig(
        artifacts_dir / "gesture_confusion_matrix.png",
        dpi=300
    )
    plt.close(fig)

    # Condition matrix
    cm = confusion_matrix(
        y_condition_true,
        y_condition_pred
    )

    fig, ax = plt.subplots(figsize=(6, 6))
    ConfusionMatrixDisplay(
        confusion_matrix=cm,
        display_labels=condition_names
    ).plot(ax=ax)

    fig.savefig(
        artifacts_dir / "condition_confusion_matrix.png",
        dpi=300
    )
    plt.close(fig)


# Data

def split_train_validation(x: np.ndarray, y: np.ndarray, validation_split: float):
    return train_test_split(x, y, test_size=validation_split, shuffle=True, random_state=42)

def load_and_preprocess(path, label_gesture, label_condition, size = 224):
    """Loads one image from disk"""
    raw   = tf.io.read_file(path)
    image = tf.image.decode_bmp(raw, channels=3)
    image = tf.cast(image, tf.uint8) #/ 255.0
    image = tf.image.resize(image, [size, size])
    return image, (label_gesture, label_condition)

def augment(image, labels):
    image = tf.image.convert_image_dtype(image, tf.float32)
    image = tf.image.random_flip_left_right(image)
    image = tf.image.random_brightness(image, max_delta=0.4)
    image = tf.clip_by_value(image, 0.0, 1.0)
    return image, labels

def make_dataset(paths, yg, yc, batch_size, shuffle=False, use_augment = False):
    ds = tf.data.Dataset.from_tensor_slices((paths, yg, yc))
    if shuffle:
        ds = ds.shuffle(buffer_size=len(paths), seed=42)
    ds = ds.map(load_and_preprocess, num_parallel_calls=tf.data.AUTOTUNE)
    if use_augment:
        ds = ds.map(augment, num_parallel_calls = tf.data.AUTOTUNE)
    ds = ds.batch(batch_size).prefetch(tf.data.AUTOTUNE)
    return ds


# Modeling


def make_model_mobilenet_multihead(img_size: int, num_classes1: int) -> tf.keras.Model:
    """
    Shared MobileNetV3Small backbone with two independent classification heads.
    Head 1 (primary): Dense(64) -> Dropout -> Dense(16) -> Dense(num_classes1)
    Head 2 (primary 2): Dense(2) -> Dense(1)
    Both heads are trained jointly.
    """
    backbone = tf.keras.applications.MobileNetV3Small(
        input_shape=(img_size, img_size, 3),
        include_top=False,
        weights="imagenet",
        pooling="avg",
        include_preprocessing=True, 
    )
    inputs = tf.keras.Input(shape=(img_size, img_size, 3), name="image")

    backbone.trainable = True
    x = backbone(inputs)

    
    # Head 1 — for rock paper sissors & gay
    h1 = tf.keras.layers.Dense(64, activation="relu", name="h1_fc1")(x)
    h1 = tf.keras.layers.Dropout(0.3, name="h1_drop")(h1)
    h1 = tf.keras.layers.Dense(16, activation="relu", name="h1_fc2")(h1)
    out1 = tf.keras.layers.Dense(num_classes1, activation="softmax", name="output_head1")(h1)

    # Head 2 — for acessory and no acessory
    h2 = tf.keras.layers.Dense(2, activation="relu", name="h2_fc1")(x)
    out2 = tf.keras.layers.Dense(1, activation="sigmoid", name="output_head2")(h2)

    model = tf.keras.Model(inputs, [out1, out2], name="mobilenet_multihead")
    model.compile(
        optimizer=tf.keras.optimizers.Adam(1e-3),
        loss=["sparse_categorical_crossentropy", "binary_crossentropy"],
        loss_weights=[1.0, 1.0], # both count the same
        metrics=["accuracy"],
    )
    return model

#Evaluation

def representative_dataset(x: np.ndarray, sample_count: int):
    limit = max(1, min(int(sample_count), len(x)))
    def generator():
        for i in range(limit):
            yield [x[i:i + 1].astype(np.float32)]
    return generator


def export_tflite(model: tf.keras.Model, artifacts_dir: Path, name: str, mode: str, representative_data: np.ndarray, representative_samples: int) -> Path:
    keras_dir = artifacts_dir / "keras"
    saved_dir = artifacts_dir / "saved_models" / name
    keras_dir.mkdir(parents=True, exist_ok=True)
    saved_dir.parent.mkdir(parents=True, exist_ok=True)
    if saved_dir.exists():
        shutil.rmtree(saved_dir)
    model.save(str(keras_dir / f"{name}.keras"))
    tf.saved_model.save(model, str(saved_dir))

    converter = tf.lite.TFLiteConverter.from_saved_model(str(saved_dir))
    if mode == "fp16":
        converter.optimizations = [tf.lite.Optimize.DEFAULT]
        converter.target_spec.supported_types = [tf.float16]
    elif mode == "int8":
        converter.optimizations = [tf.lite.Optimize.DEFAULT]
        converter.representative_dataset = representative_dataset(representative_data, representative_samples)
        converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
        converter.inference_input_type = tf.int8
        converter.inference_output_type = tf.int8

    path = artifacts_dir / f"{name}.tflite"
    path.write_bytes(converter.convert())
    return path


def quantize_for_tflite(values: np.ndarray, detail: dict[str, Any]) -> np.ndarray:
    dtype = detail["dtype"]
    if np.issubdtype(dtype, np.floating):
        return values.astype(dtype)
    scale, zero_point = detail["quantization"]
    scale = float(scale) if float(scale) else 1.0
    info = np.iinfo(dtype)
    return np.clip(np.round(values / scale + int(zero_point)), info.min, info.max).astype(dtype)


def dequantize_from_tflite(values: np.ndarray, detail: dict[str, Any]) -> np.ndarray:
    if not np.issubdtype(values.dtype, np.integer):
        return values.astype(np.float32)
    scale, zero_point = detail["quantization"]
    scale = float(scale) if float(scale) else 1.0
    return (values.astype(np.float32) - int(zero_point)) * scale


def evaluate_tflite(
    path: Path, x: np.ndarray, y_gesture: np.ndarray, y_condition: np.ndarray, sample_limit: int,) -> tuple[float, float]:
    interpreter = tf.lite.Interpreter(model_path=str(path))
    interpreter.allocate_tensors()

    input_detail = interpreter.get_input_details()[0]
    output_details = interpreter.get_output_details()  # all heads

    # Map output heads by tensor name so order doesn't matter
    head1_detail = next(d for d in output_details if "StatefulPartitionedCall:0" in d["name"])
    head2_detail = next(d for d in output_details if "StatefulPartitionedCall:1" in d["name"])

    limit = len(x) if sample_limit <= 0 else min(sample_limit, len(x))
    correct_gesture = 0
    correct_condition = 0

    for i in range(limit):
        sample = x[i:i + 1].astype(np.uint8)
        interpreter.set_tensor(input_detail["index"], quantize_for_tflite(sample, input_detail))
        interpreter.invoke()

        output_gesture = dequantize_from_tflite(
            interpreter.get_tensor(head1_detail["index"]), head1_detail)[0]
        output_condition = dequantize_from_tflite(
            interpreter.get_tensor(head2_detail["index"]), head2_detail)[0]

        # 4-class head
        pred_gesture = int(np.argmax(output_gesture))

        # Binary sigmoid head -> threshold at 0.5
        pred_condition = int(output_condition[0] >= 0.5)

        correct_gesture+= int(pred_gesture== int(y_gesture[i]))
        correct_condition += int(pred_condition == int(y_condition[i]))

    n = max(1, limit)
    return float(correct_gesture / n), float(correct_condition / n)


def parameter_count(model: tf.keras.Model) -> int:
    total = 0
    for layer in model.layers:
        for attr in ("kernel", "bias"):
            variable = getattr(layer, attr, None)
            if variable is not None:
                total += int(variable.numpy().size)
    return total


def evaluate_keras(model: tf.keras.Model, x_val: np.ndarray, y_val: np.ndarray, x_test: np.ndarray, y_test: np.ndarray) -> tuple[float, float]:
    val_acc = float(model.evaluate(x_val, y_val, verbose=0)[1]) if len(x_val) else float("nan")
    test_acc = float(model.evaluate(x_test, y_test, verbose=0)[1])
    return val_acc, test_acc



def append_model(models: list[dict[str, Any]], name: str, method: str, model: tf.keras.Model, mode: str = "fp32") -> None:
    models.append({"name": name, "method": method, "model": model, "mode": mode})


def paths_to_numpy(paths: np.ndarray, limit: int = 0) -> np.ndarray:
    """Load a set of image paths into a numpy array (use only for small sets)."""
    subset = paths[:limit] if limit > 0 else paths
    images = []
    for p in subset:
        img = tf.image.decode_bmp(tf.io.read_file(p), channels=3)
        img = tf.cast(img, tf.uint8) #/ 255.0
        img = tf.image.resize(img, [224, 224])
        images.append(img.numpy())
    return np.stack(images, axis=0)

import psutil


class MemoryCallback(tf.keras.callbacks.Callback):
    def on_epoch_end(self, epoch, logs=None):
        mem = psutil.Process(os.getpid()).memory_info().rss / 1024**3
        print(f"\nEpoch {epoch+1}: RAM = {mem:.2f} GB")

def main() -> int:
    args = parse_args()
    tf.keras.utils.set_random_seed(42)
    np.random.seed(42)

    artifacts_dir = Path(args.artifacts_dir)
    artifacts_dir.mkdir(parents=True, exist_ok=True)

    root = os.path.join("Data_collection", sys.argv[1] if len(sys.argv) > 1 else "processed")
    paths, yc, yg = load_multiclasses_paths(root)

    paths_train, paths_val_temp, yc_train, yc_val = split_train_validation(paths, yc, args.validation_split)
    _, _, yg_train, yg_val = split_train_validation(paths, yg, args.validation_split)
    paths_val, paths_test, yc_val, yc_test = split_train_validation(paths_val_temp, yc_val, args.test_split)
    _, _, yg_val, yg_test = split_train_validation(paths_val_temp, yg_val, args.test_split)


    # tf.data pipelines

    train_ds = make_dataset(paths_train, yg_train, yc_train, args.batch_size, shuffle=True, use_augment=False)
    val_ds = make_dataset(paths_val, yg_val, yc_val, args.batch_size)
    test_ds = make_dataset(paths_test, yg_test, yc_test, args.batch_size)


    # multihead 
    multihead = make_model_mobilenet_multihead(224, num_classes1=4)
    history = multihead.fit(
    train_ds,
    batch_size=args.batch_size,
    validation_data=val_ds,
    epochs=args.epochs,
    verbose=2,
    callbacks=[
        MemoryCallback(),
        tf.keras.callbacks.ReduceLROnPlateau(
            monitor="val_loss",
            factor=args.lr_reduce_factor,
            patience=args.lr_reduce_patience,
            min_lr=args.min_lr,
            verbose=1
        ),
        tf.keras.callbacks.EarlyStopping(
            monitor="val_loss",
            patience=args.early_stopping_patience,
            restore_best_weights=True,
            verbose=1
        ),
    ]
    )
    multihead.trainable = False
    save_training_history(history, artifacts_dir / "training_history.png")
    models: list[dict[str, Any]] = []

    append_model(models, "multihead", "normal FP32 training", multihead)
    append_model(models, "multihead-int8", "now in int8", multihead, "int8")
    

    rows: list[dict[str, Any]] = []
    for item in models:
        model = item["model"]
        mode = str(item["mode"])
        X_test_np = paths_to_numpy(paths_test)

        # Load a small representative sample for INT8 quantization
        X_rep_np = paths_to_numpy(paths_train, limit=args.representative_samples)

        # # evaluate_keras works with datasets directly
        results = dict(zip(model.metrics_names, model.evaluate(test_ds, verbose=0)))
        val_results = dict(zip(model.metrics_names, model.evaluate(val_ds, verbose=0)))
        save_confusion_matrices(model, test_ds, artifacts_dir)

        test_accg = results["output_head1_accuracy"]   # gesture
        test_accc = results["output_head2_accuracy"]   # condition
        val_accg  = val_results["output_head1_accuracy"]
        val_accc  = val_results["output_head2_accuracy"]

        # # export_tflite and evaluate_tflite still need numpy, use the small preloaded sets
        tflite_path  = export_tflite(model, artifacts_dir, str(item["name"]), mode,
                                    X_rep_np, args.representative_samples)
        tflite_accg, tflite_accc = evaluate_tflite(tflite_path, X_test_np, yg_test, yc_test, args.tflite_eval_samples)



        rows.append({
            "tflite_file": tflite_path.name,
            "method": str(item["method"]),
            "tflite_mode": mode,
            "validation_accuracy_g": f"{val_accg:.3f}",
            "test_accuracy_g": f"{test_accg:.3f}",
            "validation_accuracy_c": f"{val_accc:.3f}",
            "test_accuracy_c": f"{test_accc:.3f}",
            "tflite_test_accuracy_g": f"{tflite_accg:.3f}",
            "tflite_test_accuracy_c": f"{tflite_accc:.3f}",
            "tflite_file_size_kilobytes": int(tflite_path.stat().st_size / 1024),
            "parameters_total": parameter_count(model),
        })
        print(f"Exported {tflite_path} keras_acc={(test_accc*0.5)+(test_accg*0.5):.4f} tflite_acc={(tflite_accc*0.5)+(tflite_accg*0.5):.4f}")

    shutil.copy2(artifacts_dir / "multihead.tflite", artifacts_dir / "model.tflite")
    metrics_path = artifacts_dir / "model_metrics.csv"
    with metrics_path.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

     # makes one for each head

    print(f"Saved model metrics: {metrics_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
