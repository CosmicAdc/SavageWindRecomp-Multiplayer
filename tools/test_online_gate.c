// The online version gate (runtime/online_gate.h): only 1.9.0 lets online play's fixed addresses be used.
#undef NDEBUG
#include <assert.h>
#include <stdio.h>

#include "online_gate.h"

int main(void) {
    assert(online_version_ok("1.9.0"));
    assert(!online_version_ok("1.8.2"));
    assert(!online_version_ok("1.9.0 "));
    assert(!online_version_ok("1.9"));
    assert(!online_version_ok(""));
    assert(!online_version_ok(NULL));
    printf("online gate: ok\n");
    return 0;
}
