"""Observe camera boot/frame results without logging WiFi or credentials."""
import argparse, json, time
import serial
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--port', default='COM10')
p.add_argument('--seconds', type=float, default=25)
a = p.parse_args()
s = serial.Serial()
s.port, s.baudrate, s.timeout = a.port, 115200, 0.25
s.dtr = s.rts = False
try:
    s.open()
except serial.SerialException:
    print(json.dumps({'result': 'PORT_UNAVAILABLE'}))
    raise SystemExit(3)
result = 'NO_CAMERA_RESULT'
buffer = ''
ready = False
try:
    end = time.monotonic() + a.seconds
    while time.monotonic() < end:
        buffer += s.read(4096).decode('utf-8', errors='replace')
        while '\n' in buffer:
            line, buffer = buffer.split('\n', 1)
            if 'i2c driver install error' in line:
                print('I2C_INSTALL_FAILED', flush=True)
                result = 'CAMERA_FAILED'
            elif '[Camera] Init warning:' in line or 'camera: sccb init err' in line or '[Camera] Failed' in line:
                print('CAMERA_FAILED', flush=True)
                result = 'CAMERA_FAILED'
            elif '[Camera] GC0308 Initialized successfully' in line:
                print('CAMERA_INITIALIZED', flush=True)
                if result == 'NO_CAMERA_RESULT': result = 'CAMERA_INITIALIZED'
            elif '[Camera] Frame verified:' in line:
                print('CAMERA_FRAME_VERIFIED', flush=True)
                if result != 'CAMERA_FAILED': result = 'CAMERA_FRAME_VERIFIED'
            elif '[System]' in line and 'SLEEP state' in line:
                ready = 'Camera=ready.' in line
                print('SYSTEM_READY' if ready else 'SYSTEM_DEGRADED_OR_OLD_LOG', flush=True)
        if len(buffer) > 8192: buffer = ''
finally:
    s.close()
print(json.dumps({'result': result, 'camera_ready': ready}))
raise SystemExit(0 if result == 'CAMERA_FRAME_VERIFIED' and ready else 1 if result == 'CAMERA_FAILED' else 2)
