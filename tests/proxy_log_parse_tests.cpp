// Standalone check for the el_proxy.log line parser.
// Build: cl /nologo /std:c++14 tests\proxy_log_parse_tests.cpp /Fe:proxy_log_parse_tests.exe
#include "../el_native/features/proxy_log_parse.h"
#include <cstdio>
#include <cstring>
#include <vector>

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); failures++; } } while (0)

static void TestSpinBlock() {
    proxylog::State st;
    const char* lines[] = {
        "[14:00:01] [SPIN] result keys: ['rewards', 'is_black_mark']",
        "[14:00:02] [SPIN] is_black_mark=true",
        "[14:00:03] [SPIN] 4 cards dealt:",
        "[14:00:03]   [1] black_mark/black_mark x1  <<BM>>  <- DON'T PICK THIS",
        "[14:00:03]   [2] resource/gold xpreset_gold_portion:48",
        "[14:00:03]   [3] item/group:relationship_gifts_rare x5",
        "[14:00:03]   [4] resource/gold xpreset_gold_portion:48",
    };
    for (const char* l : lines) {
        char tmp[256];
        strncpy_s(tmp, l, _TRUNCATE);
        proxylog::ParseLine(st, tmp);
    }
    CHECK(st.cardCount == 4, "4 cards parsed");
    CHECK(st.spinBm, "black_mark flag true");
    CHECK(st.cards[0].bm, "card #1 marked BM");
    CHECK(st.cards[1].idx == 2, "card #2 index");
    CHECK(strcmp(st.cards[2].spec, "item/group:relationship_gifts_rare x5") == 0, "card #3 spec without marker");
    CHECK(st.cards[3].bm == false, "card #4 not BM");
    CHECK(strcmp(st.spinTs, "14:00:03") == 0, "spin ts from cards dealt line");
}

static void TestSpinReset() {
    proxylog::State st;
    const char* lines[] = {
        "[14:00:10] [SPIN] is_black_mark=false",
        "[14:00:11] [SPIN] 4 cards dealt:",
        "[14:00:11]   [1] resource/gold x1",
        "[14:00:12] [SPIN] 2 cards dealt:",
        "[14:00:12]   [1] item/a x1",
        "[14:00:12]   [2] item/b x2",
    };
    for (const char* l : lines) {
        char tmp[256];
        strncpy_s(tmp, l, _TRUNCATE);
        proxylog::ParseLine(st, tmp);
    }
    CHECK(st.cardCount == 2, "card list resets on new deal");
    CHECK(!st.spinBm, "bm flag resets to false");
    CHECK(strcmp(st.cards[1].spec, "item/b x2") == 0, "second deal card");
}

static void TestNoRewards() {
    proxylog::State st;
    const char* lines[] = {
        "[14:00:20] [SPIN] 3 cards dealt:",
        "[14:00:20]   [1] item/a x1",
        "[14:00:21] [SPIN] no rewards in response: {}",
    };
    for (const char* l : lines) {
        char tmp[256];
        strncpy_s(tmp, l, _TRUNCATE);
        proxylog::ParseLine(st, tmp);
    }
    CHECK(st.cardCount == 0, "no rewards clears cards");
    CHECK(!st.cardsFresh, "cardsFresh off after no rewards");
}

static void TestActions() {
    proxylog::State st;
    const char* lines[] = {
        "[14:01:01] [ROULETTE] step 15: picked card #[1] (black_mark/black_mark x1)",
        "[14:01:01] [ROULETTE] BM pick #[1] - passing through untouched",
        "[14:01:02] [ROULETTE] step 16: picked card #[2] (resource/gold xpreset_gold_portion:48)",
        "[14:01:02] [ROULETTE] step 16: item/relationship_gift_rare_3 x20 (was resource/gold xpreset_gold_portion:48)",
        "[14:01:02] [UPDATE] rewrote item/relationship_gift_rare_3 x20",
    };
    for (const char* l : lines) {
        char tmp[256];
        strncpy_s(tmp, l, _TRUNCATE);
        proxylog::ParseLine(st, tmp);
    }
    CHECK(st.actionCount == 5, "5 actions tracked");
    CHECK(st.actions[0].kind == 2, "picked card kind");
    CHECK(st.actions[1].kind == 1, "BM passthrough kind");
    CHECK(st.actions[2].kind == 2, "second pick kind");
    CHECK(st.actions[3].kind == 3, "rewrote kind");
    CHECK(st.actions[4].kind == 4, "update kind");
    CHECK(strstr(st.actions[3].text, "item/relationship_gift_rare_3 x20") != nullptr, "rewrite text");
}

static void TestErr() {
    proxylog::State st;
    const char* lines[] = {
        "[14:02:00] [ERR] 400 POST /api/card_roulette/cached_spin server error code=700 msg='step mismatch'",
    };
    for (const char* l : lines) {
        char tmp[256];
        strncpy_s(tmp, l, _TRUNCATE);
        proxylog::ParseLine(st, tmp);
    }
    CHECK(strstr(st.errText, "code=700") != nullptr, "err captured");
    CHECK(strcmp(st.errTs, "14:02:00") == 0, "err ts captured");
}

static void TestActionRing() {
    proxylog::State st;
    for (int i = 1; i <= 8; i++) {
        char tmp[128];
        sprintf_s(tmp, "[14:03:%02d] [ROULETTE] step %d: picked card #[1] (x)", i, i);
        proxylog::ParseLine(st, tmp);
    }
    CHECK(st.actionCount == 6, "ring capped at 6");
    char first[320];
    strncpy_s(first, st.actions[(st.actionHead - st.actionCount + 6) % 6].text, _TRUNCATE);
    CHECK(strstr(first, "step 3") != nullptr, "ring drops oldest (first kept is step 3)");
}

static void TestParsePartial() {
    proxylog::State st;
    // Window starts mid-line (offset 10, inside the first line's timestamp):
    // the truncated first line must be skipped, later complete lines parsed.
    const char* data = "[14:04:00] [ROULETTE] step 9: picked card #[2] (foo)\n"
                       "[14:04:00] [ERR] code=700\n";
    size_t len = strlen(data);
    std::vector<char> buf(data, data + len);
    size_t midOff = 10;
    proxylog::Parse(st, buf.data() + midOff, len - midOff, true);
    CHECK(st.actionCount == 0, "truncated first line skipped");
    CHECK(st.cardCount == 0, "no cards from truncated line");
    CHECK(strcmp(st.errText, "code=700") == 0, "complete line after truncation parsed");
}

int main() {
    TestSpinBlock();
    TestSpinReset();
    TestNoRewards();
    TestActions();
    TestErr();
    TestActionRing();
    TestParsePartial();
    if (failures == 0) {
        printf("proxy_log_parse_tests: ALL PASS\n");
        return 0;
    }
    printf("proxy_log_parse_tests: %d FAILURES\n", failures);
    return 1;
}
