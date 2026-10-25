"""Compare fixed nonpersonal R13 reply requests. No billing/config/device changes."""
import datetime as dt
import argparse
import json
import pathlib
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.validate_r13_api import request, text_payload, text_result, expected_five, REPLY_INSTRUCTION, QUESTION

MODELS = ["gemini-3.5-flash-lite", "gemini-3.1-flash-lite", "gemini-3.8-flash", "gemini-2.5-flash", "gemini-2.5-flash-lite"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--models", nargs="+", choices=MODELS, default=MODELS)
    args = parser.parse_args()
    key = json.loads((ROOT / "firmware/data/config.json").read_text(encoding="utf-8-sig"))["gemini_api_key"]
    stamp = dt.datetime.now(dt.timezone(dt.timedelta(hours=9))).strftime("%Y%m%d-%H%M%S")
    directory = ROOT / "docs/validation" / ("model-comparison-" + stamp)
    directory.mkdir(parents=True, exist_ok=False)
    listing, meta = request(key, "models?pageSize=1000", timeout=15)
    available = {m.get("name", "").removeprefix("models/"): m for m in (listing or {}).get("models", [])}
    report = {"client_date": "2026-10-25", "scope": "fixed Japanese question and structured reply; no actual user audio", "list_http_status": meta["http_status"], "generation_config": {"maxOutputTokens": 2048, "responseMimeType": "application/json"}, "thinking": "model default; omitted to keep one common request across generations", "results": []}
    if not meta["ok"]:
        report["error"] = "MODEL_LIST_UNAVAILABLE"
    else:
        body = text_payload(QUESTION, 2048, system=REPLY_INSTRUCTION)
        del body["generationConfig"]["thinkingConfig"]
        for model in args.models:
            row = {"model": model, "listed": model in available, "production_setting_changed": False}
            if "generateContent" not in available.get(model, {}).get("supportedGenerationMethods", []):
                row.update(tested=False, ok=False, reason="not_listed_for_generation")
            else:
                response, result = request(key, "models/" + model + ":generateContent", body, cap=16000, timeout=15)
                row.update(result, tested=True)
                parsed = text_result(response, row)
                answer = parsed.get("text", "") if isinstance(parsed, dict) else ""
                emotion = parsed.get("emotion", "") if isinstance(parsed, dict) else ""
                answer = answer.strip() if isinstance(answer, str) else ""
                row["answer_characters"] = len(answer)
                row["expected_answer_five"] = expected_five(answer)
                row["answer_judge"] = "kana-aware-v2"
                row["emotion_valid"] = emotion in ("NORMAL", "HAPPY", "SAD", "ANGRY", "SURPRISED", "THINKING")
                row["protocol_ok"] = bool(row["ok"] and answer and len(answer) <= 60 and row["emotion_valid"])
                row["ok"] = bool(row["protocol_ok"] and row["expected_answer_five"])
            report["results"].append(row)
            (directory / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
            print(json.dumps(row), flush=True)
            time.sleep(1)
    (directory / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"report": str(directory / "report.json")}), flush=True)
    return 0 if any(r.get("ok") for r in report["results"]) else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(json.dumps({"error_kind": type(error).__name__})); raise SystemExit(2)
