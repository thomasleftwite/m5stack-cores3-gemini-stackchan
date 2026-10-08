"""Observe Gemini response success/failure without logging credentials or response text."""
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
print('[GEMINI-OBSERVE] Monitoring started. Trigger the same screen tap / speech.', flush=True)
line_buffer = ''
result = 'NO_GEMINI_ATTEMPT_OBSERVED'
try:
    deadline = time.monotonic() + args.seconds
    while time.monotonic() < deadline:
        line_buffer += port.read(4096).decode('utf-8', errors='replace')
        while '\n' in line_buffer:
            line, line_buffer = line_buffer.split('\n', 1)
            if '[Gemini] Stream complete. Total reply:' in line:
                empty = not line.split('Total reply:', 1)[1].strip()
                result = 'EMPTY_REPLY' if empty else 'TEXT_RECEIVED'
                print('[GEMINI-OBSERVE] ' + result, flush=True)
            elif '[Gemini] HTTP status=' in line:
                import re
                match = re.search(r'HTTP status=(-?\d+)', line)
                if match:
                    code = int(match.group(1))
                    print('[GEMINI-OBSERVE] HTTP_STATUS=' + str(code), flush=True)
                    if code != 200:
                        result = 'HTTP_FAILED'
            elif '[Gemini] Stream complete. Text bytes=' in line:
                import re
                match = re.search(r'Text bytes=(\d+)', line)
                result = 'TEXT_RECEIVED' if match and int(match.group(1)) > 0 else 'EMPTY_REPLY'
                print('[GEMINI-OBSERVE] ' + result, flush=True)
            elif '[Gemini] API error ' in line:
                import re
                fields = re.findall(r'(?:status|reason|category)=[A-Z0-9_]+', line)
                print('[GEMINI-OBSERVE] API_ERROR ' + ' '.join(fields), flush=True)
            elif '[Gemini] Stream failed.' in line:
                result = 'STREAM_FAILED'
                import re
                reason = re.search(r'reason=([A-Z0-9_]+)', line)
                print('[GEMINI-OBSERVE] STREAM_FAILED' + (' reason=' + reason.group(1) if reason else ''), flush=True)
            elif 'UNKNOWN ERROR CODE (004C)' in line:
                print('[GEMINI-OBSERVE] TLS_READ_ERROR_76', flush=True)
            elif '[VAD] End of user speech' in line:
                print('[GEMINI-OBSERVE] Request triggered', flush=True)
        if len(line_buffer) > 8192:
            line_buffer = ''
finally:
    port.close()
print(json.dumps({'result': result, 'port': args.port, 'seconds': args.seconds}))
raise SystemExit(0 if result == 'TEXT_RECEIVED' else 2 if result == 'NO_GEMINI_ATTEMPT_OBSERVED' else 1)
