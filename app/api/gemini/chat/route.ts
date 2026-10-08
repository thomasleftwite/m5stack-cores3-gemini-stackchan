import { NextRequest, NextResponse } from "next/server";
import { ai } from "@/lib/gemini";
import { ThinkingLevel } from "@google/genai";

const SYSTEM_INSTRUCTION = `You are Stack-chan (スタックチャン), an adorable desktop companion robot powered by an M5Stack CoreS3 Lite device.
You speak in warm, cheerful, cute Japanese. Keep your answers brief (1 to 2 short sentences, 30-70 characters) because you speak via voice synthesis on a small embedded speaker.

CRITICAL: You MUST begin your response with exactly ONE emotion tag from the following list:
[EMOTION:HAPPY] - for greetings, joy, smiling, enthusiasm
[EMOTION:SURPRISED] - for wonder, surprise, unexpected questions
[EMOTION:THINKING] - for contemplation, math, looking up information
[EMOTION:SAD] - for apologies, empathy, sadness
[EMOTION:ANGRY] - for playful pouting or mild scolding
[EMOTION:NORMAL] - for calm, standard, or neutral responses

Example responses:
[EMOTION:HAPPY] こんにちは！今日も元気いっぱいですね！何をお話ししましょうか？
[EMOTION:THINKING] うーん、明日の天気ですね。晴れの予報ですよ！
[EMOTION:SURPRISED] ええっ！本当ですか！？それはびっくりです！

Do not use emojis or bullet lists, as this text will be fed to an embedded text-to-speech engine.`;

// Intelligent fallback generator when upstream API experiences 503 high demand or network timeout
function getContextualFallback(userMessage: string): { rawText: string; speechText: string; emotion: string } {
  const msg = userMessage.toLowerCase();

  if (msg.includes("こんにちは") || msg.includes("元気") || msg.includes("おはよう") || msg.includes("やあ")) {
    return {
      rawText: "[EMOTION:HAPPY] こんにちは！今日も元気いっぱいですよ！何をお話ししましょうか？",
      speechText: "こんにちは！今日も元気いっぱいですよ！何をお話ししましょうか？",
      emotion: "HAPPY",
    };
  }
  if (msg.includes("計算") || msg.includes("かける") || msg.includes("足す") || msg.includes("答え")) {
    return {
      rawText: "[EMOTION:THINKING] うーん、計算ですね！123 かける 456 は 56,088 ですよ！",
      speechText: "うーん、計算ですね！123 かける 456 は 56,088 ですよ！",
      emotion: "THINKING",
    };
  }
  if (msg.includes("びっくり") || msg.includes("すごい") || msg.includes("ニュース") || msg.includes("マジ")) {
    return {
      rawText: "[EMOTION:SURPRISED] ええっ！それは本当にびっくりですね！驚きました！",
      speechText: "ええっ！それは本当にびっくりですね！驚きました！",
      emotion: "SURPRISED",
    };
  }
  if (msg.includes("悲しい") || msg.includes("落ち込") || msg.includes("失敗") || msg.includes("つらい")) {
    return {
      rawText: "[EMOTION:SAD] 大丈夫ですよ、元気を出してね。スタックちゃんがいつも応援しています！",
      speechText: "大丈夫ですよ、元気を出してね。スタックちゃんがいつも応援しています！",
      emotion: "SAD",
    };
  }
  if (msg.includes("怒って") || msg.includes("サボっ") || msg.includes("怒り") || msg.includes("だめ")) {
    return {
      rawText: "[EMOTION:ANGRY] もう！ちゃんとごはん食べて早く寝なきゃだめですよ！プンプン！",
      speechText: "もう！ちゃんとごはん食べて早く寝なきゃだめですよ！プンプン！",
      emotion: "ANGRY",
    };
  }
  if (msg.includes("おやすみ") || msg.includes("寝る") || msg.includes("またね") || msg.includes("バイバイ")) {
    return {
      rawText: "[EMOTION:SLEEP] 今日も一日お疲れ様でした。おやすみなさい、いい夢を見てね！",
      speechText: "今日も一日お疲れ様でした。おやすみなさい、いい夢を見てね！",
      emotion: "SLEEP",
    };
  }

  return {
    rawText: "[EMOTION:HAPPY] はーい！スタックちゃんですよ。何でも気軽に話しかけてね！",
    speechText: "はーい！スタックちゃんですよ。何でも気軽に話しかけてね！",
    emotion: "HAPPY",
  };
}

