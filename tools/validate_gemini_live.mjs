// Fixed synthetic fixtures only. Credentials are sent in a header, never in a URL/log.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import { performance } from 'node:perf_hooks';
const require = createRequire(import.meta.url);
const WebSocket = require('ws');
const root = fileURLToPath(new URL('../', import.meta.url));
// JSON parse/read errors can contain source fragments. Keep initialization inside
// this sanitized boundary as well as the asynchronous API response handlers.
try {
const mode = process.argv[2];
if (!['text', 'audio'].includes(mode)) { console.log('Usage: node tools/validate_gemini_live.mjs text|audio'); process.exit(2); }
const config = JSON.parse(fs.readFileSync(path.join(root, 'firmware/data/config.json'), 'utf8').replace(/^\uFEFF/, ''));
const key = config.gemini_api_key;
if (typeof key !== 'string' || !key.trim() || key.includes('YOUR_')) throw Error('LocalKeyUnavailable');
const question = '二足す三はいくつですか。';
const started = performance.now();
const seconds = from => Number(((performance.now() - from) / 1000).toFixed(3));
const stamp = new Intl.DateTimeFormat('sv-SE', { timeZone: 'Asia/Tokyo', year: 'numeric', month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit', second: '2-digit' }).format(new Date()).replace(/\D/g, '') + '-' + String(Date.now() % 1000);
const directory = path.join(root, 'docs/validation', 'live-' + stamp + '-' + mode);
fs.mkdirSync(directory, { recursive: true });
const report = { client_date: '2026-10-25', model: 'gemini-3.8-live', mode, scope: 'host WebSocket; fixed synthetic input; no microphone/device', production_setting_changed: false, websocket_open: false, setup_complete: false, turn_complete: false, interrupted: false, ok: false, audio_chunks: 0, audio_bytes: 0, events: 0 };
let done = false, inputAt = null, outputTranscript = '', inputTranscript = '', textParts = '';
let timer, grace, ws;
const audio = [];
function saysFive(text) {
  return /(?<![\d.])5(?:\.0+)?(?![\d.])|(?<![〇零一二三四五六七八九十百千万])五(?![〇零一二三四五六七八九十百千万])|いつつ|ご(?:です|だ|になる|になります|こ|個|つ|[。！!」』])/.test(text.normalize('NFKC'));
}
function wav(pcm) {
  const h = Buffer.alloc(44); h.write('RIFF'); h.writeUInt32LE(pcm.length + 36, 4); h.write('WAVEfmt ', 8);
  h.writeUInt32LE(16, 16); h.writeUInt16LE(1, 20); h.writeUInt16LE(1, 22); h.writeUInt32LE(24000, 24);
  h.writeUInt32LE(48000, 28); h.writeUInt16LE(2, 32); h.writeUInt16LE(16, 34); h.write('data', 36); h.writeUInt32LE(pcm.length, 40);
  return Buffer.concat([h, pcm]);
}
function syntheticPcm16() {
  const source = path.join(root, 'docs/validation/r13-api-20261025-210943');
  const previous = JSON.parse(fs.readFileSync(path.join(source, 'report.json'), 'utf8'));
  const expected = previous.stages.find(s => s.stage === 'fixture_tts' && s.ok).sha256;
  const bytes = fs.readFileSync(path.join(source, 'synthetic-question-24k.wav'));
  if (crypto.createHash('sha256').update(bytes).digest('hex') !== expected) throw Error('FixtureMismatch');
  if (bytes.toString('ascii', 0, 4) !== 'RIFF' || bytes.readUInt32LE(4) + 8 !== bytes.length) throw Error('InvalidFixture');
  let data = null, validFormat = false;
  for (let at = 12; at + 8 <= bytes.length;) {
    const size = bytes.readUInt32LE(at + 4); if (at + 8 + size > bytes.length) throw Error('InvalidFixture');
    const tag = bytes.toString('ascii', at, at + 4);
    if (tag === 'fmt ') validFormat = size >= 16 && bytes.readUInt16LE(at+8) === 1 && bytes.readUInt16LE(at+10) === 1 && bytes.readUInt32LE(at+12) === 24000 && bytes.readUInt16LE(at+22) === 16;
    if (tag === 'data') data = bytes.subarray(at+8, at+8+size);
    at += 8 + size + (size & 1);
  }
  if (!validFormat || !data || data.length % 2) throw Error('InvalidFixture');
  const count = data.length / 2, output = Buffer.alloc(Math.floor(count * 2 / 3) * 2);
  for (let i = 0; i < output.length / 2; ++i) {
    const at = i * 1.5, a = Math.floor(at), f = at - a;
    output.writeInt16LE(Math.round(data.readInt16LE(a*2) * (1-f) + data.readInt16LE(Math.min(a+1,count-1)*2) * f), i*2);
  }
  report.input_duration_seconds = output.length / 32000;
  report.input_pcm_bytes = output.length;
  return output;
}
function finish(errorKind) {
  if (done) return;
  done = true; clearTimeout(timer); clearTimeout(grace);
  if (errorKind) report.error_kind = errorKind;
  report.input_transcript_bytes = Buffer.byteLength(inputTranscript);
  report.output_transcript_bytes = Buffer.byteLength(outputTranscript);
  const heard = inputTranscript.normalize('NFKC').replaceAll('二', '2').replaceAll('三', '3');
  report.expected_question_match = mode === 'text' || (heard.includes('2') && heard.includes('3'));
  report.answer_source = outputTranscript ? 'output_audio_transcription' : 'model_text_part';
  report.expected_answer_five = saysFive(outputTranscript || textParts);
  report.audio_complete = report.turn_complete && !report.interrupted && report.audio_bytes > 0 && report.audio_bytes <= 960000 && report.audio_bytes % 2 === 0;
  if (report.audio_complete && !errorKind) {
    const file = wav(Buffer.concat(audio)); fs.writeFileSync(path.join(directory, 'synthetic-live-reply.wav'), file);
    report.wav_bytes = file.length; report.duration_seconds = report.audio_bytes / 48000;
    report.sha256 = crypto.createHash('sha256').update(file).digest('hex');
  }
  report.total_seconds = seconds(started);
  report.ok = !errorKind && report.setup_complete && report.audio_complete && report.expected_question_match && report.expected_answer_five;
  fs.writeFileSync(path.join(directory, 'report.json'), JSON.stringify(report, null, 2) + '\n');
  console.log(JSON.stringify({ ...report, report: path.join(directory, 'report.json') }));
  process.exitCode = report.ok ? 0 : 1;
  if (ws) ws.terminate();
}
function completeIfReady() {
  if (report.turn_complete && outputTranscript && (mode === 'text' || inputTranscript)) finish();
}
try {
  const input = mode === 'audio' ? syntheticPcm16() : null;
  ws = new WebSocket('wss://generativelanguage.googleapis.com/ws/google.ai.generativelanguage.v1beta.GenerativeService.BidiGenerateContent', {
    headers: { 'x-goog-api-key': key }, handshakeTimeout: 15000, maxPayload: 2000000,
  });
  timer = setTimeout(() => finish('SessionTimeout'), 20000);
  ws.on('open', () => {
    report.websocket_open = true; report.socket_open_seconds = seconds(started);
    const setup = { model: 'models/gemini-3.8-live', generationConfig: { responseModalities: ['AUDIO'], maxOutputTokens: 2048, speechConfig: { voiceConfig: { prebuiltVoiceConfig: { voiceName: config.tts_voice || 'Kore' } } } },
      systemInstruction: { parts: [{ text: 'あなたはスタックチャン。親しみやすい日本語の1～2文、60文字以内で質問に正確に答えてください。' }] }, outputAudioTranscription: {} };
    if (mode === 'audio') { setup.inputAudioTranscription = {}; setup.realtimeInputConfig = { automaticActivityDetection: { disabled: true } }; }
    // Gemini 3.8 Live does not support thinkingLevel. Omit it.
    ws.send(JSON.stringify({ setup }));
  });
  ws.on('message', bytes => {
    if (done) return;
    if (++report.events > 512) { finish('TooManyEvents'); return; }
    try {
      const message = JSON.parse(bytes.toString());
      if (message.error) {
        report.api_error_code = message.error.code;
        const error = String(message.error.message || '').toLowerCase();
        report.overload = /overload|high demand|capacity/.test(error);
        finish('LiveApiError'); return;
      }
      if (Object.hasOwn(message, 'setupComplete') && !report.setup_complete) {
        report.setup_complete = true; report.setup_seconds = seconds(started); inputAt = performance.now();
        if (mode === 'text') ws.send(JSON.stringify({ clientContent: { turns: [{ role: 'user', parts: [{ text: question }] }], turnComplete: true } }));
        else {
          ws.send(JSON.stringify({ realtimeInput: { activityStart: {} } }));
          ws.send(JSON.stringify({ realtimeInput: { audio: { data: input.toString('base64'), mimeType: 'audio/pcm;rate=16000' } } }));
          ws.send(JSON.stringify({ realtimeInput: { activityEnd: {} } }));
        }
      }
      const content = message.serverContent;
      if (content) {
        if (content.inputTranscription?.text) inputTranscript += content.inputTranscription.text;
        if (content.outputTranscription?.text) outputTranscript += content.outputTranscription.text;
        if (content.interrupted) report.interrupted = true;
        for (const part of content.modelTurn?.parts || []) {
          if (part.thought) continue;
          if (part.text) textParts += part.text;
          if (part.inlineData) {
            const type = part.inlineData.mimeType || '', encoded = part.inlineData.data || '';
            if (!/^audio\/(?:pcm|l16)(?:;.*)?$/.test(type) || (type.includes('rate=') && !type.includes('rate=24000')) || !/^[A-Za-z0-9+/]*={0,2}$/.test(encoded) || encoded.length % 4) throw Error('InvalidAudio');
            const chunk = Buffer.from(encoded, 'base64');
            if (chunk.toString('base64') !== encoded || chunk.length % 2) throw Error('InvalidAudio');
            if (!report.audio_bytes) report.first_audio_from_input_seconds = seconds(inputAt);
            audio.push(chunk); report.audio_bytes += chunk.length; ++report.audio_chunks;
            if (report.audio_bytes > 960000) throw Error('AudioTooLong');
          }
        }
        if (content.turnComplete) {
          report.turn_complete = true; report.complete_from_input_seconds = seconds(inputAt);
          grace = setTimeout(() => finish(), 750);
        }
      }
      if (message.usageMetadata) report.usage = Object.fromEntries(Object.entries(message.usageMetadata).filter(([k,v]) => k.endsWith('TokenCount') && Number.isInteger(v)));
      completeIfReady();
    } catch { finish('InvalidLiveResponse'); }
  });
  ws.on('unexpected-response', (_req, response) => { report.http_status = response.statusCode; response.resume(); finish('HandshakeRejected'); });
  ws.on('error', () => finish('WebSocketError'));
  ws.on('close', (code) => { if (!done) { report.close_code = code; finish('ClosedBeforeComplete'); } });
} catch (error) {
  finish(error?.message === 'FixtureMismatch' ? 'FixtureMismatch' : 'LocalSetupError');
}
} catch {
  console.log(JSON.stringify({ ok: false, error_kind: 'LocalSetupError' }));
  process.exitCode = 1;
}
