"""Bounded R13 API checks. Reads the ignored local config; never logs keys/bodies.

--inspect lists only model availability. --run sends a fixed synthetic Japanese
question through TTS -> 16kHz STT -> reply -> TTS (at most four generation calls).
No microphone, device, automatic retry, or model fallback is used.
"""
import argparse
import array
import base64
import datetime as dt
import hashlib
import io
import json
import pathlib
import struct
import sys
import time
import urllib.error
import urllib.request
import wave

ROOT = pathlib.Path(__file__).resolve().parents[1]
HOST = "https://generativelanguage.googleapis.com/v1beta/"
MODELS = {"stt": "gemini-3.1-flash-lite", "reply": "gemini-3.8-flash", "tts": "gemini-3.8-flash-lite-tts"}
QUESTION = "二足す三はいくつですか。"
STT_INSTRUCTION = '音声を忠実に日本語で文字起こししてください。質問への回答、要約、推測による補完は禁止。無音はsilence、認識不能はunclear。JSONのみ: {"status":"ok|silence|unclear","transcript":"認識文"}'
REPLY_INSTRUCTION = 'あなたはスタックチャン。親しみやすい日本語1～2文、60文字以内で答える。検索機能はないため最新情報を作らない。JSONのみ返す: {"emotion":"NORMAL|HAPPY|SAD|ANGRY|SURPRISED|THINKING","text":"読み上げ本文"}。タグや絵文字や箇条書きを本文に含めない。'


def request(key, route, payload=None, cap=1400000):
    started = time.monotonic()
    encoded = None if payload is None else json.dumps(payload, ensure_ascii=False, separators=(",", ":")).encode()
    req = urllib.request.Request(HOST + route, data=encoded, headers={"x-goog-api-key": key, "Content-Type": "application/json"})
    meta = {"request_bytes": len(encoded or b""), "http_status": None, "ok": False}
    try:
        with urllib.request.urlopen(req, timeout=45) as response:
            data = response.read(cap + 1)
            meta.update(http_status=response.status, content_type=response.headers.get_content_type(), response_bytes=len(data))
            expected = response.headers.get("Content-Length")
            if len(data) > cap or (expected and int(expected) != len(data)):
                meta["error_kind"] = "incomplete_or_oversize_response"
                return None, meta
            if meta["content_type"] != "application/json":
                meta["error_kind"] = "unexpected_content_type"
                return None, meta
            parsed = json.loads(data)
            meta["ok"] = True
            usage = parsed.get("usageMetadata", {})
            meta["usage"] = {k: v for k, v in usage.items() if isinstance(v, int) and k.endswith("TokenCount")}
            return parsed, meta
    except urllib.error.HTTPError as error:
        meta["http_status"] = error.code
        try:
            body = json.loads(error.read(32000)).get("error", {})
            status = body.get("status", "")
            meta["api_status"] = status if isinstance(status, str) and status.replace("_", "").isalnum() else "UNKNOWN"
            reasons = [d.get("reason", "") for d in body.get("details", [])]
            meta["reason_codes"] = [r for r in reasons if isinstance(r, str) and r.replace("_", "").isalnum()]
            # Record only quota metric identifiers/limits, never project/account data.
            quotas = []
            for detail in body.get("details", []):
                for item in detail.get("violations", []):
                    quotas.append({k: item[k] for k in ("quotaMetric", "quotaId", "quotaValue") if k in item})
            if quotas:
                meta["quota"] = quotas
        except Exception:
            meta["api_status"] = "UNPARSED_ERROR"
        meta["error_kind"] = "http_error"
        return None, meta
    except Exception as error:
        meta["error_kind"] = type(error).__name__
        return None, meta
    finally:
        meta["elapsed_seconds"] = round(time.monotonic() - started, 3)


def text_payload(text, tokens, system=None, audio=None):
    parts = []
    if audio:
        parts.append({"inlineData": {"mimeType": "audio/wav", "data": base64.b64encode(audio).decode()}})
    parts.append({"text": text})
    payload = {"contents": [{"role": "user", "parts": parts}], "generationConfig": {"responseMimeType": "application/json", "maxOutputTokens": tokens, "thinkingConfig": {"thinkingLevel": "low"}}}
    if system:
        payload["systemInstruction"] = {"parts": [{"text": system}]}
    return payload


