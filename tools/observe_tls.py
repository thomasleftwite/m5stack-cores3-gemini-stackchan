"""Observe the real TLS failure on the target without sending commands or logging secrets."""
import argparse
import json
import time
import serial

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port', default='COM10')
parser.add_argument('--seconds', type=float, default=45)
args = parser.parse_args()
port = serial.Serial()
port.port = args.port
port.baudrate = 115200
port.timeout = 0.25
port.dtr = False
port.rts = False
try:
    port.open()
except serial.SerialException:
    print(json.dumps({'result': 'PORT_UNAVAILABLE', 'port': args.port}))
    raise SystemExit(3)
print('[TLS-OBSERVE] Monitoring started. Trigger the same screen tap / speech.', flush=True)
line_buffer = ''
result = 'NO_TLS_ATTEMPT_OBSERVED'
try:
    deadline = time.monotonic() + args.seconds
    while time.monotonic() < deadline:
        line_buffer += port.read(4096).decode('utf-8', errors='replace')
        while '\n' in line_buffer:
            line, line_buffer = line_buffer.split('\n', 1)
            if 'SSL - Memory allocation failed' in line or 'start_ssl_client: -32512' in line:
                result = 'TLS_ALLOCATION_FAILED'
                print('[TLS-OBSERVE] TLS_ALLOCATION_FAILED', flush=True)
            elif '[VAD] End of user speech' in line:
                print('[TLS-OBSERVE] TLS attempt triggered', flush=True)
            elif '[Gemini] TLS connected' in line or '[Gemini] Stream complete.' in line:
                if result != 'TLS_ALLOCATION_FAILED':
                    result = 'TLS_CONNECTED'
                print('[TLS-OBSERVE] TLS_CONNECTED', flush=True)
        if len(line_buffer) > 8192:
            line_buffer = ''
finally:
    port.close()
print(json.dumps({'result': result, 'port': args.port, 'seconds': args.seconds}))
raise SystemExit({'TLS_CONNECTED': 0, 'TLS_ALLOCATION_FAILED': 1}.get(result, 2))
