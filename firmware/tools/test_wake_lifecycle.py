"""Host regression of the actual Ready -> cue -> capture -> request dispatch.

Run: python -B firmware/tools/test_wake_lifecycle.py
Requires the installed Visual C++ Build Tools. No hardware, network or user PCM.
This compiles extracted main.cpp functions, touch dispatch and enrollment service
with I/O fakes, the real CaptureGate and EnrollmentWindow. ReferenceWake is a
transaction fake: DSP/storage tests run separately. No MFCC accuracy, physical
I2S timing, display rendering or API responses are validated here.
"""

import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]


def block(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


HARNESS = r'''
#include "ConversationRules.h"
#include "EnrollmentWindow.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>
using String = std::string;
uint32_t clockMs = 4000;
uint32_t millis() { return clockMs; }
enum class Mode { Ready, Sleep, Setting, Enroll, Cue, Recording, Busy, Speaking, Error, MicError };
enum AvatarMode { Sleep };
enum AvatarEmotion { EMOTION_NORMAL, EMOTION_SLEEP, EMOTION_THINKING };
struct { void setBrightness(int) {} } display;
struct { decltype(display) Display; } M5;
struct {
    void setMode(AvatarMode) {} void setStatus(const char*) {}
    void setLipSyncLevel(int) {} void update() {}
} g_avatar;
void setEmotion(AvatarEmotion) {}
struct { void println(const char*) {} } Serial;
struct WakeFake {
    struct Diagnostics {};
    bool present=true, pending=false, active=false, candidate=false, verified=false;
    bool match=false, saveOk=true, uncertain=false;
    unsigned detections=0, resets=0, begins=0, takes=0, publishes=0;
    unsigned verificationFrames=0, calibrationInputs=0, matchingInputs=0;
    size_t smallestTake=100000;
    bool registered() { return present; }
    bool publicationUncertain() { return uncertain; }
    bool detectWindow(const int16_t*) { ++detections; return pending; }
    void resetDetection() { pending=false; verified=false; ++resets; }
    void cancelEnrollment() { active=candidate=verified=false; takes=verificationFrames=0; }
    void beginEnrollment() { cancelEnrollment(); resetDetection(); active=true; ++begins; }
    unsigned enrollmentCount() { return takes; }
    bool enrollmentCandidateReady() { return candidate; }
    int submitEnrollment(const int16_t* data,size_t size) {
        if (!active || candidate || !data || size<6400 || size>48000) return -1;
        smallestTake=std::min(smallestTake,size);
        if (++takes<3) return 1;
        candidate=true; verificationFrames=0; return 2;
    }
    bool verifyEnrollmentWindow(const int16_t*,bool allowMatch=true) {
        if (!active || !candidate) return false;
        ++verificationFrames;
        if (allowMatch) ++matchingInputs; else ++calibrationInputs;
        if (allowMatch && match && verificationFrames>=300) verified=true;
        return verified;
    }
    bool commitEnrollment() {
        if (!active || !candidate || !verified) return false;
        if (!saveOk) { cancelEnrollment(); return false; }
        ++publishes; present=true; cancelEnrollment(); return true;
    }
    Diagnostics diagnostics() { return {}; }
} wake;
struct AudioTask {
    static inline bool available=true, cueOk=true, mic=false, playing=false;
    static inline bool recording=false, voice=false, full=false;
    static inline bool enrollmentInput=false, enrollmentVoice=false;
    static inline unsigned reads=0, cues=0, starts=0, calibrations=0;
    static inline std::vector<int16_t> pcm;
    static void stopPlayback() { playing=false; }
    static void stopRecording() { recording=false; }
    static void clearRecording() { recording=false; pcm.clear(); }
    static bool setMicEnabled(bool value) {
        mic=value && available && !playing;
        return mic==value;
    }
    static void beginListeningCalibration() { ++calibrations; }
    static bool readFrame(int16_t* frame) {
        ++reads;
        if (!available || !mic) return false;
        for (unsigned i=0;i<320;++i) frame[i]=enrollmentInput ?
            ((i%2 ? -1 : 1)*(enrollmentVoice ? 500 : 10)) : 99;
        if (recording) {
            if (enrollmentInput) pcm.insert(pcm.end(),frame,frame+320);
            else pcm.insert(pcm.end(),320,7); // Question-only input.
        }
        return true;
    }
    static bool playCue() {
        ++cues; mic=false;
        if (!cueOk) return false;
        playing=true; return true;
    }
    static bool isPlaying() { return playing; }
    static void startRecording() { clearRecording(); recording=true; ++starts; }
    static bool isVoiceDetected() { return voice; }
    static bool recordingFull() { return full; }
    static const int16_t* getRecordedPCM(size_t* size) { *size=pcm.size(); return pcm.data(); }
};
struct GeminiClient {
    static inline unsigned requests=0;
    static inline bool accepts=true;
    static inline std::vector<int16_t> sent;
    static void clear(bool = false) {}
    static void cancel() {}
    static bool retry() { return false; }
    static bool start(const int16_t* data,size_t count,const String&) {
        ++requests; sent.assign(data,data+count); return accepts;
    }
};
Mode mode=Mode::Ready, micResume=Mode::Ready;
uint32_t lastActivity=0, entered=0, lastDraw=0, wakeDiagnosticAt=0, wakeDrawMaxMs=0;
WakeFake::Diagnostics wakeBaseline;
conversation::CaptureGate gate;
String notice;
bool cancelRequested=false;
int16_t frame[320];
String getFormattedJSTTime() { return "synthetic"; }
void requestError() { mode=Mode::Error; }
namespace runtimetrace { enum Event { EnrollmentResult }; void mark(Event,unsigned) {} }
'''

SCENARIOS = r'''
unsigned failures=0, checks=0;
void check(bool value,const char* label) {
    ++checks;
    printf("%s: %s\n",value ? "PASS" : "FAIL",label);
    if (!value) ++failures;
}
void fresh() {
    wake=WakeFake{};
    AudioTask::available=AudioTask::cueOk=true;
    AudioTask::playing=AudioTask::recording=AudioTask::voice=AudioTask::full=false;
    AudioTask::enrollmentInput=AudioTask::enrollmentVoice=false;
    AudioTask::reads=AudioTask::cues=AudioTask::starts=AudioTask::calibrations=0;
    AudioTask::pcm.clear();
    GeminiClient::requests=0; GeminiClient::accepts=true; GeminiClient::sent.clear();
    clockMs=4000; ready(); clockMs+=1200;
}
void detect() { wake.pending=true; dispatch(millis()); }
void finishCue() { AudioTask::playing=false; dispatch(millis()); }
void capture(unsigned voicedFrames) {
    for (unsigned i=0;i<voicedFrames+50;++i) {
        AudioTask::voice=i<voicedFrames; clockMs+=20; dispatch(millis());
    }
}
void enrollFrame(bool voiced=false) {
    AudioTask::enrollmentVoice=voiced; clockMs+=20; dispatch(millis());
}
void startEnrollment(bool registered=true) {
    fresh(); wake.present=registered; enterSetting(); AudioTask::enrollmentInput=true; tap(100);
}
void enrollmentTake() {
    // Exercise the actual 50-frame background, cue, 250-frame capture, crop,
    // analysis and submit path, rather than jumping straight to CandidateReady.
    for (unsigned i=0;i<50;++i) enrollFrame();
    AudioTask::playing=false; dispatch(millis());
    for (unsigned i=0;i<250;++i) enrollFrame(i>=50 && i<90);
    for (unsigned i=0;i<300 && enrollmentPhase==EnrollmentPhase::Analyzing;++i) dispatch(millis());
}
void candidateReady() {
    for (unsigned i=0;i<3;++i) enrollmentTake();
}
void verificationPrompt() {
    // 1.2s guard consumes input without feeding the candidate verifier.
    for (unsigned i=0;i<60;++i) enrollFrame();
    for (unsigned i=0;i<50;++i) enrollFrame();
}
void enrollmentScenarios() {
    startEnrollment(false);
    check(mode==Mode::Enroll && wake.begins==1 && wake.active && AudioTask::mic,"settings explicitly begins the first enrollment session");
    enrollmentTake();
    check(wake.takes==1 && !wake.candidate && !wake.publishes && enrollmentPhase==EnrollmentPhase::Calibrating,"first real cropped take prepares the next background without publication");
    enrollmentTake(); enrollmentTake();
    check(wake.takes==3 && wake.candidate && !wake.publishes && !wake.present && wake.smallestTake>=6400 && enrollmentPhase==EnrollmentPhase::VerificationCalibration,"three captured takes create a candidate while initial registration remains unpublished");
    for (unsigned i=0;i<59;++i) enrollFrame();
    check(wake.verificationFrames==0 && enrollmentPhase==EnrollmentPhase::VerificationCalibration,"verification guard reads 59 frames without feeding or matching the candidate");
    enrollFrame();
    for (unsigned i=0;i<49;++i) enrollFrame();
    check(wake.calibrationInputs==49 && wake.matchingInputs==0 && enrollmentPhase==EnrollmentPhase::VerificationWarmup,"49 valid background frames cannot show the speaking prompt or permit matching");
    enrollFrame();
    check(wake.calibrationInputs==50 && wake.matchingInputs==0 && enrollmentPhase==EnrollmentPhase::Verifying && notice=="同じ呼びかけを1回話してください","50th valid background frame shows the explicit verification prompt");
    const unsigned starts=AudioTask::starts, cues=AudioTask::cues;
    wake.match=true;
    for (unsigned i=0;i<249;++i) enrollFrame(true);
    check(!wake.publishes && wake.verificationFrames==299,"post-prompt input continues the existing rolling window before matching");
    enrollFrame(true);
    check(mode==Mode::Enroll && enrollmentPhase==EnrollmentPhase::Saved && wake.present && wake.publishes==1 && !wake.active && !wake.candidate && !AudioTask::mic && GeminiClient::requests==0 && AudioTask::starts==starts && AudioTask::cues==cues,"candidate match publishes once and shows success without question cue, recording or API request");
    serviceEnrollment();
    check(wake.publishes==1 && notice.find("保存しました")!=String::npos,"success phase retains the saved result and cannot republish");
    clockMs+=2999; dispatch(millis());
    check(mode==Mode::Enroll,"saved result remains visible for the full three-second interval");
    clockMs+=1; dispatch(millis());
    check(mode==Mode::Ready && notice.find("保存しました")!=String::npos,"saved result returns to Ready after three seconds and retains notice for settings");

    for (bool registered : {false,true}) {
        startEnrollment(registered); candidateReady();
        wake.match=true; // Even an immediately matching fake must not match during background preparation.
        verificationPrompt();
        check(!wake.publishes && wake.matchingInputs==0,"background preparation explicitly suppresses candidate-match authorization");
        clockMs=enrollmentVerificationAt+30000; dispatch(millis());
        check(mode==Mode::Setting && !wake.active && !wake.candidate && wake.present==registered && !wake.publishes && !AudioTask::mic && notice.find(registered ? "旧登録を保持" : "未登録のまま")!=String::npos,"verification timeout discards candidate and accurately reports retained registration state");
        startEnrollment(registered); candidateReady(); tap(200);
        check(mode==Mode::Setting && !wake.active && !wake.candidate && wake.present==registered && !wake.publishes && !AudioTask::mic && notice.find(registered ? "旧登録を保持" : "未登録のまま")!=String::npos,"touch cancellation discards candidate and preserves old-or-unregistered state");
        startEnrollment(registered); candidateReady(); AudioTask::available=false; dispatch(millis());
        check(mode==Mode::MicError && micResume==Mode::Enroll && !wake.active && !wake.candidate && wake.present==registered && !wake.publishes,"verification microphone failure cancels the transaction and preserves registration");
        tap(100);
        check(mode==Mode::MicError && wake.begins==1,"failed microphone retry cannot create a partial enrollment session");
        AudioTask::available=true; tap(100);
        check(mode==Mode::Enroll && wake.begins==2 && wake.active && wake.takes==0 && !wake.candidate && enrollmentPhase==EnrollmentPhase::Calibrating,"successful microphone retry explicitly starts a new session at take one");
        candidateReady(); verificationPrompt(); wake.match=true;
        for (unsigned i=0;i<250;++i) enrollFrame(true);
        clockMs+=3000; dispatch(millis());
        check(mode==Mode::Ready && wake.publishes==1,"fresh three-take enrollment can publish after microphone retry");
        startEnrollment(registered); candidateReady(); verificationPrompt(); wake.match=true; wake.saveOk=false;
        for (unsigned i=0;i<250;++i) enrollFrame(true);
        check(mode==Mode::Setting && wake.present==registered && !wake.publishes && !wake.active && notice.find(registered ? "旧登録を保持" : "未登録のまま")!=String::npos,"recoverable save failure reports the actual unchanged registration state");
    }
    startEnrollment(); candidateReady(); verificationPrompt(); wake.match=true; wake.saveOk=false; wake.uncertain=true;
    for (unsigned i=0;i<250;++i) enrollFrame(true);
    check(mode==Mode::Setting && notice=="保存結果を確認できません。再起動後に確認してください" && notice.find("旧登録を保持")==String::npos,"uncertain publication never claims that the old registration is preserved");
    startEnrollment(); AudioTask::available=false; dispatch(millis()); AudioTask::available=true; tap(200);
    check(mode==Mode::Setting && !wake.active && wake.present,"cancel from microphone recovery returns safely to settings");
    fresh(); enterSetting(); AudioTask::available=false; tap(100);
    check(mode==Mode::MicError && !wake.active && !wake.candidate,"microphone startup failure cancels the newly opened enrollment session");
    startEnrollment();
    for (unsigned i=0;i<50;++i) enrollFrame();
    bool cuePlaying=AudioTask::playing; tap(200);
    check(cuePlaying && mode==Mode::Setting && !AudioTask::playing && !AudioTask::mic && !wake.active,"cancelling during an enrollment cue stops playback and releases microphone ownership");
    startEnrollment(); candidateReady(); clockMs=0xfffffff0u; enrollmentVerificationAt=clockMs;
    verificationPrompt();
    check(enrollmentPhase==EnrollmentPhase::Verifying && wake.calibrationInputs==50,"candidate guard and background prompt work across millisecond rollover");
    wake.match=true; for(unsigned i=0;i<250;++i) enrollFrame(true); tap(200);
    check(mode==Mode::Ready && wake.publishes==1 && notice.find("保存しました")!=String::npos && notice.find("中止")==String::npos,"touching the saved result returns to Ready without claiming a published registration was cancelled");
}
int main() {
    enrollmentScenarios();
    fresh(); wake.present=false; detect();
    check(mode==Mode::Ready && wake.detections==0 && AudioTask::cues==0 && GeminiClient::requests==0,"unregistered wake cannot start a question");
    fresh(); clockMs=entered+1199; detect();
    bool guard=mode==Mode::Ready && wake.detections==0 && AudioTask::reads==1;
    check(guard,"Ready guard still reads calibration frames and suppresses detection before 1200ms");
    fresh(); enterSetting(); detect(); serviceReadyWake(millis());
    check(mode==Mode::Setting && AudioTask::reads==0 && AudioTask::cues==0 && GeminiClient::requests==0 && !AudioTask::mic,"settings suppress pending wake and release microphone");
    fresh(); clockMs=lastActivity+conversation::IdleMs; detect();
    check(mode==Mode::Sleep && AudioTask::reads==0 && !wake.pending && !AudioTask::mic,"idle sleep wins over a pending wake and clears its window");
    fresh(); AudioTask::available=false; detect();
    check(mode==Mode::MicError && micResume==Mode::Ready && AudioTask::cues==0 && GeminiClient::requests==0,"Ready microphone failure offers recovery without a question");
    fresh();
    for (Mode other : {Mode::Sleep,Mode::Enroll,Mode::Cue,Mode::Recording,Mode::Busy,Mode::Speaking,Mode::Error,Mode::MicError}) {
        mode=other; serviceReadyWake(millis());
    }
    check(AudioTask::reads==0 && AudioTask::cues==0,"wake service ignores every non-Ready mode");
#if STACKCHAN_WAKE_DIAGNOSTIC_HOLD
    check(readyStatus().find("診断中")!=String::npos,"diagnostic hold has a visible Ready status");
    fresh(); detect();
    check(mode==Mode::Ready && !wake.pending && AudioTask::cues==0 && AudioTask::starts==0 && GeminiClient::requests==0,"explicit diagnostic hold consumes match without cue, recording or request");
    for (unsigned i=0;i<100;++i) { clockMs+=20; dispatch(millis()); }
    check(AudioTask::cues==0 && AudioTask::starts==0 && GeminiClient::requests==0,"diagnostic hold remains isolated on later frames");
#else
    fresh(); check(readyStatus().empty(),"normal registered Ready status does not claim diagnostic hold");
    fresh(); AudioTask::pcm.assign(320,99); detect();
    check(mode==Mode::Cue && AudioTask::cues==1 && AudioTask::starts==0 && AudioTask::pcm.empty() && !wake.pending && !AudioTask::mic,"normal match enters cue once and clears stale PCM and wake window");
    wake.pending=true;
    for (unsigned i=0;i<5;++i) dispatch(millis());
    check(mode==Mode::Cue && AudioTask::cues==1 && AudioTask::starts==0 && GeminiClient::requests==0,"unfinished cue cannot record, send or start twice");
    finishCue(); serviceQuestionCue();
    check(mode==Mode::Recording && AudioTask::starts==1 && AudioTask::pcm.empty() && AudioTask::mic,"finished cue starts exactly one clean recording");
    capture(15);
    bool clean=!GeminiClient::sent.empty() && std::all_of(GeminiClient::sent.begin(),GeminiClient::sent.end(),[](int16_t s){return s==7;});
    check(mode==Mode::Busy && GeminiClient::requests==1 && !AudioTask::recording && !AudioTask::mic && clean,"voiced question and end silence submit question-only PCM once");
    serviceReadyWake(millis());
    check(GeminiClient::requests==1 && AudioTask::cues==1,"pending wake during Busy cannot restart or resend");
    ready(); dispatch(millis());
    bool rearm=mode==Mode::Ready && !wake.pending && AudioTask::cues==1;
    clockMs+=1200; detect(); finishCue(); capture(15);
    check(rearm && mode==Mode::Busy && AudioTask::cues==2 && GeminiClient::requests==2,"return to Ready drops stale wake then allows one fresh second conversation");
    fresh(); AudioTask::cueOk=false; detect();
    check(mode==Mode::Ready && !wake.pending && AudioTask::pcm.empty() && AudioTask::starts==0 && GeminiClient::requests==0,"failed cue returns cleanly to Ready without recording or request");
    fresh(); detect(); AudioTask::available=false; finishCue();
    check(mode==Mode::Ready && AudioTask::starts==0 && GeminiClient::requests==0,"microphone startup failure after cue cannot record or request");
    fresh(); detect(); finishCue(); capture(0);
    clockMs+=conversation::StartWaitMs; dispatch(millis());
    check(mode==Mode::Ready && GeminiClient::requests==0 && AudioTask::pcm.empty(),"empty question times out without sending and clears PCM");
    fresh(); detect(); finishCue(); capture(3);
    check(mode==Mode::Ready && GeminiClient::requests==0,"question shorter than 200ms voiced is rejected without sending");
    fresh(); detect(); finishCue(); GeminiClient::accepts=false; capture(15);
    check(mode==Mode::Error && GeminiClient::requests==1 && !AudioTask::recording,"request startup failure follows existing request-error path");
    fresh(); detect(); finishCue(); AudioTask::available=false; dispatch(millis());
    check(mode==Mode::MicError && GeminiClient::requests==0 && AudioTask::pcm.empty(),"recording microphone failure clears input and cannot submit");
    fresh(); clockMs=0xfffffff0u; ready(); clockMs+=1200; detect();
    check(mode==Mode::Cue && AudioTask::cues==1,"Ready timing guard works across millisecond rollover");
#endif
    printf("RESULT: %u checks, %u failures\n",checks,failures);
    return failures ? 1 : 0;
}
'''


def main():
    source = (ROOT / 'src/main.cpp').read_text(encoding='utf-8-sig')
    policy_start = source.index('#ifndef STACKCHAN_WAKE_DIAGNOSTIC_HOLD')
    policy_end = source.index('\n\n', policy_start)
    policy = source[policy_start:policy_end]
    functions = '\n'.join(block(source, signature) for signature in (
        'static void ready(', 'static void sleepNow(', 'static void enterSetting(',
        'static void beginQuestion(', 'static void serviceQuestionCue(',
        'static String enrollmentRetentionMessage(', 'static void microphoneError(',
        'static void serviceReadyWake(', 'static void enrollmentFailed(',
        'static bool prepareEnrollment(', 'static void beginEnrollmentSession(',
        'static void retryMicrophone(', 'static void cancelEnrollmentSession(',
        'static void serviceEnrollment(', 'static String readyStatus('))
    loop = block(source, 'void loop()')
    touch = 'void tap(int y) {\n' + block(loop, 'switch (mode)') + '\n}'
    enrollment_start = source.index('enum class EnrollmentPhase')
    enrollment_end = source.index('\nstatic void setEmotion(', enrollment_start)
    enrollment_state = source[enrollment_start:enrollment_end]
    start = loop.index('if (mode == Mode::Ready)')
    end = loop.index('} else if (mode == Mode::Busy)', start)
    dispatch = 'void dispatch(uint32_t now) {\n' + loop[start:end] + '}\n}'
    out = ROOT / '.pio/wake-lifecycle-tests'
    out.mkdir(parents=True, exist_ok=True)
    test = out / 'wake_lifecycle.cpp'
    test.write_text(policy + '\n' + HARNESS + enrollment_state + functions + '\n' + touch + '\n' + dispatch + SCENARIOS, encoding='utf-8')
    vswhere = pathlib.Path(r'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe')
    vs = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-property', 'installationPath'], text=True).strip()
    vcvars = pathlib.Path(vs) / 'VC/Auxiliary/Build/vcvars64.bat'
    for label, value in (('default', None), ('normal', '0'), ('hold', '1'), ('invalid', '2')):
        exe = out / f'{label}.exe'
        obj = out / f'{label}.obj'
        flag = '' if value is None else f'/DSTACKCHAN_WAKE_DIAGNOSTIC_HOLD={value}'
        command = f'"{vcvars}" >nul && cl /nologo /std:c++17 /utf-8 /EHsc /W4 {flag} /I"{ROOT / "src"}" /Fe:"{exe}" /Fo:"{obj}" "{test}"'
        runner = out / f'build_{label}.ps1'
        runner.write_text("$buildCommand='" + command.replace("'", "''") + "'\n& cmd.exe /d /s /c $buildCommand\nexit $LASTEXITCODE\n", encoding='utf-8-sig')
        # Compiler diagnostics may mix Japanese system-codepage and UTF-8
        # bytes. Preserve the exit code and ASCII policy diagnostic regardless.
        result = subprocess.run(['powershell.exe', '-NoProfile', '-File', str(runner)],
                                capture_output=True, text=True, encoding='utf-8', errors='replace')
        if label == 'invalid':
            if not result.returncode or 'STACKCHAN_WAKE_DIAGNOSTIC_HOLD must be 0 or 1' not in result.stdout:
                print(result.stdout, result.stderr)
                return 1
            print('PASS: invalid diagnostic hold build flag is rejected')
            continue
        if result.returncode:
            print(result.stdout, result.stderr)
            return result.returncode
        print(f'Build policy: {label}', flush=True)
        result = subprocess.run([str(exe)])
        if result.returncode:
            return result.returncode
    return 0


if __name__ == '__main__':
    sys.exit(main())
