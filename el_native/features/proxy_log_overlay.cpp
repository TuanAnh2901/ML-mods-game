#include "proxy_log_overlay.h"
#include "proxy_log_parse.h"
#include "../overlay_pos.h"
#include "../framework.h"
#include "../config_registry.h"
#include "../feature.h"
#include "../../third_party/imgui/imgui.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
static std::string logPathOverride;
static std::string resolvedPath;
static bool haveLog = false;
static LONGLONG lastSize = 0;
static proxylog::State st;
static OverlayPos s_pos = { "proxylog.window" };

static void ResolvePath() {
    if (!logPathOverride.empty()) {
        resolvedPath = logPathOverride;
        return;
    }
    char exe[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    char* slash = strrchr(exe, '\\');
    if (slash) *slash = 0;
    resolvedPath = std::string(exe) + "\\el_proxy.log";
}

static void ReadTail() {
    HANDLE h = CreateFileA(resolvedPath.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        haveLog = false;
        return;
    }
    LARGE_INTEGER sz = {};
    GetFileSizeEx(h, &sz);
    if (sz.QuadPart < lastSize) lastSize = 0;
    if (sz.QuadPart == lastSize) {
        CloseHandle(h);
        return;
    }
    haveLog = true;
    const size_t BUF = 1u << 16;
    std::vector<char> buf(BUF);
    LONGLONG start = lastSize;
    LONGLONG span = sz.QuadPart - lastSize;
    if (span > (LONGLONG)BUF) {
        start = sz.QuadPart - (LONGLONG)BUF;
        span = BUF;
    }
    LARGE_INTEGER off = {};
    off.QuadPart = start;
    SetFilePointerEx(h, off, nullptr, FILE_BEGIN);
    DWORD got = 0;
    bool partialStart = false;
    if (start > 0) {
        // Window starts mid-file; if the byte before it is not a newline, the
        // first line is truncated and must be skipped.
        LARGE_INTEGER prev = {};
        prev.QuadPart = start - 1;
        SetFilePointerEx(h, prev, nullptr, FILE_BEGIN);
        char c = 0;
        DWORD gotPrev = 0;
        partialStart = !(ReadFile(h, &c, 1, &gotPrev, nullptr) && gotPrev == 1 && c == '\n');
        SetFilePointerEx(h, off, nullptr, FILE_BEGIN);
    }
    if (ReadFile(h, buf.data(), (DWORD)span, &got, nullptr) && got > 0) {
        proxylog::Parse(st, buf.data(), got, partialStart);
    }
    CloseHandle(h);
    lastSize = sz.QuadPart;
}

static void RenderContent() {
    using proxylog::Card;
    using proxylog::ActionLine;
    ImGui::Text("log: %s", resolvedPath.c_str());
    if (!haveLog) {
        ImGui::TextColored(ImVec4(1, 0.6f, 0.2f, 1), "el_proxy.log not found");
        return;
    }
    ImGui::SeparatorText("Latest spin");
    if (st.spinTs[0]) {
        ImGui::Text("time %s   black_mark=%s", st.spinTs, st.spinBm ? "YES" : "no");
    } else {
        ImGui::Text("no spin yet");
    }
    if (st.cardCount == 0) {
        ImGui::Text("(no cards)");
    }
    for (int i = 0; i < st.cardCount; i++) {
        const Card& c = st.cards[i];
        if (c.bm) {
            ImGui::TextColored(ImVec4(1, 0.2f, 0.2f, 1), "[%d] %s  <<BM>> <- DON'T PICK", c.idx, c.spec);
        } else {
            ImGui::Text("[%d] %s", c.idx, c.spec);
        }
    }
    ImGui::Text("Actions (last %d):", st.actionCount);
    for (int i = 0; i < st.actionCount; i++) {
        const ActionLine& a = st.actions[(st.actionHead - st.actionCount + i + 6) % 6];
        if (a.kind == 3) {
            ImGui::TextColored(ImVec4(0.3f, 1, 0.4f, 1), "[%s] %s", a.ts, a.text);
        } else if (a.kind == 1) {
            ImGui::TextColored(ImVec4(1, 0.85f, 0.3f, 1), "[%s] %s", a.ts, a.text);
        } else {
            ImGui::Text("[%s] %s", a.ts, a.text);
        }
    }
    if (st.errText[0]) {
        ImGui::SeparatorText("Last error");
        ImGui::TextColored(ImVec4(1, 0.2f, 0.2f, 1), "[%s] %s", st.errTs, st.errText);
    }
}
}

ProxyLogOverlayFeature::ProxyLogOverlayFeature() {
    name = "Roulette Proxy Log";
    enabled = false;
}
void ProxyLogOverlayFeature::Init() {
    GlobalConfigRegistry().RegisterString("proxy.log_path", &logPathOverride);
    s_pos.Register();
    ResolvePath();
    LOG("[PROXYLOG] monitor path=%s", resolvedPath.c_str());
}
void ProxyLogOverlayFeature::OnUpdate() {
    static std::string lastOverride;
    if (lastOverride != logPathOverride) {
        lastOverride = logPathOverride;
        ResolvePath();
        lastSize = 0;
    }
    ReadTail();
}
void ProxyLogOverlayFeature::OnMenu() {
    static char path[520] = {};
    static bool initialized = false;
    static std::string lastPath;
    if (!initialized || lastPath != logPathOverride) {
        strncpy_s(path, logPathOverride.c_str(), _TRUNCATE);
        lastPath = logPathOverride;
        initialized = true;
    }
    if (ImGui::InputText("Proxy log path (empty = game dir)", path, sizeof(path))) {
        logPathOverride = path;
        ConfigMarkDirty();
    }
    ImGui::TextWrapped("Reads el_proxy.log written by el_proxy.py (auto-detected next to the game exe, or set an explicit path). "
                       "Shows the latest card roulette spin, rewrites and server errors without tabbing out.");
    ImGui::Separator();
    ProxyLogOverlayRender();
}
void ProxyLogOverlayRender() {
    ImGui::BeginChild("proxy_log_view", ImVec2(0, 0), true);
    RenderContent();
    ImGui::EndChild();
}
void ProxyLogOverlayFeature::OnOverlay() {
    s_pos.Apply();
    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("Roulette Proxy", nullptr, ImGuiWindowFlags_NoSavedSettings);
    RenderContent();
    ImGui::End();
    s_pos.Save();
}

static ProxyLogOverlayFeature g_feature;
static int g_registered = (RegisterFeature(&g_feature), 0);
