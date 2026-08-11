// Standalone check for the action trace ring (wrap/order/JSONL-free).
// Build: cl /nologo /std:c++14 tests\action_trace_tests.cpp /Fe:action_trace_tests.exe
#include "../el_native/action_trace.h"
#include <cstdio>
#include <cstring>

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); failures++; } } while (0)

int main() {
    // Push more than ring capacity; verify oldest-first order and wrap.
    const int total = actiontrace::kRing + 5;
    for (int i = 0; i < total; i++) {
        char buf[32];
        sprintf_s(buf, "event-%d", i);
        actiontrace::Push("test", "%s", buf);
    }
    static actiontrace::Event ev[actiontrace::kRing];
    int n = 0;
    actiontrace::Snapshot(ev, &n);
    CHECK(n == actiontrace::kRing, "ring caps at capacity");
    CHECK(strcmp(ev[0].text, "event-5") == 0, "oldest kept is first after wrap");
    CHECK(strcmp(ev[n - 1].text, "event-68") == 0, "newest is last");
    CHECK(strcmp(ev[0].source, "test") == 0, "source preserved");
    CHECK(ev[0].tick > 0, "tick set");
    // JSONL file written in cwd
    FILE* f = fopen("el_native_action_trace.jsonl", "rb");
    CHECK(f != nullptr, "jsonl file created");
    if (f) {
        fseek(f, 0, SEEK_END);
        CHECK(ftell(f) > 0, "jsonl non-empty");
        fclose(f);
    }
    if (failures == 0) {
        printf("action_trace_tests: ALL PASS\n");
        return 0;
    }
    printf("action_trace_tests: %d FAILURES\n", failures);
    return 1;
}
