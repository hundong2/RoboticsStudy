#!/usr/bin/env bash
# Build and benchmark a target-specific FP16 TensorRT engine from an ONNX model.
# Run this on the deployment Jetson so the engine matches its TensorRT/GPU versions.
set -euo pipefail

# Arguments are kept separate from trtexec options to avoid accidental shell expansion.
onnx_path=""
engine_path=""
input_name=""
fixed_shape=""
min_shape=""
opt_shape=""
max_shape=""
trtexec_bin="${TRTEXEC_BIN:-trtexec}"

usage() {
  # Print the two supported modes: one fixed shape or a dynamic min/opt/max profile.
  echo "Usage: $0 --onnx MODEL --engine ENGINE --input NAME [--shape 1x3x640x640 | --min-shape ... --opt-shape ... --max-shape ...]"
}

# Parse `--name value` pairs. Unknown options fail instead of being forwarded silently.
while [[ $# -gt 0 ]]; do
  case "$1" in
    --onnx) onnx_path="$2"; shift 2 ;;
    --engine) engine_path="$2"; shift 2 ;;
    --input) input_name="$2"; shift 2 ;;
    --shape) fixed_shape="$2"; shift 2 ;;
    --min-shape) min_shape="$2"; shift 2 ;;
    --opt-shape) opt_shape="$2"; shift 2 ;;
    --max-shape) max_shape="$2"; shift 2 ;;
    --trtexec) trtexec_bin="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

if [[ -z "$onnx_path" || -z "$engine_path" || -z "$input_name" ]]; then
  usage >&2
  exit 2
fi
if [[ ! -f "$onnx_path" ]]; then
  echo "ONNX model not found: $onnx_path" >&2
  exit 2
fi
if ! command -v "$trtexec_bin" >/dev/null 2>&1; then
  echo "trtexec not found. Set TRTEXEC_BIN or pass --trtexec /usr/src/tensorrt/bin/trtexec" >&2
  exit 2
fi

mkdir -p "$(dirname "$engine_path")"
build_shape_args=()
run_shape_args=()
# Dynamic profiles are builder options; benchmarking uses the most common opt shape.
if [[ -n "$fixed_shape" ]]; then
  build_shape_args+=("--shapes=${input_name}:${fixed_shape}")
  run_shape_args+=("--shapes=${input_name}:${fixed_shape}")
elif [[ -n "$min_shape" && -n "$opt_shape" && -n "$max_shape" ]]; then
  build_shape_args+=("--minShapes=${input_name}:${min_shape}")
  build_shape_args+=("--optShapes=${input_name}:${opt_shape}")
  build_shape_args+=("--maxShapes=${input_name}:${max_shape}")
  run_shape_args+=("--shapes=${input_name}:${opt_shape}")
else
  echo "Provide --shape or all of --min-shape, --opt-shape, --max-shape" >&2
  exit 2
fi

# First pass parses ONNX and serializes the optimized engine without benchmarking.
"$trtexec_bin" \
  "--onnx=$onnx_path" \
  "--saveEngine=$engine_path" \
  --fp16 \
  --skipInference \
  "${build_shape_args[@]}"

# Second pass loads exactly that engine, warms it up, and measures steady-state latency.
"$trtexec_bin" \
  "--loadEngine=$engine_path" \
  --warmUp=1000 \
  --duration=10 \
  "${run_shape_args[@]}"

echo "TensorRT engine ready: $engine_path"
