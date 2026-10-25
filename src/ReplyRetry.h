#pragma once
#include <stdint.h>

namespace replyretry {
constexpr unsigned MaxAttempts = 2;
constexpr uint32_t RetryStartBudgetMs = 15000, MinAttemptRoomMs = 2000, MaxAutomaticWaitMs = 4000;
// A dedicated seam around the real reply transport; STT/TTS do not use it.
// This limits when a retry may START; the underlying HTTP timeout is separate.
template<class Transport> bool execute(Transport& transport) {
    const uint32_t started = transport.nowMillis();
    for (unsigned attempt = 0; attempt < MaxAttempts; ++attempt) {
        if (transport.cancelled()) return false;
        if (transport.attempt()) return true;
        if (transport.status() != 503 || attempt + 1 >= MaxAttempts || transport.cancelled()) return false;
        const uint32_t serverWait = transport.retryWaitMs();
        if (serverWait > MaxAutomaticWaitMs) return false;
        const uint32_t backoff = 1000u << attempt;
        const uint32_t wait = (serverWait > backoff ? serverWait : backoff) + transport.jitterMs() % 501;
        const uint32_t elapsed = transport.nowMillis() - started;
        if (wait > MaxAutomaticWaitMs || elapsed >= RetryStartBudgetMs ||
            RetryStartBudgetMs - elapsed < wait + MinAttemptRoomMs) return false;
        transport.onRetry();
        if (!transport.wait(wait)) return false;
        // A preempted task may resume after the retry-start window.
        if (transport.nowMillis() - started > RetryStartBudgetMs - MinAttemptRoomMs) return false;
    }
    return false;
}
}
