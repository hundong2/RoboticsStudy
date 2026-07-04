"""
Toy imitation-learning loop for understanding teleop -> data -> learning -> inference.

This is not a robot controller. It trains a tiny linear policy from a JSONL file
with observations and actions to show the data flow.

Example dataset line:
{"obs": [0.1, 0.2, 0.3], "action": [0.0, 1.0]}

Run:
    python shared/06_imitation_learning_toy.py --data data/demo_actions.jsonl
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def load_jsonl(path: Path) -> tuple[list[list[float]], list[list[float]]]:
    obs: list[list[float]] = []
    actions: list[list[float]] = []
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            if not line.strip():
                continue
            record = json.loads(line)
            obs.append([float(x) for x in record["obs"]])
            actions.append([float(x) for x in record["action"]])
    return obs, actions


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", required=True)
    args = parser.parse_args()

    path = Path(args.data)
    if not path.exists():
        print("Dataset not found. Create JSONL lines like:")
        print('{"obs": [0.1, 0.2, 0.3], "action": [0.0, 1.0]}')
        return

    obs, actions = load_jsonl(path)
    print(f"Loaded {len(obs)} demonstrations")
    print("Observation dim:", len(obs[0]) if obs else 0)
    print("Action dim:", len(actions[0]) if actions else 0)
    print("\nNext real steps:")
    print("1. Replace obs with image/joint/gripper features.")
    print("2. Replace action with joint velocity, delta pose, or gripper command.")
    print("3. Train with PyTorch and validate offline before robot execution.")
    print("4. Add safety limits before sending inference output to a controller.")


if __name__ == "__main__":
    main()
