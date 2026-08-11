#pragma once
// Pure parser for el_proxy.log lines ([SPIN]/[ROULETTE]/[UPDATE]/[ERR]).
// No DLL/ImGui deps — unit-testable standalone.

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace proxylog {

struct Card { int idx = 0; char spec[160] = {}; bool bm = false; };
struct ActionLine { char ts[16] = {}; char text[320] = {}; int kind = 0; };

struct State {
    Card cards[4];
    int cardCount = 0;
    bool cardsFresh = false;
    char spinTs[16] = {};
    bool spinBm = false;
    ActionLine actions[6];
    int actionHead = 0;
    int actionCount = 0;
    char errTs[16] = {};
    char errText[320] = {};
};

inline void Trim(char* s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\r')) s[--n] = 0;
}

inline void PushAction(State& st, int kind, const char* ts, const char* text) {
    ActionLine& a = st.actions[st.actionHead];
    a.kind = kind;
    strncpy_s(a.ts, ts, _TRUNCATE);
    strncpy_s(a.text, text, _TRUNCATE);
    st.actionHead = (st.actionHead + 1) % 6;
    if (st.actionCount < 6) st.actionCount++;
}

inline void ParseLine(State& st, char* line) {
    if (line[0] != '[') return;
    char* close1 = strchr(line, ']');
    if (!close1) return;
    char ts[16] = {};
    size_t tlen = (size_t)(close1 - line - 1);
    if (tlen >= sizeof(ts)) tlen = sizeof(ts) - 1;
    memcpy(ts, line + 1, tlen);
    ts[tlen] = 0;
    char* p = close1 + 1;
    while (*p == ' ') p++;

    if (strncmp(p, "[SPIN]", 6) == 0) {
        p += 6;
        while (*p == ' ') p++;
        if (strstr(p, "cards dealt:")) {
            st.cardCount = 0;
            st.cardsFresh = true;
            strncpy_s(st.spinTs, ts, _TRUNCATE);
        } else if (strstr(p, "no rewards")) {
            st.cardCount = 0;
            st.cardsFresh = false;
            strncpy_s(st.spinTs, ts, _TRUNCATE);
        } else if (strstr(p, "is_black_mark=true")) {
            st.spinBm = true;
            strncpy_s(st.spinTs, ts, _TRUNCATE);
        } else if (strstr(p, "is_black_mark=false")) {
            st.spinBm = false;
            strncpy_s(st.spinTs, ts, _TRUNCATE);
        }
        return;
    }

    if (st.cardsFresh && p[0] == '[' && p[1] >= '1' && p[1] <= '9' && st.cardCount < 4) {
        char* close2 = strchr(p, ']');
        if (close2) {
            Card c;
            c.idx = atoi(p + 1);
            char* body = close2 + 1;
            while (*body == ' ') body++;
            char tmp[160] = {};
            size_t n = 0;
            for (char* q = body; *q && *q != '<' && n < sizeof(tmp) - 1; q++) tmp[n++] = *q;
            while (n > 0 && tmp[n - 1] == ' ') n--;
            tmp[n] = 0;
            strncpy_s(c.spec, tmp, _TRUNCATE);
            c.bm = strstr(body, "<<BM>>") != nullptr;
            st.cards[st.cardCount++] = c;
            if (c.bm) st.spinBm = true;
        }
        return;
    }

    if (strncmp(p, "[ROULETTE]", 10) == 0) {
        char* b = p + 10;
        while (*b == ' ') b++;
        if (strstr(b, "passing through untouched")) PushAction(st, 1, ts, b);
        else if (strstr(b, "picked card")) PushAction(st, 2, ts, b);
        else if (strstr(b, "(was ")) PushAction(st, 3, ts, b);
        else PushAction(st, 0, ts, b);
        return;
    }
    if (strncmp(p, "[UPDATE]", 8) == 0) {
        char* b = p + 8;
        while (*b == ' ') b++;
        PushAction(st, 4, ts, b);
        return;
    }
    if (strncmp(p, "[ERR]", 5) == 0) {
        char* b = p + 5;
        while (*b == ' ') b++;
        strncpy_s(st.errTs, ts, _TRUNCATE);
        strncpy_s(st.errText, b, _TRUNCATE);
        return;
    }
}

inline void Parse(State& st, char* data, size_t len, bool partialStart) {
    char* line = data;
    char* end = data + len;
    if (partialStart) {
        char* nl = (char*)memchr(line, '\n', (size_t)(end - line));
        if (!nl) return;
        line = nl + 1;
    }
    while (line < end) {
        char* nl = (char*)memchr(line, '\n', (size_t)(end - line));
        char* le = nl ? nl : end;
        char saved = *le;
        *le = 0;
        if (line[0] != '\n' && line[0] != 0) {
            Trim(line);
            if (line[0]) ParseLine(st, line);
        }
        *le = saved;
        if (!nl) break;
        line = nl + 1;
    }
}

} // namespace proxylog
