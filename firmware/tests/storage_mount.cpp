#include "../src/LocalStorage.h"
#include <assert.h>
#include <stdio.h>
int main() {
    FFat.mountSucceeds = false;
    assert(!localstorage::begin());
    assert(FFat.mountCalls == 1 && !FFat.formatRequested);
    FFat.mountSucceeds = true;
    assert(localstorage::begin());
    assert(!FFat.formatRequested);
    File file = localstorage::filesystem().open("/test.bin", "w");
    const uint8_t data[] = {1, 2, 3};
    assert(file.write(data, sizeof(data)) == sizeof(data)); file.close();
    assert(localstorage::filesystem().exists("/test.bin"));
    puts("PASS: failed/successful mount never formats; common FS supports file operations");
}