def tts_payload(text, voice):
    return {"contents": [{"parts": [{"text": text}]}], "generationConfig": {"responseModalities": ["AUDIO"], "speechConfig": {"voiceConfig": {"prebuiltVoiceConfig": {"voiceName": voice}}}}}


def text_result(response, meta):
    if not response:
        return None
    candidate = (response.get("candidates") or [{}])[0]
    meta["finish_reason"] = candidate.get("finishReason")
    if meta["finish_reason"] != "STOP":
        meta.update(ok=False, error_kind="unfinished_candidate")
        return None
    text = "".join(p.get("text", "") for p in candidate.get("content", {}).get("parts", []) if not p.get("thought"))
    meta["text_bytes"] = len(text.encode())
    try:
        return json.loads(text)
    except Exception:
        meta.update(ok=False, error_kind="invalid_structured_text")
        return None


def wav_result(response, meta):
    if not response:
        return None
    candidate = (response.get("candidates") or [{}])[0]
    meta["finish_reason"] = candidate.get("finishReason")
    parts = [p["inlineData"] for p in candidate.get("content", {}).get("parts", []) if not p.get("thought") and "inlineData" in p]
    if meta["finish_reason"] != "STOP" or len(parts) != 1:
        meta.update(ok=False, error_kind="incomplete_audio_candidate")
        return None
    meta["audio_mime"] = parts[0].get("mimeType")
    try:
        data = base64.b64decode(parts[0]["data"], validate=True)
        if meta["audio_mime"] != "audio/wav" or data[:4] != b"RIFF" or struct.unpack_from("<I", data, 4)[0] + 8 != len(data):
            raise ValueError()
        with wave.open(io.BytesIO(data)) as wav:
            meta.update(audio_bytes=len(data), sample_rate=wav.getframerate(), channels=wav.getnchannels(), sample_width=wav.getsampwidth(), duration_seconds=round(wav.getnframes() / wav.getframerate(), 3))
            if wav.getframerate() != 24000 or wav.getnchannels() != 1 or wav.getsampwidth() != 2 or not 0 < wav.getnframes() <= 480000 or wav.getcomptype() != "NONE":
                raise ValueError()
        meta["sha256"] = hashlib.sha256(data).hexdigest()
        return data
    except Exception:
        meta.update(ok=False, error_kind="invalid_or_unsupported_wav")
        return None


