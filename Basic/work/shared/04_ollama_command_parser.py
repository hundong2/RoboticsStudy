"""
Natural-language robot command parser using Ollama.

Run:
    ollama serve
    ollama pull llama3.1
    python shared/04_ollama_command_parser.py "빨간 컵을 집어 왼쪽 상자에 넣어줘"

The output is a constrained JSON action plan. Do not send LLM text directly to
motors. Validate the plan against allowed actions and current robot state.
"""

from __future__ import annotations

import argparse
import json
import urllib.error
import urllib.request


SYSTEM_PROMPT = """
You are a robot task parser. Convert the Korean user command into JSON only.
Allowed actions: detect_object, navigate_to, pick, place, ask_clarification, stop.
Schema:
{
  "intent": "short Korean intent",
  "objects": [{"name": "...", "attributes": ["..."]}],
  "plan": [{"action": "allowed_action", "target": "...", "notes": "..."}],
  "safety_checks": ["..."]
}
Do not invent motor commands. Use ask_clarification if the target is ambiguous.
"""


def call_ollama(model: str, command: str, host: str) -> str:
    payload = {
        "model": model,
        "prompt": SYSTEM_PROMPT + "\nUser command: " + command,
        "stream": False,
        "format": "json",
    }
    data = json.dumps(payload).encode("utf-8")
    request = urllib.request.Request(
        host.rstrip("/") + "/api/generate",
        data=data,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(request, timeout=60) as response:
        body = json.loads(response.read().decode("utf-8"))
    return body.get("response", "")


def fallback_plan(command: str) -> dict:
    return {
        "intent": "Ollama 미연결 상태의 예시 계획",
        "objects": [{"name": "unknown", "attributes": []}],
        "plan": [
            {"action": "detect_object", "target": "user_target", "notes": command},
            {"action": "ask_clarification", "target": "user", "notes": "Ollama 서버 연결 후 재시도"},
        ],
        "safety_checks": ["LLM 서버 연결 확인", "허용 action 목록 검증"],
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("command", nargs="+", help="Korean robot command")
    parser.add_argument("--model", default="llama3.1")
    parser.add_argument("--host", default="http://localhost:11434")
    args = parser.parse_args()

    command = " ".join(args.command)
    try:
        response = call_ollama(args.model, command, args.host)
        print(json.dumps(json.loads(response), ensure_ascii=False, indent=2))
    except (urllib.error.URLError, TimeoutError, json.JSONDecodeError) as exc:
        print("Ollama call failed:", exc)
        print(json.dumps(fallback_plan(command), ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
