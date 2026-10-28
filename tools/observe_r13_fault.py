"""Read allowlisted metadata only; reset is opt-in, no audio or raw log."""
import argparse
import datetime as dt
import json
import pathlib
import re
import time
import serial

ROOT = pathlib.Path(__file__).resolve().parents[1]


def classify(line):
    if line.strip() == '[DIAG] revision=S08':
        return {'kind':'diagnostic_revision','value':'S08'}
    hold = re.fullmatch(r'\[Wake\] diagnostic_hold=([01])', line.strip())
    if hold:
        return {'kind':'diagnostic_hold','enabled':int(hold[1])}
    session = re.fullmatch(r'\[DEBUG-sv8\] session=(\d{1,10}) state=([1-7]) take=([0-3]) frames=(\d{1,3})', line.strip())
    if session and 1<=int(session[1])<=0xffffffff and int(session[4])<=186:
        return dict(kind='enrollment_session',**dict(zip(('session','state','take','frames'),map(int,session.groups()))))
    if line.strip() == '[DIAG] revision=S07':
        return {'kind':'diagnostic_revision','value':'S07'}
    comparison = re.fullmatch(r'\[DEBUG-sv7\] current=(\d{1,4}) previous=(\d{1,4}) available=([01])',line.strip())
    if comparison and int(comparison[1])<=2001 and int(comparison[2])<=2001:
        return dict(kind='registration_comparison',current=int(comparison[1]),previous=int(comparison[2]),available=int(comparison[3]))
    if line.strip() == '[DIAG] revision=S06':
        return {'kind':'diagnostic_revision','value':'S06'}
    slot = re.fullmatch(r'\[DEBUG-sv6\] slot=([01]) valid=([01]) generation=(\d{1,10}) frames=(\d{1,3})',line.strip())
    if slot and int(slot[3])<=0xffffffff and int(slot[4])<=186:
        return dict(kind='registration_slot',**dict(zip(('slot','valid','generation','frames'),map(int,slot.groups()))))
    audit = re.fullmatch(r'\[DEBUG-sv6\] active=([01]) generation=(\d{1,10}) frames=(\d{1,3}) ram=([01]) disk=([01]) same=([01])',line.strip())
    if audit and int(audit[2])<=0xffffffff and int(audit[3])<=186:
        return dict(kind='registration_audit',**dict(zip(('slot','generation','frames','ram','disk','same'),map(int,audit.groups()))))
    runtime = re.fullmatch(r'\[DEBUG-sv6\] runtime_frames=(\d{1,5}) ram=([01])',line.strip())
    if runtime and int(runtime[1])<=65535:
        return dict(kind='registration_runtime',frames=int(runtime[1]),ram=int(runtime[2]))
    take = re.fullmatch(r'\[DEBUG-sv6\] take=([1-3]) samples=(\d{1,5}) frames=(\d{1,3}) expected=(\d{1,3})',line.strip())
    if take and 6400<=int(take[2])<=48000 and all(2<=int(v)<=186 for v in take.groups()[2:]):
        return dict(kind='registration_take',**dict(zip(('take','samples','frames','expected'),map(int,take.groups()))))
    if line.strip() == '[DIAG] revision=S05':
        return {'kind': 'diagnostic_revision', 'value': 'S05'}
    crop = re.fullmatch(r'\[DEBUG-sv5\] first=(\d{1,3}) end=(\d{1,3}) template_frames=(\d{1,3}) candidate_frames=(\d{1,3})',line.strip())
    if crop and 0<=int(crop[1])<int(crop[2])<=250 and 20<=int(crop[2])-int(crop[1])<=150 and all(2<=int(v)<=186 for v in crop.groups()[2:]):
        return dict(kind='reference_window_crop',**dict(zip(('first','end','template_frames','candidate_frames'),map(int,crop.groups()))))
    quality = re.fullmatch(r'\[DEBUG-sv5\] noise=(\d{1,5}) warm=(\d{1,5}) gate=(\d{1,6}) rms=(\d{1,5}) peak=(\d{1,5}) dc=(\d{1,5}) clips=(\d{1,5})',line.strip())
    if quality and all(int(quality[i])<=65535 for i in (1,2,4,5,6)) and int(quality[3])<=200000 and int(quality[7])<=48000:
        return dict(kind='reference_window_quality',**dict(zip(('noise','warm','gate','rms','peak','dc','clips'),map(int,quality.groups()))))
    timing = re.fullmatch(r'\[DEBUG-sv5\] compute_ms=(\d{1,5}) input_gap_ms=(\d{1,5})',line.strip())
    if timing and all(int(v)<=60000 for v in timing.groups()):
        return dict(kind='reference_window_timing',**dict(zip(('compute_ms','input_gap_ms'),map(int,timing.groups()))))
    if line.strip() in ('[UI] revision=UI01', '[UI] revision=UI02', '[UI] revision=UI03', '[UI] revision=UI04'):
        return {'kind': 'ui_revision', 'value': line.strip().split('=')[1]}
    if line.strip() in tuple('[DIAG] revision=D' + str(i) for i in range(1, 14)) + ('[DIAG] revision=M01','[DIAG] revision=M02','[DIAG] revision=M03','[DIAG] revision=M04','[DIAG] revision=E01','[DIAG] revision=E02','[DIAG] revision=E03','[DIAG] revision=E04','[DIAG] revision=E05','[DIAG] revision=E06','[DIAG] revision=S01','[DIAG] revision=S02','[DIAG] revision=S03','[DIAG] revision=S04'):
        return {'kind': 'diagnostic_revision', 'value': line.strip().split('=')[1]}
    probe = re.fullmatch(r'\[DEBUG-sv2\] self=(\d{1,4}) level=(\d{1,4}) other=(\d{1,4})',line.strip())
    if probe and all(int(v)<=2001 for v in probe.groups()):
        return dict(kind='reference_engine_probe',**dict(zip(('self','level','other'),map(int,probe.groups()))))
    paired = re.fullmatch(r'\[DEBUG-sv3\] profile=([0-3]) self=(\d{1,4}) level=(\d{1,4}) other=(\d{1,4}) raw=(\d{1,10}) raw_other=(\d{1,10}) feature=(\d{1,10}) feature_other=(\d{1,10}) align=(\d{1,2})',line.strip())
    if paired and all(int(v)<=2001 for v in paired.groups()[1:4]) and all(int(v)<=0xffffffff for v in paired.groups()[4:8]) and int(paired[9])<=15:
        return dict(kind='reference_paired_probe',**dict(zip(('profile','self','level','other','raw','raw_other','feature','feature_other','align'),map(int,paired.groups()))))
    if line.strip()=='[DEBUG-sv2] probe_failed=1':
        return {'kind':'reference_engine_probe_failed'}
    quality = re.fullmatch(r'\[DEBUG-sv2\] take=([1-3]) noise=(\d{1,5}) warm=(\d{1,5}) gate=(\d{1,6}) rms=(\d{1,5}) peak=(\d{1,5}) dc=(\d{1,5}) clips=(\d{1,5})',line.strip())
    if quality and all(int(v)<=65535 for v in (quality[2],quality[3],quality[5],quality[6],quality[7])) and int(quality[4])<=200000 and int(quality[8])<=48000:
        return dict(kind='reference_input_quality',**dict(zip(('take','noise','warm','gate','rms','peak','dc','clips'),map(int,quality.groups()))))
    reference = re.fullmatch(r'\[DEBUG-sv1\] frames=(\d{1,3}) distance=(\d{1,4}) threshold=250 matched=([01])',line.strip())
    if reference and 2<=int(reference[1])<=186 and int(reference[2])<=2001:
        return dict(kind='reference_match',**dict(zip(('frames','distance','matched'),map(int,reference.groups()))))
    init = re.fullmatch(r'\[DEBUG-sv1\] init=([01]) (stored|reason)=([012])',line.strip())
    if init and ((init[1]=='1' and init[2]=='stored' and int(init[3])<=1) or (init[1]=='0' and init[2]=='reason' and int(init[3]) in (1,2))):
        return {'kind':'reference_init','initialized':bool(int(init[1])),init[2]:int(init[3])}
    pair = re.fullmatch(r'\[DEBUG-sv1\] pair=([125]) distance=(\d{1,4})',line.strip())
    if pair and int(pair[2])<=2001:
        return {'kind':'reference_pair','pair':int(pair[1]),'distance':int(pair[2])}
    crop = re.fullmatch(r'\[DEBUG-e06\] take=([1-3]) first=(\d{1,3}) end=(\d{1,3}) narrow_first=(\d{1,3}) narrow_end=(\d{1,3})', line.strip())
    if crop and all(int(v)<=250 for v in crop.groups()[1:]) and int(crop[2])<=int(crop[3]) and int(crop[4])<=int(crop[5]):
        return dict(kind='enrollment_crop',**dict(zip(('take','first','end','narrow_first','narrow_end'),map(int,crop.groups()))))
    window = re.fullmatch(r'\[DEBUG-e04\] window_crop=(\d{1,3}) matches=([0-3]) absolute=([0-3]) best_bin=(\d{1,3}) threshold_bin=(\d{1,2})', line.strip())
    if window and int(window[1])<=150 and 12<=int(window[5])<=32:
        return dict(kind='rolling_probe',**dict(zip(('frames','matches','absolute','best_bin','threshold_bin'),map(int,window.groups()))))
    template = re.fullmatch(r'\[DEBUG-e04\] template=([0-2]) frames=(\d{1,3}) actual_bin=(\d{1,3}) stationary_bin=(\d{1,3})', line.strip())
    if template and 10<=int(template[2])<=150:
        return dict(kind='rolling_comparison',**dict(zip(('template','frames','actual_bin','stationary_bin'),map(int,template.groups()))))
    epoch = re.fullmatch(r'\[DEBUG-e02\] (primary|energy)_epoch_reset=1 frames=(\d{1,3})', line.strip())
    if epoch and int(epoch[2]) <= 150:
        return {'kind':'wake_epoch_reset','path':epoch[1],'frames':int(epoch[2])}
    energy = re.fullmatch(r'\[DEBUG-e02\] energy_frames=(\d{1,3}) voice=(\d{1,3}) shape=(\d{1,3}) rejection=([0-3]) matches=([0-3]) absolute=([0-3]) best_bin=(\d{1,3}) threshold_bin=(\d{1,2})', line.strip())
    if energy and all(int(v)<=150 for v in energy.groups()[:3]) and 12<=int(energy[8])<=32:
        return dict(kind='energy_probe',**dict(zip(('frames','voice','shape','rejection','matches','absolute','best_bin','threshold_bin'),map(int,energy.groups()))))
    quality = re.fullmatch(r'\[DEBUG-r13w2\] flat_templates=([0-3]) reversed_templates=([0-3]) pair_matches=([0-3])', line.strip())
    if quality:
        return dict(kind='wake_template_quality', **dict(zip(('flat_templates','reversed_templates','pair_matches'),map(int,quality.groups()))))
    sequence = re.fullmatch(r'\[DEBUG-r13w2\] sequence_pairs=([0-3])', line.strip())
    if sequence:
        return {'kind': 'wake_sequence_quality', 'sequence_pairs': int(sequence[1])}
    contrast = re.fullmatch(r'\[DEBUG-r13w2\] base_matches=([0-3]) mean_matches=([0-3])(?: sequence_matches=([0-3]))?', line.strip())
    if contrast:
        event = {'kind': 'wake_contrast', 'base_matches': int(contrast[1]), 'mean_matches': int(contrast[2])}
        if contrast[3] is not None: event['sequence_matches'] = int(contrast[3])
        return event
    centered = re.fullmatch(r'\[DEBUG-r13w2\] raw_matches=([0-3]) centered_matches=([0-3]) sequence_matches=([0-3])', line.strip())
    if centered:
        return dict(kind='wake_centered_contrast', **dict(zip(('raw_matches','centered_matches','sequence_matches'),map(int,centered.groups()))))
    strict = re.fullmatch(r'\[DEBUG-r13w2\] strict25=([0-3]) strict35=([0-3]) strict50=([0-3])', line.strip())
    if strict:
        return dict(kind='wake_strict', **dict(zip(('margin25','margin35','margin50'),map(int,strict.groups()))))
    if line.strip() == '[DEBUG-r13w2] held=1':
        return {'kind': 'wake_held'}
    enrollment = re.fullmatch(r'\[DEBUG-r13w3\] rejection=([1-6])', line.strip())
    if enrollment:
        return {'kind': 'enrollment_rejection', 'reason': int(enrollment[1])}
    pair = re.fullmatch(r'\[DEBUG-r13w3\] pair_flags=(\d{1,2}) candidate_bin=(\d{1,2}) reference_bin=(\d{1,2})', line.strip())
    if pair and int(pair[1]) <= 15 and 1 <= int(pair[2]) <= 15 and 1 <= int(pair[3]) <= 15:
        return dict(kind='enrollment_pair', **dict(zip(('flags','candidate_bin','reference_bin'),map(int,pair.groups()))))
    voice = re.fullmatch(r'\[DEBUG-r13v1\] frames=(\d{1,3}) energy=(\d{1,3}) speech=(\d{1,3}) joint=(\d{1,3})', line.strip())
    if voice and all(int(v) <= 100 for v in voice.groups()):
        return dict(kind='speech_gate', **dict(zip(('frames','energy','speech','joint'),map(int,voice.groups()))))
    probe = re.fullmatch(r'\[DEBUG-r13v2\] mild=(\d{1,3}) centered=(\d{1,3}) gain4=(\d{1,3}) rms=(\d{1,5}) dc=(\d{1,5})', line.strip())
    if probe and all(int(v) <= 100 for v in probe.groups()[:3]) and all(int(v) <= 65535 for v in probe.groups()[3:]):
        return dict(kind='speech_probe', **dict(zip(('mild','centered','gain4','rms','dc'),map(int,probe.groups()))))
    evidence = re.fullmatch(r'\[DEBUG-r13v1\] segment_voice=(\d{1,3}) segment_shape=(\d{1,3})', line.strip())
    if evidence and all(int(v) <= 150 for v in evidence.groups()):
        return {'kind': 'speech_evidence', 'voice': int(evidence[1]), 'shape': int(evidence[2])}
    decision = re.fullmatch(r'\[DEBUG-r13w1\] decision=([1-5]) matches=([0-3])', line.strip())
    if decision:
        return {'kind': 'wake_decision', 'decision': int(decision[1]), 'matches': int(decision[2])}
    window = re.fullmatch(r'\[DEBUG-r13w1\] window_ms=(\d{1,10}) ready_ms=(\d{1,10}) frames=(\d{1,10}) voice=(\d{1,10}) complete=(\d{1,10}) short=(\d{1,10}) clipped=(\d{1,10}) long=(\d{1,10}) matched=(\d{1,10}) last_matches=([0-3]) draw_max_ms=(\d{1,10})', line.strip())
    if window:
        names = ('window_ms', 'ready_ms', 'frames', 'voice', 'complete', 'short', 'clipped', 'long', 'matched', 'last_matches', 'draw_max_ms')
        return dict(kind='wake_window', **dict(zip(names, map(int, window.groups()))))
    failure = re.fullmatch(r'\[DEBUG-r13d2\] api_stage=([0-7]) status=(-?\d{1,3}) reason=(\d{1,2})', line.strip())
    if failure:
        return {'kind': 'request_failure', 'stage': int(failure[1]), 'status': int(failure[2]), 'reason': int(failure[3])}
    trace = re.fullmatch(r'\[DEBUG-r13d1\] event=(\d{1,2}) state=(\d{1,2}) ms=(\d{1,10}) heap=(\d{1,10}) stack=(\d{1,10})', line.strip())
    if trace and int(trace[1]) in (1, 2, 3, 4, 10, 11, 12, 13, 14, 15, 16):
        return dict(kind='runtime_trace', **dict(zip(('event', 'state', 'ms', 'heap', 'stack'), map(int, trace.groups()))))
    api = re.fullmatch(r'\[API\] stage=([0-7]) status=(-?\d{1,3}) elapsed_ms=(\d{1,10})', line.strip())
    if api:
        return {'kind': 'api_result', 'stage': int(api[1]), 'status': int(api[2]), 'elapsed_ms': int(api[3])}
    for pattern, kind in (
        (r'Guru Meditation Error: Core\s+([01])\s+panicked\s+\(([^)]+)\)', 'panic'),
        (r'Stack canary watchpoint triggered \((loopTask|mic_task|IDLE[01])\)', 'stack_canary'),
        (r'Backtrace:\s*((?:0x[0-9a-fA-F]+:0x[0-9a-fA-F]+\s*)+)', 'backtrace'),
        (r'\brst:(0x[0-9a-fA-F]+)', 'reset'),
    ):
        match = re.search(pattern, line)
        if match:
            if kind == 'panic':
                reasons = ('LoadProhibited', 'StoreProhibited', 'IllegalInstruction', 'Unhandled debug exception', 'Interrupt wdt timeout on CPU0', 'Interrupt wdt timeout on CPU1')
                return {'kind': kind, 'core': int(match[1]), 'reason': match[2] if match[2] in reasons else 'other'}
            return {'kind': kind, 'value': match[1].strip()}
    for keyword, kind in (('Brownout detector', 'brownout'), ('watchdog', 'watchdog'), ('wdt timeout', 'watchdog'), ('abort() was called', 'abort'), ('Rebooting...', 'reboot'), ('[System] ready=1', 'ready')):
        if keyword.lower() in line.lower():
            return {'kind': kind}
    if re.search(r'\[System\] CoreS3 file conversation R13\.\d+', line):
        return {'kind': 'application_start'}
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', default='COM10')
    parser.add_argument('--seconds', type=float, default=90)
    parser.add_argument('--stop-file', type=pathlib.Path, help='Graceful stop marker inside the project .pio directory')
    parser.add_argument('--trial-file', type=pathlib.Path, help='Host-only numeric trial marker inside project .pio')
    parser.add_argument('--reset', action='store_true', help='Approved controlled reset, then require one clean UI boot')
    parser.add_argument('--expected-version', choices=('R13.3', 'R13.4'), default='R13.3')
    args = parser.parse_args()
    if args.stop_file:
        args.stop_file = args.stop_file.resolve()
        if not args.stop_file.is_relative_to(ROOT / '.pio') or args.stop_file.exists():
            parser.error('Stop marker must be a new path inside project .pio')
    if args.trial_file:
        args.trial_file = args.trial_file.resolve()
        if not args.trial_file.is_relative_to(ROOT / '.pio') or args.trial_file.exists():
            parser.error('Trial marker must be a new path inside project .pio')
    report = {'scope': 'allowlisted metadata only; no microphone capture', 'port': args.port, 'reset_requested': args.reset, 'expected_version': args.expected_version, 'events': []}
    if args.reset:
        from observe_r13_boot import consume, passed
        report.update({'storage_mounted': False, 'config_loaded': False, 'key_configured': False,
                       'wifi_connected': False, 'wifi_portal': False, 'ready': False, 'fault': False,
                       'boot_count': 0, 'unexpected_reset': False})
    port = serial.Serial(port=None, baudrate=115200, timeout=0.25)
    port.port = args.port
    port.dtr = False
    port.rts = False
    started = time.monotonic()
    report['started_at_jst'] = dt.datetime.now(dt.timezone(dt.timedelta(hours=9))).isoformat(timespec='milliseconds')
    buffer = ''
    awaiting_reset_boundary = args.reset
    last_trial = None
    try:
        port.open()
        if args.reset:
            port.reset_input_buffer()  # Discard buffered output from before this controlled reset.
            import sys
            sys.path.insert(0, str(pathlib.Path.home() / '.platformio/packages/tool-esptoolpy@1.40501.0'))
            from esptool.reset import HardReset
            HardReset(port)()
        print('Metadata observation started; reset_requested=' + str(args.reset), flush=True)
        while time.monotonic() - started < args.seconds:
            if args.stop_file and args.stop_file.exists():
                report['stopped_by_marker'] = True
                break
            if args.trial_file and args.trial_file.exists():
                trial = args.trial_file.read_text(encoding='utf-8-sig').strip()
                if re.fullmatch('[0-9]', trial) and trial != last_trial:
                    last_trial = trial
                    event = {'kind': 'trial_marker', 'trial': int(trial), 'seconds': round(time.monotonic() - started, 3)}
                    report['events'].append(event); print(json.dumps(event), flush=True)
            buffer += port.read(4096).decode('utf-8', errors='replace')
            while '\n' in buffer:
                line, buffer = buffer.split('\n', 1)
                if awaiting_reset_boundary:
                    # USB's device-side TX FIFO can deliver the previous boot's
                    # output even after the host input buffer has been cleared.
                    # Preserve allowlisted old metadata separately; start the
                    # controlled boot only at its ROM/reset boundary.
                    boundary = line.startswith('ESP-ROM:') or re.search(r'\brst:0x[0-9a-f]+', line, re.I)
                    if not boundary:
                        old = classify(line)
                        if old and len(report.setdefault('pre_reset_events', [])) < 20:
                            report['pre_reset_events'].append(old)
                        continue
                    awaiting_reset_boundary = False
                if args.reset:
                    consume(report, line)
                event = classify(line)
                if event and len(report['events']) < 1500:
                    event['seconds'] = round(time.monotonic() - started, 3)
                    report['events'].append(event)
                    print(json.dumps(event), flush=True)
                elif event:
                    report['dropped_events'] = report.get('dropped_events', 0) + 1
            if len(buffer) > 8192:
                buffer = ''
    except serial.SerialException:
        report['error_kind'] = 'SerialUnavailable'
    finally:
        port.close()
    report['elapsed_seconds'] = round(time.monotonic() - started, 3)
    report['ended_at_jst'] = dt.datetime.now(dt.timezone(dt.timedelta(hours=9))).isoformat(timespec='milliseconds')
    if args.reset:
        report['boot_pass'] = passed(report, args.expected_version) and any(e['kind'] == 'ui_revision' for e in report['events']) and 'error_kind' not in report
    stamp = dt.datetime.now(dt.timezone(dt.timedelta(hours=9))).strftime('%Y%m%d-%H%M%S')
    directory = ROOT / 'docs/validation' / ('r13-fault-' + stamp)
    directory.mkdir(parents=True, exist_ok=False)
    path = directory / 'report.json'
    path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'report': str(path), 'events': len(report['events']),
                      **{k: report[k] for k in ('elapsed_seconds', 'boot_pass', 'boot_count', 'fault', 'unexpected_reset', 'dropped_events', 'error_kind') if k in report}}))
    return 2 if 'error_kind' in report else 1 if args.reset and not report['boot_pass'] else 0


if __name__ == '__main__':
    raise SystemExit(main())