def to_input_wav(data):
    with wave.open(io.BytesIO(data)) as wav:
        samples = array.array("h", wav.readframes(wav.getnframes()))
    if sys.byteorder != "little":
        samples.byteswap()
    resampled = array.array("h")
    for i in range(len(samples) * 2 // 3):
        at = i * 1.5
        a = int(at)
        resampled.append(round(samples[a] * (1 - (at-a)) + samples[min(a+1, len(samples)-1)] * (at-a)))
    if sys.byteorder != "little":
        resampled.byteswap()
    output = io.BytesIO()
    with wave.open(output, "wb") as wav:
        wav.setnchannels(1); wav.setsampwidth(2); wav.setframerate(16000); wav.writeframes(resampled.tobytes())
    return output.getvalue()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inspect", action="store_true")
    parser.add_argument("--run", action="store_true")
    parser.add_argument("--resume", type=pathlib.Path, help="Previous report from this tool; reuse its synthetic fixture for one bounded retry")
    args = parser.parse_args()
    if args.inspect == args.run:
        parser.error("Choose exactly one of --inspect or --run")
    config = json.loads((ROOT / "firmware/data/config.json").read_text(encoding="utf-8-sig"))
    key = config.get("gemini_api_key", "")
    if not key or "YOUR_" in key:
        print(json.dumps({"result": "LOCAL_KEY_UNAVAILABLE"})); return 2
    if args.inspect:
        response, meta = request(key, "models?pageSize=1000")
        names = {m.get("name", "").removeprefix("models/"): m for m in (response or {}).get("models", [])}
        print(json.dumps({"operation": "inspect_models", "transport": meta, "models": {role: {"id": model, "listed": model in names if meta["ok"] else None, "generate_content": "generateContent" in names.get(model, {}).get("supportedGenerationMethods", []) if meta["ok"] else None} for role, model in MODELS.items()}, "next_page": bool((response or {}).get("nextPageToken"))}))
        return 0 if meta["ok"] else 1
    stamp = dt.datetime.now(dt.timezone(dt.timedelta(hours=9))).strftime("%Y%m%d-%H%M%S")
    output = ROOT / "docs/validation" / ("r13-api-" + stamp)
    output.mkdir(parents=True, exist_ok=False)
    report = {"client_date": "2026-10-25", "scope": "host REST contract using fixed synthetic Japanese speech; no device/microphone", "models": MODELS, "stages": []}
    started = time.monotonic()
    if args.resume:
        source_report = args.resume.resolve()
        source_report.relative_to((ROOT / "docs/validation").resolve())
        previous = json.loads(source_report.read_text(encoding="utf-8"))
        meta = next(s.copy() for s in previous["stages"] if s["stage"] == "fixture_tts" and s["ok"])
        fixture = (source_report.parent / "synthetic-question-24k.wav").read_bytes()
        if previous["scope"] != report["scope"] or hashlib.sha256(fixture).hexdigest() != meta["sha256"]:
            raise ValueError("Fixture provenance mismatch")
        meta.update(input_source="verified_previous_synthetic_fixture", elapsed_seconds=0)
        report["source_report"] = str(source_report)
    else:
        response, meta = request(key, "models/" + MODELS["tts"] + ":generateContent", tts_payload(QUESTION, config.get("tts_voice", "Kore")))
        meta.update(stage="fixture_tts", input_source="fixed_public_test_phrase")
        fixture = wav_result(response, meta)
    report["stages"].append(meta); print(json.dumps(meta), flush=True)
    recognized = ""
    if fixture:
        (output / "synthetic-question-24k.wav").write_bytes(fixture)
        speech = to_input_wav(fixture); (output / "synthetic-question-16k.wav").write_bytes(speech)
        response, meta = request(key, "models/" + MODELS["stt"] + ":generateContent", text_payload(STT_INSTRUCTION, 1024, audio=speech), cap=16000)
        meta.update(stage="stt", input_source="synthetic_question_16k")
        result = text_result(response, meta)
        if result:
            recognized = result.get("transcript", "").strip() if result.get("status") == "ok" else ""
        meta["ok"] = bool(meta["ok"] and recognized)
        meta["transcript_bytes"] = len(recognized.encode())
        normalized = recognized.translate(str.maketrans("２３二三", "2323"))
        meta["expected_question_match"] = "2" in normalized and "3" in normalized
        report["stages"].append(meta); print(json.dumps(meta), flush=True)
    response, meta = request(key, "models/" + MODELS["reply"] + ":generateContent", text_payload(recognized or QUESTION, 2048, system=REPLY_INSTRUCTION), cap=16000)
    meta.update(stage="reply", input_source="stt_result" if recognized else "fixed_fixture_independent_test")
    result = text_result(response, meta)
    answer = result.get("text", "").strip()[:60] if result else ""
    meta["ok"] = bool(meta["ok"] and answer)
    meta["answer_characters"] = len(answer)
    meta["expected_answer_five"] = bool(answer and any(s in answer for s in ("5", "５", "五", "ご")))
    report["stages"].append(meta); print(json.dumps(meta), flush=True)
    if answer:
        response, meta = request(key, "models/" + MODELS["tts"] + ":generateContent", tts_payload(answer, config.get("tts_voice", "Kore")))
        meta.update(stage="reply_tts", input_source="generated_reply")
        audio = wav_result(response, meta)
        if audio:
            (output / "synthetic-reply.wav").write_bytes(audio)
        report["stages"].append(meta); print(json.dumps(meta), flush=True)
    report["total_seconds"] = round(time.monotonic() - started, 3)
    report["pipeline_pass"] = len(report["stages"]) == 4 and all(s["ok"] for s in report["stages"])
    (output / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"pipeline_pass": report["pipeline_pass"], "report": str(output / "report.json")}), flush=True)
    return 0 if report["pipeline_pass"] else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(json.dumps({"result": "LOCAL_TEST_ERROR", "kind": type(error).__name__}))
        raise SystemExit(2)
