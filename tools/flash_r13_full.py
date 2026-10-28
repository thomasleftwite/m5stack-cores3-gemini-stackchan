"""Guarded CoreS3 full firmware/FAT initialization. Private images stay ignored."""
import argparse
import contextlib
import datetime as dt
import hashlib
import importlib.util
import io
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
PACKAGES = pathlib.Path.home() / ".platformio/packages"
FRAMEWORK = PACKAGES / "framework-arduinoespressif32@3.20014.231204"
BUILD = ROOT / "firmware/.pio/build/m5stack-cores3"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_mutually_exclusive_group(required=True)
    modes.add_argument("--inspect", action="store_true")
    modes.add_argument("--preflight", action="store_true")
    modes.add_argument("--execute", action="store_true", help="Erase/reinitialize FAT and write all firmware images; preserves NVS")
    parser.add_argument("--port", default="COM10")
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location("partition_flash_guard", FRAMEWORK / "tools/gen_esp32part.py")
    partition = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(partition)
    table_bytes = (BUILD / "partitions.bin").read_bytes()
    table = partition.PartitionTable.from_binary(table_bytes)
    table.verify(); table.verify_size_fits(16*1024*1024)
    selected = partition.PartitionTable.from_csv((ROOT / "firmware/partitions/cores3_16mb_app3m_fat9m.csv").read_text())
    if selected.to_binary() != table_bytes: raise ValueError("UnapprovedLayout")
    fat = table.find_by_name("ffat")
    app = table.find_by_name("app0")
    if (fat.offset, fat.size, app.offset, app.size) != (0x610000, 0x9e0000, 0x10000, 0x300000):
        raise ValueError("UnexpectedLayout")
    images = [(0, BUILD/"bootloader.bin"), (0x8000, BUILD/"partitions.bin"),
              (0xe000, FRAMEWORK/"tools/partitions/boot_app0.bin"),
              (app.offset, BUILD/"firmware.bin"), (fat.offset+4096, BUILD/"fatfs.bin")]
    firmware = (BUILD/"firmware.bin").read_bytes()
    if not 0 < len(firmware) <= app.size or b"CoreS3 file conversation R13.3" not in firmware:
        raise ValueError("WrongFirmware")
    if (BUILD/"fatfs.bin").stat().st_size != fat.size-4096: raise ValueError("WrongFatImageSize")
    config = json.loads((ROOT/"firmware/data/config.json").read_text(encoding="utf-8-sig"))
    key = config.get("gemini_api_key")
    if not isinstance(key, str) or not key or "YOUR_" in key or key.encode() not in (BUILD/"fatfs.bin").read_bytes():
        raise ValueError("LocalConfigNotInImage")
    # Reject erase-sector overlap with Wi-Fi NVS and overlap between images.
    ends = [0x8000, 0x9000, 0x10000, app.offset+app.size, fat.offset+fat.size]
    for (offset, file), limit in zip(images, ends):
        if offset % 4096 or offset + ((file.stat().st_size+4095)//4096)*4096 > limit:
            raise ValueError("UnsafeImageRange")
    report = {"version":"R13.3", "scope":"full firmware plus authorized FAT initialization; NVS excluded",
              "port":args.port, "mode":"execute" if args.execute else "preflight" if args.preflight else "inspect",
              "firmware_bytes":len(firmware), "firmware_sha256":hashlib.sha256(firmware).hexdigest(),
              "images":[{"offset":hex(o), "bytes":p.stat().st_size, "private":p.name=="fatfs.bin"} for o,p in images],
              "fat_erase":{"offset":hex(fat.offset), "bytes":fat.size}, "written":False}
    if args.inspect:
        print(json.dumps(report)); return 0
    sys.path.insert(0, str(PACKAGES/"tool-esptoolpy@1.40501.0"))
    import esptool
    stamp = dt.datetime.now(dt.timezone(dt.timedelta(hours=9))).strftime("%Y%m%d-%H%M%S")
    directory = ROOT/"docs/validation"/("r13-fullflash-"+stamp)
    directory.mkdir(parents=True, exist_ok=False)

    def save():
        (directory/"report.json").write_text(json.dumps(report, indent=2)+"\n")

    def connect(before="default_reset"):
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            esp = esptool.detect_chip(args.port, baud=115200, connect_mode=before)
            try:
                if esp.CHIP_NAME != "ESP32-S3": raise ValueError("WrongChip")
                esp = esp.run_stub(); esp.flash_spi_attach(0)
                if (esp.flash_id() >> 16) & 0xff != 0x18: raise ValueError("WrongFlashSize")
                return esp
            except Exception:
                esp._port.close(); raise

    try:
        esp = connect()
        try:
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                actual = esp.read_flash(0x8000, 0xc00)
                nvs_before = esp.read_flash(0x9000, 0x5000)
            report["chip"] = "ESP32-S3"; report["flash_MiB"] = 16
            report["device_layout_match"] = actual == table_bytes
            if actual != table_bytes: raise ValueError("DeviceLayoutChanged")
        finally:
            esp._port.close()
        save(); print(json.dumps({"stage":"preflight", "chip":"ESP32-S3", "layout_match":True}), flush=True)
        if not args.execute: return 0
        def command(arguments):
            output = io.StringIO()
            with contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
                esptool.main(["--chip","esp32s3","--port",args.port,"--baud","460800",
                              "--before","default_reset","--after","no_reset", *arguments])
            return output.getvalue()
        print(json.dumps({"stage":"fat_initialization", "offset":hex(fat.offset), "bytes":fat.size}), flush=True)
        command(["erase_region",hex(fat.offset),hex(fat.size)])
        report["fat_erased"] = True; save()
        print(json.dumps({"stage":"writing_firmware_and_private_fat_image", "image_count":len(images)}), flush=True)
        text = command(["write_flash","-z","--flash_mode","keep","--flash_freq","keep","--flash_size","16MB",
                        *[item for offset, file in images for item in (hex(offset), str(file))]])
        report["verified_image_count"] = text.count("Hash of data verified.")
        if report["verified_image_count"] != len(images): raise ValueError("IncompleteVerification")
        report["written"] = True; save()
        esp = connect()
        try:
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                report["post_layout_match"] = esp.read_flash(0x8000,0xc00) == table_bytes
                report["nvs_unchanged"] = esp.read_flash(0x9000,0x5000) == nvs_before
                esp.hard_reset()
        finally:
            esp._port.close()
        if not report["post_layout_match"] or not report["nvs_unchanged"]: raise ValueError("PostWriteMismatch")
        save(); print(json.dumps({"stage":"complete", "verified_images":5, "nvs_unchanged":True,
                                 "post_layout_match":True, "report":str(directory/"report.json")}), flush=True)
        return 0
    except BaseException as error:
        report["error_kind"] = type(error).__name__
        save(); print(json.dumps({"stage":"failed", "error_kind":type(error).__name__, "report":str(directory/"report.json")}), flush=True)
        return 1


if __name__ == "__main__":
    try: sys.exit(main())
    except Exception:
        print(json.dumps({"error_kind":"LocalFlashSetupError"})); sys.exit(2)
