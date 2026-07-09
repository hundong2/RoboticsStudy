from __future__ import annotations

import ast
import json
from pathlib import Path


BASE = Path(__file__).resolve().parents[1]


def main() -> None:
    notebooks = sorted(BASE.rglob("*.ipynb"))
    errors: list[tuple[str, int, int | None, str]] = []

    for path in notebooks:
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except Exception as exc:
            errors.append((path.as_posix(), -1, None, f"invalid json: {exc}"))
            continue

        if data.get("nbformat") != 4 or not data.get("cells"):
            errors.append((path.as_posix(), -1, None, "missing nbformat=4 or cells"))
            continue

        for idx, cell in enumerate(data["cells"]):
            if cell.get("cell_type") != "code":
                continue
            source = "".join(cell.get("source", []))
            try:
                ast.parse(source)
            except SyntaxError as exc:
                errors.append((path.as_posix(), idx, exc.lineno, exc.msg))

    if errors:
        for path, cell, line, msg in errors:
            print(f"{path}: cell={cell}, line={line}, error={msg}")
        raise SystemExit(1)

    print(f"validated {len(notebooks)} notebooks")


if __name__ == "__main__":
    main()
