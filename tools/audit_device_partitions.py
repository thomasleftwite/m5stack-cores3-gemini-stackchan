"""Offline audit of public partition tables only; no device, FS image or config access."""
import argparse
import hashlib
import importlib.util
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
FRAMEWORK = pathlib.Path.home() / ".platformio/packages/framework-arduinoespressif32@3.20014.231204"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device-table", type=pathlib.Path, default=ROOT / ".pio/r13-device/partition-table.bin")
    parser.add_argument("--build-table", type=pathlib.Path, default=ROOT / "firmware/.pio/build/m5stack-cores3/partitions.bin")
    parser.add_argument("--framework-dir", type=pathlib.Path, default=FRAMEWORK)
    parser.add_argument("--candidate-csv", type=pathlib.Path, help="Design candidate only; does not change the active build")
    parser.add_argument("--output", type=pathlib.Path)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location("esp_partition_audit", args.framework_dir / "tools/gen_esp32part.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    def parse(file):
        data = file.read_bytes()
        if len(data) != 0xC00 or not any(data[i:i+16] == b"\xeb\xeb" + b"\xff"*14 for i in range(0, len(data), 32)):
            raise ValueError("InvalidTableLengthOrMissingChecksum")
        table = module.PartitionTable.from_binary(data)
        table.verify()
        table.verify_size_fits(0x1000000)
        if table.to_binary() != data:
            raise ValueError("NoncanonicalTable")
        return data, table

    device_bytes, device = parse(args.device_table)
    build_bytes, build = parse(args.build_table)

    def rows(table):
        return [{"name": p.name, "type": p.type, "subtype": p.subtype,
                 "offset": p.offset, "size": p.size, "end_exclusive": p.offset+p.size}
                for p in table]

    def metadata(data, table):
        return {"sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data),
                "md5_verified": True, "geometry_verified": True, "entries": rows(table)}

    references = []
    for name in ("app3M_fat9M_16MB.csv", "default_16MB.csv"):
        csv = args.framework_dir / "tools/partitions" / name
        reference = module.PartitionTable.from_csv(csv.read_text())
        reference.verify()
        encoded = reference.to_binary()
        references.append({"csv": name, "device_binary_match": device_bytes == encoded,
                           "build_binary_match": build_bytes == encoded})

    device_data = [p for p in device if p.type == 1 and p.subtype in (0x81, 0x82)]
    hazards = []
    for p in build:
        if not (p.type == 0 or (p.type == 1 and p.subtype in (0x81, 0x82))):
            continue
        for d in device_data:
            start, end = max(p.offset, d.offset), min(p.offset+p.size, d.offset+d.size)
            if start < end:
                hazards.append({"build_partition": p.name, "device_data_partition": d.name,
                                "overlap_start": start, "overlap_end_exclusive": end,
                                "overlap_bytes": end-start,
                                "meaning": "potential write span; not evidence of a past write"})
    image = ROOT / "firmware/.pio/build/m5stack-cores3/firmware.bin"
    size = image.stat().st_size if image.exists() else None
    app0 = device.find_by_name("app0")
    report = {
        "client_date": "2026-10-25", "scope": "offline comparison of previously captured public tables",
        "device_access": False, "user_data_read": False, "firmware_or_configuration_changed": False,
        "framework": "Arduino-ESP32 2.0.14 / package 3.20014.231204",
        "device": metadata(device_bytes, device), "build": metadata(build_bytes, build),
        "references": references, "layout_match": device_bytes == build_bytes,
        "device_has_default_littlefs_label": any(p.name == "spiffs" and p.type == 1 for p in device),
        "filesystem_contents_verified": False,
        "firmware_bytes": size, "device_app0_capacity": app0.size if app0 else None,
        "firmware_fits_device_app0": size <= app0.size if size is not None and app0 else None,
        "potential_layout_overlaps": hazards,
    }
    if args.candidate_csv:
        candidate = module.PartitionTable.from_csv(args.candidate_csv.read_text(encoding="utf-8-sig"))
        candidate.verify()
        candidate.verify_size_fits(0x1000000)
        candidate_bytes = candidate.to_binary()
        report["candidate"] = {"csv": str(args.candidate_csv), "device_binary_match": candidate_bytes == device_bytes,
                               "sha256": hashlib.sha256(candidate_bytes).hexdigest(), "entries": rows(candidate)}
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"layout_match": report["layout_match"], "md5_verified": True,
                      "references": references, "firmware_fits_device_app0": report["firmware_fits_device_app0"],
                      "candidate_device_match": report.get("candidate", {}).get("device_binary_match"),
                      "output": str(args.output) if args.output else None}))
    return 0 if report["layout_match"] else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception:
        # Keep parsing failures from accidentally echoing arbitrary input bytes.
        print(json.dumps({"error_kind": "PartitionAuditInputError"}))
        sys.exit(2)
