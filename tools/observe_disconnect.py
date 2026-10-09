"""Monitor request/disconnect/reset markers, reconnecting without reset or logging secrets."""
import argparse, json, re, time
import serial
from serial.tools import list_ports
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--seconds', type=float, default=60)
a = p.parse_args()
s = None
buf = ''
attempts = 0
replies = 0
fault = False
disconnect = False
end = time.monotonic() + a.seconds
try:
    while time.monotonic() < end:
        if s is None:
            matches = [x for x in list_ports.comports() if x.vid == 0x303a and x.pid == 0x1001]
            if len(matches) != 1:
                time.sleep(0.25)
                continue
            s = serial.Serial()
            s.port, s.baudrate, s.timeout = matches[0].device, 115200, 0.25
            s.dtr = s.rts = False
            try:
                s.open()
                print('SERIAL_ATTACHED', flush=True)
            except serial.SerialException:
                s = None
                time.sleep(0.25)
                continue
        try:
            buf += s.read(4096).decode('utf-8', errors='replace')
        except (serial.SerialException, OSError):
            print('USB_DISCONNECTED', flush=True)
            disconnect = True
            try: s.close()
            except Exception: pass
            s = None
            buf = ''
            continue
        while '\n' in buf:
            line, buf = buf.split('\n', 1)
            if '[BootDiag]' in line:
                safe = re.findall(r'(?:reset|phase|stack|heap|largest|vbus_mv|battery_mv|request)=[A-Z0-9_]+', line)
                print('BOOT_DIAG ' + ' '.join(safe), flush=True)
            elif 'Brownout detector' in line:
                fault = True
                print('BROWNOUT', flush=True)
            elif 'Guru Meditation Error' in line or 'Stack canary' in line or 'stack overflow' in line:
                fault = True
                print('CPU_PANIC_OR_STACK_FAULT', flush=True)
            elif 'watchdog' in line.lower():
                fault = True
                print('WATCHDOG_MESSAGE', flush=True)
            elif '[VAD] End of user speech' in line:
                attempts += 1
                print('GEMINI_REQUEST_STARTED', flush=True)
            elif '[Gemini] HTTP status=' in line:
                match = re.search(r'HTTP status=(-?\d+)', line)
                if match: print('HTTP_STATUS=' + match.group(1), flush=True)
            elif '[Gemini] Stream complete. Text bytes=' in line:
                replies += 1
                print('GEMINI_REPLY_COMPLETED', flush=True)
            elif '[Gemini] Stream failed.' in line:
                print('GEMINI_REQUEST_FAILED', flush=True)
        if len(buf) > 8192: buf = ''
finally:
    if s is not None: s.close()
result = 'FAULT_OR_DISCONNECT' if fault or disconnect else 'REPLY_COMPLETED' if replies else 'NO_COMPLETED_REPLY'
print(json.dumps({'result': result, 'attempts': attempts, 'replies': replies}))
raise SystemExit(1 if fault or disconnect else 0 if replies else 2)
