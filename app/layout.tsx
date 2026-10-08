import type {Metadata} from 'next';
import './globals.css'; // Global styles

export const metadata: Metadata = {
  title: 'M5Stack CoreS3 Lite Gemini AI Voice Stack-chan',
  description: 'M5Stack CoreS3 Lite向けGemini音声チャットAI。スタックチャン感情アバター、低遅延ストリーミング、カメラ動体検知、非同期FreeRTOS最適化ファームウェアとリアルタイムWebエミュレータ。',
  openGraph: {
    title: 'M5Stack CoreS3 Lite Gemini AI Voice Stack-chan',
    description: 'M5Stack CoreS3 Lite向けGemini音声チャットAI。スタックチャン感情アバター、低遅延ストリーミング、カメラ動体検知、非同期FreeRTOS最適化ファームウェアとリアルタイムWebエミュレータ。',
    type: 'website',
  },
  twitter: {
    card: 'summary_large_image',
    title: 'M5Stack CoreS3 Lite Gemini AI Voice Stack-chan',
    description: 'M5Stack CoreS3 Lite向けGemini音声チャットAI。スタックチャン感情アバター、低遅延ストリーミング、カメラ動体検知、非同期FreeRTOS最適化ファームウェアとリアルタイムWebエミュレータ。',
  },
};

export default function RootLayout({children}: {children: React.ReactNode}) {
  return (
    <html lang="en">
      <body suppressHydrationWarning>{children}</body>
    </html>
  );
}
