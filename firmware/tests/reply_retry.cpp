#include "../src/ReplyRetry.h"
#include <stdint.h>
#include <stdio.h>
#include <vector>
struct FakeTransport {
    std::vector<int> codes;
    unsigned calls = 0, retries = 0;
    uint32_t clock = 0, requestTime = 100, serverWait = 0, waited = 0, oversleep = 0;
    bool cancel = false, cancelDuringWait = false, validResponse = true;
    explicit FakeTransport(std::vector<int> statuses) : codes(statuses) {}
    bool attempt() { clock += requestTime; ++calls; return status() == 200 && validResponse; }
    int status() const { return codes[calls - 1]; }
    uint32_t nowMillis() const { return clock; }
    uint32_t retryWaitMs() const { return serverWait; }
    uint32_t jitterMs() const { return 250; }
    bool cancelled() const { return cancel; }
    void onRetry() { ++retries; }
    bool wait(uint32_t ms) { waited += ms; clock += ms + oversleep; if (cancelDuringWait) cancel = true; return !cancel; }
};
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); return 1; } } while (0)
int main() {
    FakeTransport recover({503, 200});
    CHECK(replyretry::execute(recover));
    CHECK(recover.calls == 2 && recover.retries == 1);
    CHECK(recover.waited >= 1000 && recover.waited <= 4000);
    FakeTransport persistent({503, 503, 200});
    CHECK(!replyretry::execute(persistent) && persistent.calls == 2);
    for (int status : {400, 401, 403, 404, 429, 500, 0, -11}) {
        FakeTransport rejected({status, 200});
        CHECK(!replyretry::execute(rejected) && rejected.calls == 1 && rejected.retries == 0);
    }
    FakeTransport cancelled({503, 200}); cancelled.cancelDuringWait = true;
    CHECK(!replyretry::execute(cancelled) && cancelled.calls == 1);
    FakeTransport alreadyCancelled({503}); alreadyCancelled.cancel = true;
    CHECK(!replyretry::execute(alreadyCancelled) && alreadyCancelled.calls == 0);
    FakeTransport longWait({503, 200}); longWait.serverWait = 60000;
    CHECK(!replyretry::execute(longWait) && longWait.calls == 1 && longWait.waited == 0);
    FakeTransport serverWait({503, 200}); serverWait.serverWait = 2500;
    CHECK(replyretry::execute(serverWait) && serverWait.waited >= 2500);
    FakeTransport slow({503, 200}); slow.requestTime = 14000;
    CHECK(!replyretry::execute(slow) && slow.calls == 1);
    FakeTransport delayed({503, 200}); delayed.oversleep = 14000;
    CHECK(!replyretry::execute(delayed) && delayed.calls == 1);
    FakeTransport waitAtCap({503, 200}); waitAtCap.serverWait = 4000;
    CHECK(!replyretry::execute(waitAtCap) && waitAtCap.calls == 1);
    FakeTransport malformed({200}); malformed.validResponse = false;
    CHECK(!replyretry::execute(malformed) && malformed.calls == 1);
    FakeTransport normal({200}); CHECK(replyretry::execute(normal) && normal.calls == 1 && normal.retries == 0);
    FakeTransport wrapped({503, 200}); wrapped.clock = 0xffffff00u;
    CHECK(replyretry::execute(wrapped) && wrapped.calls == 2);
    puts("PASS: reply 503 recovery, bounded attempts/wait, Retry-After, cancellation, status filters, elapsed budget, timer wrap");
}
