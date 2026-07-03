"""Tiny DNN forward/inference demo with only NumPy.

Run:
    uv run python play/dnn_forward_inference_demo.py
"""

from __future__ import annotations

import numpy as np


def relu(x: np.ndarray) -> np.ndarray:
    return np.maximum(x, 0.0)


def softmax(x: np.ndarray) -> np.ndarray:
    shifted = x - np.max(x)
    exp = np.exp(shifted)
    return exp / exp.sum()


def main() -> None:
    # A toy "image feature" vector: [edge, corner, brightness, texture].
    x = np.array([0.8, 0.2, 0.6, 0.4], dtype=np.float32)

    w1 = np.array(
        [
            [0.4, -0.2, 0.1, 0.7, -0.3, 0.2],
            [0.1, 0.5, -0.4, 0.2, 0.6, -0.1],
            [0.3, 0.2, 0.5, -0.2, 0.1, 0.4],
            [-0.2, 0.1, 0.3, 0.4, 0.2, 0.5],
        ],
        dtype=np.float32,
    )
    b1 = np.array([0.05, -0.05, 0.02, 0.0, 0.03, -0.02], dtype=np.float32)
    w2 = np.array(
        [
            [0.3, -0.4, 0.2],
            [0.1, 0.5, -0.2],
            [-0.3, 0.2, 0.4],
            [0.6, -0.1, 0.1],
            [-0.2, 0.4, 0.3],
            [0.2, 0.1, 0.5],
        ],
        dtype=np.float32,
    )
    b2 = np.array([0.01, -0.02, 0.03], dtype=np.float32)

    hidden = relu(x @ w1 + b1)
    logits = hidden @ w2 + b2
    probabilities = softmax(logits)

    labels = np.array(["background", "part", "tool"])
    predicted_index = int(np.argmax(probabilities))

    print("input:", x)
    print("hidden activation:", np.round(hidden, 3))
    print("logits:", np.round(logits, 3))
    readable_probabilities = {
        str(label): float(round(probability, 3))
        for label, probability in zip(labels, probabilities)
    }
    print("probabilities:", readable_probabilities)
    print("prediction:", labels[predicted_index])


if __name__ == "__main__":
    main()
