"""Single-variable R13 reply probes; fixed prompts, no key/body/transcript logs."""
import argparse
import datetime as dt
import json
import pathlib
import re
import time
import urllib.error
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]
BASE = "https://generativelanguage.googleapis.com/v1beta/models/"
MODEL = "gemini-3.8-flash"


def payload(variant):
    body = {"contents": [{"parts": [{"text": "Reply with OK only."}]}],
            "generationConfig": {"maxOutputTokens": 128, "thinkingConfig": {"thinkingLevel": "low"}}}
    if variant == "no_thinking":
        del body["generationConfig"]["thinkingConfig"]
    elif variant == "more_output":
        body["generationConfig"]["maxOutputTokens"] = 2048
    elif variant == "json":
        body["generationConfig"]["responseMimeType"] = "application/json"
    elif variant == "production":
        from tools.validate_r13_api import text_payload, REPLY_INSTRUCTION, QUESTION
        body = text_payload(QUESTION, 2048, system=REPLY_INSTRUCTION)
    return body


def run(variant, timeout):
    key = json.loads((ROOT / "firmware/data/config.json").read_text(encoding="utf-8-sig"))["gemini_api_key"]
    body = json.dumps(payload(variant), ensure_ascii=False, separators=(",", ":")).encode()
    req = urllib.request.Request(BASE + MODEL + ":generateContent", data=body,
                                 headers={"x-goog-api-key": key, "Content-Type": "application/json"})
    result = {"client_date": "2026-10-25", "variant": variant, "model": MODEL, "request_bytes": len(body),
              "http_status": None, "ok": False, "timeout_seconds": timeout, "proxy_configured": bool(urllib.request.getproxies())}
    started = time.monotonic()
    try:
        with urllib.request.urlopen(req, timeout=timeout) as response:
            result.update(http_status=response.status, content_type=response.headers.get_content_type())
            data = response.read(16001)
            if len(data) > 16000:
                raise ValueError("oversize")
            parsed = json.loads(data)
            candidate = (parsed.get("candidates") or [{}])[0]
            text = "".join(p.get("text", "") for p in candidate.get("content", {}).get("parts", []) if not p.get("thought"))
            result.update(finish_reason=candidate.get("finishReason"), nonempty_text=bool(text.strip()), text_bytes=len(text.encode()))
            result["ok"] = candidate.get("finishReason") == "STOP" and bool(text.strip())
            result["usage"] = {k: v for k, v in parsed.get("usageMetadata", {}).items() if isinstance(v, int) and k.endswith("TokenCount")}
            if variant in ("production", "json"):
                try:
                    structured = json.loads(text)
                    result["structured_json"] = True
                    if variant == "production":
                        answer = structured.get("text", "")
                        result["expected_answer_five"] = bool(re.search(r"(?<!\d)[5５](?!\d)|五", answer))
                        result["answer_characters"] = len(answer)
                        result["ok"] = result["ok"] and isinstance(answer, str) and bool(answer.strip())
                except Exception:
                    result["structured_json"] = False
                    result["ok"] = False
    except urllib.error.HTTPError as error:
        result["http_status"] = error.code
        try:
            data = json.loads(error.read(32000)).get("error", {})
            status = data.get("status", "")
            result["api_status"] = status if isinstance(status, str) and re.fullmatch("[A-Z_]+", status) else "UNKNOWN"
            message = str(data.get("message", "")).lower()
            result["message_classification"] = {tag: any(term in message for term in terms) for tag, terms in {
                "overload": ["overload", "high demand", "capacity"], "temporary": ["temporar", "try again"],
                "quota": ["quota", "rate limit", "resource exhaust"], "account": ["account", "billing", "credit"],
                "unsupported": ["not supported", "not found", "invalid argument"], "region": ["region", "location"]}.items()}
            result["reason_codes"] = [d["reason"] for d in data.get("details", []) if isinstance(d.get("reason"), str) and re.fullmatch("[A-Z_]+", d["reason"])]
        except Exception:
            result["api_status"] = "UNPARSED_ERROR"
        retry = error.headers.get("Retry-After", "")
        result["retry_after_seconds"] = int(retry) if retry.isdigit() else None
        result["retry_header_allows_automatic_retry"] = not retry or retry.isdigit()
        result["server_google_frontend"] = "google" in error.headers.get("Server", "").lower() or error.headers.get("Server", "").lower() == "esf"
    except Exception as error:
        result["error_kind"] = type(error).__name__
    result["elapsed_seconds"] = round(time.monotonic() - started, 3)
    stamp = dt.datetime.now(dt.timezone(dt.timedelta(hours=9))).strftime("%Y%m%d-%H%M%S-%f")
    directory = ROOT / "docs/validation/reply-503-2026-10-25"
    directory.mkdir(parents=True, exist_ok=True)
    path = directory / (stamp + "-" + variant + ".json")
    path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result), flush=True)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("variant", choices=["baseline", "no_thinking", "more_output", "json", "production"])
    parser.add_argument("--timeout", type=float, default=20)
    parser.add_argument("--retry-on-503", action="store_true", help="At most one retry, matching the reply-only firmware policy")
    args = parser.parse_args()
    # Permit importing the existing fixed-fixture request builder, not a network SDK.
    import sys
    sys.path.insert(0, str(ROOT))
    try:
        started = time.monotonic()
        first = run(args.variant, args.timeout)
        result = first
        if args.retry_on_503 and first.get("http_status") == 503 and first.get("retry_header_allows_automatic_retry", False):
            # Use a deterministic 250ms jitter for an auditable live comparison.
            wait = max(2, first.get("retry_after_seconds") or 0) + .250
            elapsed = time.monotonic() - started
            if wait <= 4 and elapsed + wait + 2 <= 15:
                print(json.dumps({"retry": 1, "wait_seconds": wait, "same_model_and_payload": True}), flush=True)
                time.sleep(wait)
                result = run(args.variant, min(args.timeout, max(2, 15 - (time.monotonic() - started))))
        raise SystemExit(0 if result["ok"] else 1)
    except Exception as error:
        print(json.dumps({"error_kind": type(error).__name__}))
        raise SystemExit(2)
