#!/usr/bin/env python3
"""Validate an ONNX graph and print its deployment-relevant metadata."""

from __future__ import annotations

import argparse
from pathlib import Path

import onnx
from onnx import TensorProto, checker, shape_inference


def tensor_shape(value: onnx.ValueInfoProto) -> str:
    tensor_type = value.type.tensor_type
    dimensions: list[str] = []
    for dimension in tensor_type.shape.dim:
        if dimension.HasField("dim_value"):
            dimensions.append(str(dimension.dim_value))
        elif dimension.HasField("dim_param"):
            dimensions.append(dimension.dim_param)
        else:
            dimensions.append("?")
    return "x".join(dimensions)


def describe(kind: str, values: list[onnx.ValueInfoProto]) -> None:
    for value in values:
        element_type = TensorProto.DataType.Name(value.type.tensor_type.elem_type)
        print(f"{kind}: name={value.name} shape={tensor_shape(value)} type={element_type}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("model", type=Path)
    parser.add_argument("--save-inferred", type=Path, help="Write a shape-inferred ONNX copy")
    args = parser.parse_args()

    model = onnx.load(args.model, load_external_data=True)
    checker.check_model(model)
    inferred = shape_inference.infer_shapes(model)

    print(f"model={args.model}")
    print("opsets=" + ",".join(f"{item.domain or 'ai.onnx'}:{item.version}" for item in model.opset_import))
    print(f"nodes={len(model.graph.node)} initializers={len(model.graph.initializer)}")
    initializer_names = {item.name for item in inferred.graph.initializer}
    describe("input", [item for item in inferred.graph.input if item.name not in initializer_names])
    describe("output", list(inferred.graph.output))
    print("checker=ok")

    if args.save_inferred:
        args.save_inferred.parent.mkdir(parents=True, exist_ok=True)
        onnx.save(inferred, args.save_inferred)
        print(f"shape_inferred_model={args.save_inferred}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

