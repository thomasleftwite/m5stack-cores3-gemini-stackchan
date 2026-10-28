"""Offline boot-log classification regressions, fixed nonpersonal fixtures only."""
import pathlib
import sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from tools.observe_r13_boot import consume, passed

GOOD = ["ESP-ROM:esp32s3", "rst:0x15 (USB_UART_CHIP_RESET)",
        "[System] CoreS3 file conversation R13.3", "[Storage] backend=FFat mounted=1",
        "[Config] Local configuration loaded.", "[Config] key_configured=1",
        "[WiFi] Connected!", "[Wake] registered=0", "[System] ready=1"]


def replay(lines):
    report = {}
    for line in lines: consume(report, line)
    return report


assert passed(replay(GOOD))
assert not passed(replay(GOOD + ["ESP-ROM:esp32s3"]))
assert not passed(replay(GOOD + ["Rebooting..."]))
assert not passed(replay(GOOD + ["abort() was called"]))
assert not passed(replay(GOOD + ["Task watchdog got triggered"]))
assert not passed(replay(GOOD + ["Interrupt wdt timeout on CPU0"]))
assert not passed(replay(GOOD + GOOD))
assert not passed(replay(GOOD + ["[System] CoreS3 file conversation R13.3", "[System] ready=1"]))
assert not passed(replay(GOOD[:-1]))
print("PASS: one-boot success; missing-ready/reset/abort/WDT/mixed-boot evidence rejected (9 fixtures)")
new = replay([line.replace('R13.3', 'R13.4') for line in GOOD])
assert not passed(new)
assert passed(new, 'R13.4')
print('PASS: R13.4 requires explicitly selecting its expected version; R13.3 default unchanged')

from tools.observe_r13_fault import classify
crop = '[DEBUG-sv5] first=122 end=176 template_frames=66 candidate_frames=66'
quality = '[DEBUG-sv5] noise=69 warm=42 gate=169 rms=401 peak=1577 dc=81 clips=0'
timing = '[DEBUG-sv5] compute_ms=45 input_gap_ms=80'
assert classify('[DIAG] revision=S05')['value'] == 'S05'
assert classify(crop) == dict(kind='reference_window_crop', first=122, end=176, template_frames=66, candidate_frames=66)
assert classify(quality) == dict(kind='reference_window_quality', noise=69, warm=42, gate=169, rms=401, peak=1577, dc=81, clips=0)
assert classify(timing) == dict(kind='reference_window_timing', compute_ms=45, input_gap_ms=80)
for line in (crop.replace('first=122', 'first=177'), crop.replace('end=176', 'end=251'),
             crop.replace('candidate_frames=66', 'candidate_frames=187'),
             quality.replace('noise=69', 'noise=65536'), quality.replace('gate=169', 'gate=200001'),
             timing.replace('compute_ms=45', 'compute_ms=60001'), timing.replace('input_gap_ms=80', 'input_gap_ms=-1')):
    assert classify(line) is None, line
for line in (crop, quality, timing):
    assert classify(line + ' arbitrary_text') is None
    assert classify(line + '\nprivate_payload') is None
assert classify('[DEBUG-sv5] arbitrary_text') is None
print('PASS: S05 crop/quality/timing metadata accepted; invalid bounds and arbitrary payloads rejected')
assert classify('[DIAG] revision=S06')['value']=='S06'
for text,kind in (
    ('[DEBUG-sv6] slot=0 valid=1 generation=1 frames=42','registration_slot'),
    ('[DEBUG-sv6] active=0 generation=1 frames=42 ram=1 disk=1 same=1','registration_audit'),
    ('[DEBUG-sv6] runtime_frames=55 ram=0','registration_runtime'),
    ('[DEBUG-sv6] take=2 samples=11200 frames=42 expected=42','registration_take')):
    assert classify(text)['kind']==kind
    assert classify(text+' arbitrary_payload') is None
assert classify('[DEBUG-sv6] active=0 generation=4294967296 frames=42 ram=1 disk=1 same=1') is None
print('PASS: S06 storage/RAM integrity and enrollment-length metadata; unknown payload rejected')
assert classify('[DIAG] revision=S07')['value']=='S07'
assert classify('[DEBUG-sv7] current=345 previous=180 available=1')==dict(kind='registration_comparison',current=345,previous=180,available=1)
assert classify('[DEBUG-sv7] current=2002 previous=180 available=1') is None
assert classify('[DEBUG-sv7] current=345 previous=180 available=1 arbitrary_payload') is None
print('PASS: S07 same-input comparison metadata remains numeric and bounded')
assert classify('[DIAG] revision=S08')['value']=='S08'
for state in range(1,8):
    line=f'[DEBUG-sv8] session=4294967295 state={state} take=3 frames=186'
    assert classify(line)==dict(kind='enrollment_session',session=4294967295,state=state,take=3,frames=186)
    assert classify(line+' arbitrary_payload') is None
for bad in ('session=0 state=1 take=0 frames=0','session=4294967296 state=1 take=0 frames=0',
            'session=1 state=8 take=0 frames=0','session=1 state=1 take=4 frames=0',
            'session=1 state=1 take=0 frames=187'):
    assert classify('[DEBUG-sv8] '+bad) is None
assert classify('[Wake] diagnostic_hold=0')==dict(kind='diagnostic_hold',enabled=0)
assert classify('[Wake] diagnostic_hold=1')==dict(kind='diagnostic_hold',enabled=1)
assert classify('[Wake] diagnostic_hold=2') is None
assert classify('[Wake] diagnostic_hold=0 arbitrary_payload') is None
print('PASS: S08 session state and diagnostic hold are numeric, bounded and reject arbitrary payload')
