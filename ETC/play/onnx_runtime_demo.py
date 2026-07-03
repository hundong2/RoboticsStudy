"""Create and run a tiny ONNX model with ONNX Runtime.

The model computes y = x @ W + b. It keeps the demo small while showing the
same load/session/run flow used by larger exported vision models.

Run:
    uv run python play/onnx_runtime_demo.py
"""

from __future__ import annotations

from pathlib import Path

import numpy as np
import onnx
import onnx.helper as oh
import onnx.numpy_helper as nh
import onnxruntime as ort


ROOT = Path(__file__).resolve().parent
MODEL_PATH = ROOT / "tiny_linear.onnx"


def build_model(path: Path) -> None:
    x = oh.make_tensor_value_info("x", onnx.TensorProto.FLOAT, [None, 4])
    y = oh.make_tensor_value_info("y", onnx.TensorProto.FLOAT, [None, 3])

    weight = np.array(
        [
            [0.2, -0.4, 0.7],
            [0.1, 0.8, -0.5],
            [0.6, 0.2, 0.3],
            [-0.3, 0.4, 0.9],
        ],
        dtype=np.float32,
    )
    bias = np.array([0.01, -0.02, 0.03], dtype=np.float32)

    nodes = [
        oh.make_node("MatMul", ["x", "weight"], ["matmul_out"]),
        oh.make_node("Add", ["matmul_out", "bias"], ["y"]),
    ]
    graph = oh.make_graph(
        nodes,
        "tiny_linear",
        [x],
        [y],
        initializer=[nh.from_array(weight, "weight"), nh.from_array(bias, "bias")],
    )
    model = oh.make_model(
        graph,
        producer_name="robotics-study",
        opset_imports=[oh.make_opsetid("", 17)],
    )
    model.ir_version = 10
    onnx.checker.check_model(model)
    onnx.save(model, path)


def main() -> None:
    build_model(MODEL_PATH)

    session = ort.InferenceSession(str(MODEL_PATH), providers=["CPUExecutionProvider"])
    sample = np.array([[0.8, 0.2, 0.6, 0.4]], dtype=np.float32)
    output = session.run(["y"], {"x": sample})[0]

    print("model:", MODEL_PATH)
    print("providers:", session.get_providers())
    print("input:", sample)
    print("output:", np.round(output, 3))


if __name__ == "__main__":
    main()
