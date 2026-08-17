from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


class CreateManifestTests(unittest.TestCase):
    def test_cli_writes_hash_labels_and_input_contract(self) -> None:
        tool = Path(__file__).parents[1] / "create_manifest.py"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            model = root / "model.onnx"
            labels = root / "labels.txt"
            output = root / "manifest.json"
            model.write_bytes(b"small-test-model")
            labels.write_text("person\n\nforklift\n", encoding="utf-8")

            subprocess.run(
                [
                    sys.executable, str(tool),
                    "--model", str(model),
                    "--model-id", "test-detector",
                    "--version", "1.2.3",
                    "--labels", str(labels),
                    "--input-name", "images",
                    "--input-shape", "1,3,320,320",
                    "--output", str(output),
                ],
                check=True,
            )

            manifest = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(manifest["modelId"], "test-detector")
            self.assertEqual(manifest["labels"], ["person", "forklift"])
            self.assertEqual(manifest["input"]["shape"], [1, 3, 320, 320])
            self.assertEqual(manifest["artifact"]["sha256"], hashlib.sha256(model.read_bytes()).hexdigest())


if __name__ == "__main__":
    unittest.main()
