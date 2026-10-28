"""Observe allowlisted R13 boot state only; never print raw serial lines."""
import argparse
import datetime as dt
import json
import pathlib
import re
import sys
import time
import serial

ROOT = pathlib.Path(__file__).resolve().parents[1]


def reset_flags(report):
    for key in ("storage_mounted", "config_loaded", "key_configured", "wifi_connected", "wifi_portal", "ready"):
        report[key] = False
    report.pop("wake_registered", None)
    report.pop("version", None)


def consume(report, line):
    # Keep evidence from one boot only. A later reset invalidates earlier success.
    if line.startswith("ESP-ROM:") or "Rebooting..." in line or re.search(r"\brst:0x[0-9a-f]+", line, re.I):
        if report.get("boot_count", 0):
            report["unexpected_reset"] = True; report["fault"] = True; reset_flags(report)
    if any(s in line.lower() for s in ("guru meditation", "brownout detector", "watchdog", "wdt timeout", "abort() was called")):
        report["fault"] = True
    version = re.search(r"\[System\] CoreS3 file conversation (R13\.\d+)", line)
    if version:
        count = report.get("boot_count", 0) + 1
        if count > 1: report["unexpected_reset"] = True; report["fault"] = True
        reset_flags(report); report["boot_count"] = count; report["version"] = version.group(1)
    if "[Storage] backend=FFat mounted=1" in line: report["storage_mounted"] = True
    if "[Storage] backend=FFat mounted=0" in line: report["fault"] = True
    if "[Config] Local configuration loaded." in line: report["config_loaded"] = True
    if "[Config] key_configured=1" in line: report["key_configured"] = True
    if "[WiFi] Connected!" in line: report["wifi_connected"] = True
    if "[WiFi] Setup AP active." in line: report["wifi_portal"] = True
    wake = re.search(r"\[Wake\] registered=([01])", line)
    if wake: report["wake_registered"] = wake.group(1) == "1"
    if "[System] ready=1" in line: report["ready"] = True


def passed(report, expected_version="R13.3"):
    return all(report.get(k, False) for k in ("storage_mounted", "config_loaded", "key_configured", "wifi_connected", "ready")) and not report.get("fault", False) and report.get("version") == expected_version and report.get("boot_count") == 1


def event_kind(line):
    if line.startswith("ESP-ROM:"): return "rom_start"
    reset = re.search(r"\brst:(0x[0-9a-f]+)", line, re.I)
    if reset: return "reset_reason_" + reset.group(1).lower()
    if "[System] CoreS3 file conversation" in line: return "application_start"
    if "[Storage] backend=FFat mounted=1" in line: return "fat_mounted"
    if "[Config] Local configuration loaded." in line: return "config_loaded"
    if "[WiFi] Connected!" in line: return "wifi_connected"
    if "[System] ready=1" in line: return "ready"
    for keyword, kind in (("Guru Meditation", "panic"), ("Brownout detector", "brownout"),
                          ("watchdog", "watchdog"), ("wdt timeout", "watchdog"),
                          ("abort() was called", "abort"), ("Rebooting...", "rebooting")):
        if keyword.lower() in line.lower(): return kind
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM10")
    parser.add_argument("--seconds", type=float, default=45)
    parser.add_argument("--reset", action="store_true")
    args = parser.parse_args()
    report = {"scope":"device allowlisted boot state; no microphone test", "port":args.port,
              "storage_mounted":False, "config_loaded":False, "key_configured":False,
              "wifi_connected":False, "wifi_portal":False, "ready":False, "fault":False,
              "boot_count":0, "unexpected_reset":False, "events":[]}
    port = serial.Serial(); port.port=args.port; port.baudrate=115200; port.timeout=0.25; port.dtr=False; port.rts=False
    started = time.monotonic(); buffer=""
    try:
        port.open()
        port.reset_input_buffer()
        if args.reset:
            sys.path.insert(0,str(pathlib.Path.home()/".platformio/packages/tool-esptoolpy@1.40501.0"))
            from esptool.reset import HardReset
            HardReset(port)()
        while time.monotonic()-started < args.seconds:
            buffer += port.read(4096).decode("utf-8",errors="replace")
            while "\n" in buffer:
                line,buffer=buffer.split("\n",1)
                kind = event_kind(line)
                if kind and len(report["events"]) < 100:
                    report["events"].append({"seconds":round(time.monotonic()-started,3), "kind":kind})
                consume(report, line)
            if len(buffer)>8192: buffer=""
        report["elapsed_seconds"]=round(time.monotonic()-started,3)
    except serial.SerialException:
        report["error_kind"]="SerialUnavailable"
    finally:
        port.close()
    report["boot_pass"] = passed(report) and "error_kind" not in report
    stamp=dt.datetime.now(dt.timezone(dt.timedelta(hours=9))).strftime("%Y%m%d-%H%M%S")
    directory=ROOT/"docs/validation"/("r13-boot-"+stamp);directory.mkdir(parents=True,exist_ok=False)
    file=directory/"report.json";file.write_text(json.dumps(report,indent=2)+"\n")
    print(json.dumps({**report,"report":str(file)}))
    return 0 if report["boot_pass"] else 1


if __name__=="__main__":
    try: sys.exit(main())
    except Exception:
        print(json.dumps({"error_kind":"BootObservationError"}));sys.exit(2)
