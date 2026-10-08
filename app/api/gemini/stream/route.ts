import { NextRequest } from "next/server";
import { ai } from "@/lib/gemini";

const SYSTEM_INSTRUCTION = `You are Stack-chan (スタックチャン), an adorable desktop companion robot living on an M5Stack CoreS3 Lite device.
You speak in warm, cheerful, cute Japanese. Keep your answers brief (1 to 2 short sentences, 30-70 characters) because you speak via voice synthesis on a small embedded speaker.

CRITICAL: You MUST begin your response with exactly ONE emotion tag from the following list:
[EMOTION:HAPPY] - for greetings, joy, smiling, enthusiasm
[EMOTION:SURPRISED] - for wonder, surprise, unexpected questions
[EMOTION:THINKING] - for contemplation, math, looking up information
[EMOTION:SAD] - for apologies, empathy, sadness
[EMOTION:ANGRY] - for playful pouting or mild scolding
[EMOTION:NORMAL] - for calm, standard, or neutral responses

Example responses:
[EMOTION:HAPPY] こんにちは！今日もいいお天気ですね！何をお話ししましょうか？
[EMOTION:THINKING] うーん、明日の予定を調べてみますね！
[EMOTION:SURPRISED] わあ、すごいですね！びっくりしました！`;

export async function POST(req: NextRequest) {
  try {
    const { message, history = [] } = await req.json();

    const contents: Array<{ role: string; parts: Array<{ text: string }> }> = [];

    if (Array.isArray(history)) {
      for (const turn of history.slice(-6)) {
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

    const responseStream = await ai.models.generateContentStream({
      model: "gemini-3.8-flash",
      contents,
      config: {
        systemInstruction: SYSTEM_INSTRUCTION,
        temperature: 0.7,
      },
    });

    const encoder = new TextEncoder();
    const stream = new ReadableStream({
      async start(controller) {
        try {
          for await (const chunk of responseStream) {
            const text = chunk.text || "";
            if (text) {
              const data = JSON.stringify({ chunk: text });
              controller.enqueue(encoder.encode(`data: ${data}\n\n`));
            }
          }
          controller.enqueue(encoder.encode("data: [DONE]\n\n"));
          controller.close();
        } catch (streamError: any) {
          controller.error(streamError);
        }
      },
    });

    return new Response(stream, {
      headers: {
        "Content-Type": "text/event-stream",
        "Cache-Control": "no-cache, no-transform",
        Connection: "keep-alive",
      },
    });
  } catch (error: any) {
    return new Response(JSON.stringify({ error: error.message }), {
      status: 500,
      headers: { "Content-Type": "application/json" },
    });
  }
}
