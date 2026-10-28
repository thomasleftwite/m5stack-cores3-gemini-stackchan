"""Guarded UI application-only update. Never writes boot metadata, NVS or FAT."""
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
PACKAGES = pathlib.Path.home() / '.platformio/packages'
FRAMEWORK = PACKAGES / 'framework-arduinoespressif32@3.20014.231204'
BUILD = ROOT / 'firmware/.pio/build/m5stack-cores3'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_mutually_exclusive_group(required=True)
    modes.add_argument('--inspect', action='store_true')
    modes.add_argument('--execute', action='store_true')
    parser.add_argument('--port', default='COM10')
    parser.add_argument('--sha256', required=True)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location('ui_app_partition_guard', FRAMEWORK / 'tools/gen_esp32part.py')
    partition = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(partition)
    table_bytes = (BUILD / 'partitions.bin').read_bytes()
    table = partition.PartitionTable.from_binary(table_bytes)
    table.verify(); table.verify_size_fits(16 * 1024 * 1024)
    selected = partition.PartitionTable.from_csv((ROOT / 'firmware/partitions/cores3_16mb_app3m_fat9m.csv').read_text())
    if selected.to_binary() != table_bytes:
        raise ValueError('UnapprovedLayout')
    app = table.find_by_name('app0')
    if (app.offset, app.size) != (0x10000, 0x300000):
        raise ValueError('UnexpectedAppRange')
    firmware = (BUILD / 'firmware.bin').read_bytes()
    digest = hashlib.sha256(firmware).hexdigest()
    revision = next((v for v in ('UI04', 'UI03', 'UI02', 'UI01') if ('[UI] revision=' + v).encode() in firmware), None)
    if digest != args.sha256.lower() or revision is None:
        raise ValueError('WrongApplication')
    if not firmware or ((len(firmware) + 4095) // 4096) * 4096 > app.size:
        raise ValueError('ApplicationTooLarge')
    boot_app0 = (FRAMEWORK / 'tools/partitions/boot_app0.bin').read_bytes()
    if len(boot_app0) != 0x2000:
        raise ValueError('UnexpectedBootSelectionSize')
    report = {'scope': 'UI app0 only; bootloader, table, boot selection, NVS and FAT preserved',
              'ui_revision': revision,
              'diagnostic': next((v for v in ('S08', 'S07', 'S06', 'S05', 'S04', 'S03', 'S02', 'S01', 'E06', 'E05', 'E04', 'E03', 'E02', 'E01', 'M04', 'M03', 'M02', 'M01', 'D13', 'D12', 'D11', 'D10', 'D9', 'D8', 'D7', 'D6', 'D5', 'D4', 'D3', 'D2', 'D1') if ('[DIAG] revision=' + v).encode() in firmware), None),
              'port': args.port, 'firmware_sha256': digest, 'firmware_bytes': len(firmware),
              'write_offset': '0x10000', 'written': False, 'hash_verified': False}
    if args.inspect:
        print(json.dumps(report)); return 0
    # Stage exactly the reviewed bytes so a later build cannot change this update.
    stage = ROOT / 'firmware/.pio/app-only'
    stage.mkdir(parents=True, exist_ok=True)
    image = stage / (digest + '.bin')
    image.write_bytes(firmware)
    stamp = dt.datetime.now(dt.timezone(dt.timedelta(hours=9))).strftime('%Y%m%d-%H%M%S')
    directory = ROOT / 'docs/validation' / ('r13-ui-app-' + stamp)
    directory.mkdir(parents=True, exist_ok=False)
    path = directory / 'report.json'

    def save():
        path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

    sys.path.insert(0, str(PACKAGES / 'tool-esptoolpy@1.40501.0'))
    import esptool

    def quiet(call):
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            return call()

    def connect():
        esp = quiet(lambda: esptool.detect_chip(args.port, baud=115200, connect_mode='default_reset'))
        try:
            if esp.CHIP_NAME != 'ESP32-S3':
                raise ValueError('WrongChip')
            esp = quiet(esp.run_stub)
            quiet(lambda: esp.flash_spi_attach(0))
            if (quiet(esp.flash_id) >> 16) & 0xff != 0x18:
                raise ValueError('WrongFlashSize')
            return esp
        except BaseException:
            esp._port.close(); raise

    # Hashes of private ranges remain in RAM only; persist equality booleans only.
    preserved_ranges = {'bootloader': (0, 0x8000), 'nvs': (0x9000, 0x5000),
                        'boot_selection': (0xe000, 0x2000), 'fat': (0x610000, 0x9e0000)}
    try:
        esp = connect()
        try:
            if quiet(lambda: esp.read_flash(0x8000, 0xc00)) != table_bytes:
                raise ValueError('DeviceLayoutMismatch')
            if quiet(lambda: esp.read_flash(0xe000, 0x2000)) != boot_app0:
                raise ValueError('App0BootSelectionNotConfirmed')
            before = {name: quiet(lambda o=o, n=n: esp.flash_md5sum(o, n)) for name, (o, n) in preserved_ranges.items()}
        finally:
            esp._port.close()
        report.update({'device_layout_match': True, 'app0_boot_selection_confirmed': True})
        save(); print(json.dumps({'stage': 'preflight_passed', 'write_offset': '0x10000', 'images': 1}), flush=True)
        output = io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
            esptool.main(['--chip', 'esp32s3', '--port', args.port, '--baud', '460800',
                          '--before', 'default_reset', '--after', 'no_reset', 'write_flash', '-z',
                          '--flash_mode', 'keep', '--flash_freq', 'keep', '--flash_size', '16MB',
                          '0x10000', str(image)])
        report['written'] = True
        report['hash_verified'] = output.getvalue().count('Hash of data verified.') == 1
        if not report['hash_verified']:
            raise ValueError('ApplicationHashNotVerified')
        save(); print(json.dumps({'stage': 'application_written', 'hash_verified': True}), flush=True)
        esp = connect()
        try:
            report['post_layout_match'] = quiet(lambda: esp.read_flash(0x8000, 0xc00)) == table_bytes
            report['app_bytes_verified'] = quiet(lambda: esp.flash_md5sum(app.offset, len(firmware))) == hashlib.md5(firmware).hexdigest()
            report['preserved'] = {name: quiet(lambda o=o, n=n: esp.flash_md5sum(o, n)) == before[name] for name, (o, n) in preserved_ranges.items()}
            if not report['post_layout_match'] or not report['app_bytes_verified'] or not all(report['preserved'].values()):
                raise ValueError('PostWriteMismatch')
            quiet(esp.hard_reset)
        finally:
            esp._port.close()
        save(); print(json.dumps({'stage': 'complete', 'report': str(path), **report}), flush=True)
        return 0
    except Exception as error:
        report['error_kind'] = type(error).__name__
        if isinstance(error, ValueError):
            report['guard'] = str(error)
        save(); print(json.dumps({'stage': 'failed', 'report': str(path), **report}), flush=True)
        return 1


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except Exception:
        print(json.dumps({'error_kind': 'AppOnlySetupError'})); raise SystemExit(2)
