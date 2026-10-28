"""Replay the actual main.cpp drawing branch and avatar with a display spy; no device."""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]


def block(source, start):
    begin = source.index(start)
    brace = source.index('{', begin)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[begin:end]


def main():
    out = ROOT / '.pio/r13-tests'
    out.mkdir(parents=True, exist_ok=True)
    source = (ROOT / 'src/main.cpp').read_text(encoding='utf-8-sig')
    menu = block(source, 'static void drawMenu(')
    enter_setting = block(source, 'static void enterSetting(')
    face_mode = block(source, 'static AvatarMode faceModeFor(')
    sleep_now = block(source, 'static void sleepNow(')
    failed_enrollment = block(source, 'static void enrollmentFailed(')
    enrollment_globals = source[source.index('enum class EnrollmentPhase'):source.index('static void setEmotion(')]
    mic_error = block(source, 'static void microphoneError(')
    prepare_enrollment = block(source, 'static bool prepareEnrollment(')
    retention_message = block(source, 'static String enrollmentRetentionMessage(')
    begin_enrollment = block(source, 'static void beginEnrollmentSession(')
    retry_microphone = block(source, 'static void retryMicrophone(')
    cancel_enrollment = block(source, 'static void cancelEnrollmentSession(')
    ready_status = block(source, 'static String readyStatus(')
    service_enrollment = block(source, 'static void serviceEnrollment(')
    begin_question = block(source, 'static void beginQuestion(')
    service_question_cue = block(source, 'static void serviceQuestionCue(')
    tap_switch = block(block(source, 'void loop()'), 'switch (mode)')
    interval_start=source.index('const uint32_t drawInterval =')
    interval=source[interval_start:source.index(';',interval_start)+1]
    draw = interval + '\n' + block(source, 'if (mode != Mode::Sleep && millis() - lastDraw >= drawInterval)')
    harness = r'''
#include "StackChanAvatar.h"
#include "EnrollmentWindow.h"
#include "LocalWakeCore.h"
#include "../../tests/SyntheticSpeech.h"
#include <stdio.h>
#include <set>
#include <fstream>
enum class Mode { Ready, Sleep, Setting, Enroll, Cue, Recording, Busy, Speaking, Error, MicError };
StackChanAvatar g_avatar;
struct { unsigned count = 0; bool present = false; bool registered() { return present; } unsigned enrollmentCount() { return count; }
    int next = 0; unsigned calls = 0, verificationFrames = 0, commits = 0;
    bool candidate = false, match = false;
    bool publicationUncertain() { return false; }
    void resetDetection() {} void beginEnrollment() { count = 0; }
    void cancelEnrollment() { count=verificationFrames=0; candidate=false; }
    bool enrollmentCandidateReady() { return candidate; }
    bool verifyEnrollmentWindow(const int16_t*,bool allowMatch=true) { return ++verificationFrames>=300 && allowMatch && match; }
    bool commitEnrollment() { if(!candidate || !match) return false; ++commits; present=true; cancelEnrollment(); return true; }
    int submitEnrollment(const int16_t*,size_t) { ++calls; int r=next; next=0; if(r==1) ++count; else if(r==2) { count=3; candidate=true; verificationFrames=0; } else cancelEnrollment(); return r; } } wake;
struct AudioTask {
    static inline unsigned cues=0, calibrations=0, used=0;
    static inline bool playing=false, available=true, recording=false, emitVoice=true, cueOk=true;
    static inline int16_t pcm[80000]={};
    static float getLiveRMS() { return 0; }
    static bool setMicEnabled(bool) { return available; }
    static void clearRecording() { used=0; recording=false; }
    static void stopRecording() { recording=false; }
    static void startRecording() { used=0; recording=true; }
    static const int16_t* getRecordedPCM(size_t* n) { *n=used; return pcm; }
    static void beginListeningCalibration() { ++calibrations; }
    static bool readFrame(int16_t* frame) {
        if(!available) return false;
        unsigned n=used/320;
        if(recording && emitVoice && n>=100 && n<130) syntheticSpeechFrame(frame,n-100,3000);
        else std::fill(frame,frame+320,0);
        if(recording) { std::copy(frame,frame+320,pcm+used); used+=320; }
        testClock+=20; return true;
    }
    static bool playCue() { if(!cueOk) return false; ++cues; playing=true; return true; }
    static bool isPlaying() { return playing; }
    static void stopPlayback() { playing=false; }
};
namespace runtimetrace { enum { EnrollmentResult }; void mark(int, unsigned) {} }
struct GeminiClient { static bool retry() { return false; } static void cancel() {} static void clear(bool = false) {} };
struct { void reset(uint32_t) {} } gate;
Mode mode = Mode::Ready;
Mode micResume = Mode::Ready;
uint32_t entered = 0, lastActivity = 0;
bool cancelRequested = false;
String notice;
uint32_t lastDraw = 0;
uint32_t wakeDrawMaxMs = 0;
int16_t frame[320];
void ready(const String& message = "") { mode = Mode::Ready; notice = message; }
void setEmotion(AvatarEmotion emotion) { g_avatar.setEmotion(emotion); }
String stageText() { return "busy"; }
String requestErrorText() { return "error"; }
''' + enrollment_globals + '\n' + retention_message + '\n' + mic_error + '\n' + face_mode + '\n' + sleep_now + '\n' + enter_setting + '\n' + begin_question + '\n' + service_question_cue + '\n' + menu + '\n' + failed_enrollment + '\n' + prepare_enrollment + '\n' + begin_enrollment + '\n' + retry_microphone + '\n' + cancel_enrollment + '\n' + service_enrollment + '\n' + ready_status + '\nvoid tap(int x, int y) {\n' + tap_switch + '\n}\nvoid draw() {\n' + draw + r'''
}
int main(int argc, char** argv) {
    g_avatar.init(&M5.Display);
    draw(); testClock += 100; draw();
    bool readyOk = !M5.Display.settings && M5.Display.lostSettings == 0;
    printf("%s: ready face has no persistent settings button\n", readyOk ? "PASS" : "FAIL");
    bool faceOnly = M5.Display.text.empty();
    wake.present = true; testClock += 100; draw();
    faceOnly = faceOnly && M5.Display.text.empty();
    notice = "saved";
    testClock += 100; draw();
    faceOnly = faceOnly && M5.Display.text.empty() && notice == "saved";
    printf("%s: Ready has no text when unregistered, registered or carrying a notice\n", faceOnly ? "PASS" : "FAIL");
    tap(10, 120);
    bool tapOk = mode == Mode::Setting;
    testClock += 100; draw();
    bool noticeDeferred = notice == "saved" && M5.Display.text.find("saved") != std::string::npos;
    mode = Mode::Ready; tap(0, 0);
    tapOk = tapOk && mode == Mode::Setting;
    printf("%s: one tap at center or corner opens settings\n", tapOk ? "PASS" : "FAIL");
    printf("%s: retained notification is displayed only after the settings tap\n", noticeDeferred ? "PASS" : "FAIL");
    mode = Mode::Sleep; tap(10, 120);
    bool sleepOk = mode == Mode::Ready;
    printf("%s: sleep tap only wakes to the face\n", sleepOk ? "PASS" : "FAIL");
    notice = "";
    unsigned clears = M5.Display.clears;
    mode = Mode::Setting; testClock += 100; draw(); testClock += 100; draw();
    mode = Mode::Enroll; testClock += 100; draw(); testClock += 100; draw();
    bool menuOk = M5.Display.clears == clears;
    printf("%s: settings/enrollment never clear the physical display (clears=%u)\n", menuOk ? "PASS" : "FAIL", M5.Display.clears - clears);
    unsigned transfers = M5.Display.transfers;
    for (int i = 0; i < 30; ++i) { testClock += 100; draw(); }
    bool stableOk = M5.Display.transfers == transfers;
    printf("%s: unchanged enrollment screen transfers zero frames over 3 seconds\n", stableOk ? "PASS" : "FAIL");
    notice = "retry"; testClock += 100; draw();
    bool noticeOk = M5.Display.transfers == transfers + 1;
    printf("%s: changed enrollment notice redraws exactly once\n", noticeOk ? "PASS" : "FAIL");
    transfers = M5.Display.transfers;
    wake.count = 1; testClock += 100; draw(); testClock += 100; draw();
    bool progressOk = M5.Display.transfers == transfers + 1;
    printf("%s: enrollment count changes redraw exactly once\n", progressOk ? "PASS" : "FAIL");
    transfers = M5.Display.transfers;
    mode = Mode::Ready; testClock += 100; draw();
    mode = Mode::Enroll; testClock += 100; draw();
    bool returnOk = M5.Display.transfers >= transfers + 2;
    printf("%s: returning from avatar redraws the menu\n", returnOk ? "PASS" : "FAIL");
    mode = Mode::Recording; testClock += 100; draw();
    bool transitionOk = !M5.Display.text.empty();
    mode = Mode::Ready; testClock += 100; draw();
    transitionOk = transitionOk && M5.Display.text.empty();
    printf("%s: recording status remains visible and is cleared on return to Ready\n", transitionOk ? "PASS" : "FAIL");
    bool mappingOk = faceModeFor(Mode::Ready) == AvatarMode::Ready && faceModeFor(Mode::Sleep) == AvatarMode::Sleep &&
        faceModeFor(Mode::Cue) == AvatarMode::Listening && faceModeFor(Mode::Recording) == AvatarMode::Listening &&
        faceModeFor(Mode::Busy) == AvatarMode::Thinking && faceModeFor(Mode::Speaking) == AvatarMode::Speaking;
    printf("%s: firmware modes select the correct face\n", mappingOk ? "PASS" : "FAIL");
    const AvatarMode faces[] = {AvatarMode::Sleep, AvatarMode::Ready, AvatarMode::Listening, AvatarMode::Thinking, AvatarMode::Speaking};
    const char* names[] = {"Sleep", "Ready", "Listening", "Thinking", "Speaking"};
    std::ofstream preview;
    if (argc > 1) { preview.open(argv[1]); preview << "["; }
    std::set<std::string> shapes;
    std::string readyShape;
    for (int i = 0; i < 5; ++i) {
        g_avatar.setMode(faces[i]); g_avatar.setStatus("");
        g_avatar.setLipSyncLevel(i == 4 ? 0.8f : 0);
        for (int n = 0; n < 20; ++n) { testClock += 100; g_avatar.update(); }
        shapes.insert(M5.Display.primitives);
        if (i == 1) readyShape = M5.Display.primitives;
        if (preview.is_open()) preview << (i ? "," : "") << "{\"mode\":\"" << names[i] << "\",\"commands\":\"" << M5.Display.primitives << "\"}";
    }
    if (preview.is_open()) { preview << "]"; preview.close(); }
    bool distinctOk = shapes.size() == 5;
    printf("%s: five modes produce five distinct actual drawing outputs\n", distinctOk ? "PASS" : "FAIL");
    g_avatar.setMode(AvatarMode::Thinking); testClock += 100; g_avatar.update();
    g_avatar.setMode(AvatarMode::Ready); testClock += 100; g_avatar.update();
    bool recoveryOk = M5.Display.primitives == readyShape;
    printf("%s: Thinking to Ready resets gaze, blink and mouth\n", recoveryOk ? "PASS" : "FAIL");
    g_avatar.setMode(AvatarMode::Speaking); g_avatar.setLipSyncLevel(0);
    for (int n = 0; n < 20; ++n) { testClock += 100; g_avatar.update(); }
    auto quiet = M5.Display.primitives;
    g_avatar.setLipSyncLevel(1);
    for (int n = 0; n < 20; ++n) { testClock += 100; g_avatar.update(); }
    bool lipOk = M5.Display.primitives != quiet;
    printf("%s: Speaking mouth responds to playback level\n", lipOk ? "PASS" : "FAIL");
    sleepNow(); transfers = M5.Display.transfers; testClock += 100; draw();
    bool dimSleepOk = mode == Mode::Sleep && M5.Display.brightness > 0 && M5.Display.brightness < 100 &&
        M5.Display.text.empty() && M5.Display.transfers == transfers;
    printf("%s: Sleep draws a dim sleeping face once and keeps text hidden\n", dimSleepOk ? "PASS" : "FAIL");
    AudioTask::used=320; AudioTask::recording=true;
    beginQuestion();
    bool cueFlowOk=mode==Mode::Cue && AudioTask::playing && !AudioTask::recording && AudioTask::used==0;
    serviceQuestionCue(); cueFlowOk &= mode==Mode::Cue && !AudioTask::recording;
    AudioTask::playing=false; serviceQuestionCue();
    cueFlowOk &= mode==Mode::Recording && AudioTask::recording;
    AudioTask::cueOk=false; beginQuestion();
    cueFlowOk &= mode==Mode::Ready && !AudioTask::recording;
    AudioTask::cueOk=true; AudioTask::available=false; beginQuestion();
    AudioTask::playing=false; serviceQuestionCue();
    cueFlowOk &= mode==Mode::Ready && !AudioTask::recording;
    AudioTask::available=true;
    printf("%s: wake clears stale audio, waits for cue before recording and recovers from cue/microphone errors\n", cueFlowOk ? "PASS" : "FAIL");
    mode = Mode::Enroll; wake.count = wake.calls = 0;
    AudioTask::cues = AudioTask::calibrations = 0; AudioTask::playing = false;
    bool enrollmentOk = prepareEnrollment();
    for (int result : {1, 1, 2}) {
        unsigned before=wake.calls, beforeCue=AudioTask::cues;
        for(int i=0;i<50;++i) serviceEnrollment();
        enrollmentOk &= wake.calls==before && AudioTask::cues==beforeCue+1 && enrollmentPhase==EnrollmentPhase::Cue;
        serviceEnrollment(); enrollmentOk &= wake.calls==before;
        AudioTask::playing=false; serviceEnrollment();
        enrollmentOk &= enrollmentPhase==EnrollmentPhase::Listening && wake.calls==before;
        for(int i=0;i<249;++i) serviceEnrollment();
        enrollmentOk &= enrollmentPhase==EnrollmentPhase::Listening && wake.calls==before;
        serviceEnrollment();
        enrollmentOk &= enrollmentPhase==EnrollmentPhase::Analyzing && wake.calls==before;
        wake.next=result;
        for(int i=0;i<150 && enrollmentPhase==EnrollmentPhase::Analyzing && mode==Mode::Enroll;++i) serviceEnrollment();
        enrollmentOk &= wake.calls==before+1;
    }
    enrollmentOk &= mode==Mode::Enroll && enrollmentPhase==EnrollmentPhase::VerificationCalibration &&
        wake.candidate && wake.commits==0 && AudioTask::cues==3 && AudioTask::calibrations==4;
    printf("%s: each take measures background, cues, records all 250 frames then crops/stages; no post-cue calibration\n", enrollmentOk ? "PASS" : "FAIL");
    for(int i=0;i<60;++i) serviceEnrollment();
    for(int i=0;i<49;++i) serviceEnrollment();
    bool verificationDisplayOk = enrollmentPhase==EnrollmentPhase::VerificationWarmup && wake.verificationFrames==49;
    serviceEnrollment(); testClock+=100; draw();
    verificationDisplayOk &= enrollmentPhase==EnrollmentPhase::Verifying && wake.commits==0 &&
        M5.Display.text.find("同じ呼びかけを1回")!=std::string::npos;
    wake.match=true;
    for(int i=0;i<250;++i) serviceEnrollment();
    testClock+=100; draw();
    verificationDisplayOk &= mode==Mode::Enroll && enrollmentPhase==EnrollmentPhase::Saved && wake.commits==1 &&
        M5.Display.text.find("登録できました")!=std::string::npos && M5.Display.text.find("保存しました")!=std::string::npos;
    testClock+=3000; serviceEnrollment(); draw();
    verificationDisplayOk &= mode==Mode::Ready && M5.Display.text.empty();
    printf("%s: candidate background, explicit prompt and saved result are visible before the normal text-free Ready face returns\n", verificationDisplayOk ? "PASS" : "FAIL");
    bool rejectedOk=true;
    for (bool silent : {true,false}) {
        mode=Mode::Enroll; wake.count=2; AudioTask::emitVoice=!silent;
        prepareEnrollment();
        for(int i=0;i<50;++i) serviceEnrollment();
        AudioTask::playing=false; serviceEnrollment();
        for(int i=0;i<250;++i) serviceEnrollment();
        wake.next=-1;
        for(int i=0;i<150 && mode==Mode::Enroll;++i) serviceEnrollment();
        unsigned cues=AudioTask::cues;
        rejectedOk &= mode==Mode::Setting && wake.count==0 && AudioTask::used==0 && !AudioTask::recording;
        for(int i=0;i<20;++i) { testClock+=50; draw(); }
        rejectedOk &= AudioTask::cues==cues;
    }
    AudioTask::emitVoice=true;
    printf("%s: empty take and third-take mismatch discard all pending audio and return to settings without an automatic cue loop\n", rejectedOk ? "PASS" : "FAIL");
    mode = Mode::Enroll; prepareEnrollment(); AudioTask::available = false;
    unsigned accepted = wake.count; serviceEnrollment();
    bool enrollmentFailureOk = mode == Mode::MicError && micResume == Mode::Enroll && wake.count == 0;
    AudioTask::available = true;
    printf("%s: enrollment microphone failure discards pending takes and exposes recovery\n", enrollmentFailureOk ? "PASS" : "FAIL");
    return readyOk && faceOnly && tapOk && noticeDeferred && sleepOk && menuOk && stableOk && noticeOk && progressOk && returnOk && transitionOk && mappingOk && distinctOk && recoveryOk && lipOk && dimSleepOk && enrollmentOk && verificationDisplayOk && rejectedOk && enrollmentFailureOk && cueFlowOk ? 0 : 1;
}
'''
    test = out / 'display.cpp'
    test.write_text(harness, encoding='utf-8')
    vswhere = pathlib.Path(r'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe')
    vs = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-property', 'installationPath'], text=True).strip()
    vcvars = pathlib.Path(vs) / 'VC/Auxiliary/Build/vcvars64.bat'
    exe = out / 'display.exe'
    object_dir = str(out) + chr(92) * 2
    command = f'"{vcvars}" >nul && cl /nologo /std:c++17 /utf-8 /EHsc /I"{ROOT / "tests/display_stubs"}" /I"{ROOT / "src"}" /Fe:"{exe}" /Fo:"{object_dir}" "{test}" "{ROOT / "src/StackChanAvatar.cpp"}"'
    # A saved script avoids nested Windows command-line quote processing.
    runner = out / 'build_display.ps1'
    runner.write_text("$buildCommand='" + command.replace("'", "''") + "'\n& cmd.exe /d /s /c $buildCommand\nexit $LASTEXITCODE\n", encoding='utf-8-sig')
    compiled = subprocess.run(['powershell.exe', '-NoProfile', '-File', str(runner)])
    if compiled.returncode:
        return compiled.returncode
    return subprocess.run([str(exe), str(out / 'mode_faces.json')]).returncode


if __name__ == '__main__':
    sys.exit(main())