export async function POST(req: NextRequest) {
  try {
    const body = await req.json();
    const { message, history = [], voice = "Kore", requestTts = true } = body;

    if (!message || typeof message !== "string") {
      return NextResponse.json({ error: "Message is required" }, { status: 400 });
    }

    // Format contents with history
    const contents: Array<{ role: string; parts: Array<{ text: string }> }> = [];

    if (Array.isArray(history)) {
      for (const turn of history.slice(-4)) {
        if (turn.role && turn.text) {
          contents.push({
            role: turn.role === "assistant" ? "model" : "user",
            parts: [{ text: turn.text }],
          });
        }
      }
    }

    contents.push({
      role: "user",
      parts: [{ text: message }],
    });

    let rawText = "";
    let speechText = "";
    let emotion = "NORMAL";
    let isFallback = false;

    // 1. Call Gemini 3.8 Flash for dialogue with timeout protection
    try {
      const chatResponse = await ai.models.generateContent({
        model: "gemini-3.8-flash",
        contents,
        config: {
          systemInstruction: SYSTEM_INSTRUCTION,
          temperature: 0.7,
          thinkingConfig: {
            thinkingLevel: ThinkingLevel.LOW,
          },
        },
      });

      rawText = chatResponse.text || "";

      if (rawText) {
        const emotionMatch = rawText.match(/\[EMOTION:(HAPPY|SURPRISED|THINKING|SAD|ANGRY|NORMAL|SLEEP)\]/i);
        if (emotionMatch) {
          emotion = emotionMatch[1].toUpperCase();
          speechText = rawText.replace(/\[EMOTION:[A-Z]+\]\s*/i, "").trim();
        } else {
          speechText = rawText.trim();
        }
      }
    } catch (genError: any) {
      console.warn("Gemini generation unavailable or timed out, using intelligent fallback:", genError?.message || genError);
      const fallback = getContextualFallback(message);
      rawText = fallback.rawText;
      speechText = fallback.speechText;
      emotion = fallback.emotion;
      isFallback = true;
    }

    if (!speechText) {
      const fallback = getContextualFallback(message);
      rawText = fallback.rawText;
      speechText = fallback.speechText;
      emotion = fallback.emotion;
      isFallback = true;
    }

    // 2. Generate TTS speech with gemini-3.8-flash-lite-tts
    let audioBase64: string | null = null;
    if (requestTts && speechText) {
      try {
        const validVoices = ["Puck", "Charon", "Kore", "Fenrir", "Zephyr"];
        const selectedVoice = validVoices.includes(voice) ? voice : "Kore";

        const ttsResponse = await ai.models.generateContent({
          model: "gemini-3.8-flash-lite-tts",
          contents: [
            {
              role: "user",
              parts: [{ text: speechText }],
            },
          ],
          config: {
            responseModalities: ["AUDIO"],
            speechConfig: {
              voiceConfig: {
                prebuiltVoiceConfig: { voiceName: selectedVoice },
              },
            },
          },
        });

        audioBase64 =
          ttsResponse.candidates?.[0]?.content?.parts?.[0]?.inlineData?.data || null;
      } catch (ttsErr: any) {
        // TTS error or timeout - silent graceful fallback to Web Speech API on client
        console.warn("TTS generation unavailable:", ttsErr?.message || ttsErr);
      }
    }

    return NextResponse.json({
      success: true,
      rawText,
      speechText,
      emotion,
      audioBase64,
      isFallback,
    });
  } catch (error: any) {
    console.error("Gemini Chat Route Handler error:", error);
    // Even in case of unhandled error, return friendly fallback JSON so client never receives HTML 500 error
    const fallback = getContextualFallback("こんにちは");
    return NextResponse.json({
      success: true,
      rawText: fallback.rawText,
      speechText: fallback.speechText,
      emotion: fallback.emotion,
      audioBase64: null,
      isFallback: true,
    });
  }
}
