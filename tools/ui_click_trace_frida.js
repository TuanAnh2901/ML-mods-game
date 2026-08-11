// Read-only UI click + reward-flow trace (Frida fallback to ActionTracer).
// il2cpp_resolve.js is prepended by run_ui_click_frida.py.
//
// Hooks:
//   - UGUIButtonListener.HandleClick   -> every click, resolved GameObject name
//   - MultichestWindow methods         -> reward flow around Open All
//   - MatchCompletedWindow methods     -> result window / claim flow
//   - Journey ClaimRewardsOnWin        -> claim signal
// Every hook emits raw ABI args (self, args..., methodInfo) so the flow can be
// reconstructed without game source.

var CLICK_CLASS = "UGUIVisual.UGUIButtonListener";
var MULTICHEST_CLASS = "UI_Scripts.WindowManager.MultichestWindow";
var MATCH_COMPLETED_CLASS = "AutoChess.UIScripts.WindowScripts.MatchCompleted.MatchCompletedWindow";
var JOURNEY_CLASS = "AutoChess.Journey.JourneyModuleABC.Service.ExternalJourneyFighter";

var hooked = {};
var lastEmitAt = {};
var MIN_INTERVAL_MS = typeof TRACE_MIN_INTERVAL_MS === "number" ? TRACE_MIN_INTERVAL_MS : 0;

function safeString(value) {
    try { return value.readUtf8String(); } catch (e) { return null; }
}
function ptrString(value) {
    try { return value.isNull() ? "0x0" : value.toString(); } catch (e) { return "<unreadable>"; }
}
function rvaString(address) {
    try { return "0x" + address.sub(ga.base).toString(16); } catch (e) { return null; }
}
function emit(kind, details) {
    details = details || {};
    details.event = kind;
    details.pid = Process.id;
    details.threadId = Process.getCurrentThreadId();
    details.tsMs = Date.now();
    send(details);
}
function emitThrottled(kind, details) {
    var now = Date.now();
    var last = lastEmitAt[kind] || 0;
    if (now - last < MIN_INTERVAL_MS) return;
    lastEmitAt[kind] = now;
    emit(kind, details);
}
function readUtf16(p) {
    if (!p || p.isNull()) return null;
    try {
        var len = p.add(0x10).readS32();
        if (len < 0 || len > 4096) return null;
        return p.add(0x14).readUtf16String(len);
    } catch (e) { return null; }
}
function rawValue(p) {
    if (!p || p.isNull()) return { ptr: "0x0", null: true };
    return {
        ptr: ptrString(p),
        i32: (function () { try { return p.toInt32(); } catch (e) { return null; } })(),
        string: readUtf16(p)
    };
}
function argsAbi(args, argc) {
    var out = [];
    for (var i = 0; i <= argc + 1; i++) {
        out.push({ index: i, role: i === 0 ? "self" : (i === argc + 1 ? "methodInfo" : "arg"), value: rawValue(args[i]) });
    }
    return out;
}

// Resolve the GameObject name of a Component via Object.get_name.
var getNamePtr = null;
function buttonName(componentPtr) {
    if (!componentPtr || componentPtr.isNull()) return null;
    if (!getNamePtr) getNamePtr = resolve("UnityEngine.Object", "get_name", 0);
    if (!getNamePtr) return null;
    try {
        var str = new NativeFunction(getNamePtr, "pointer", ["pointer", "pointer"])(componentPtr, ptr(0));
        return readUtf16(str);
    } catch (e) {
        return null;
    }
}

function hookMethod(className, methodName, paramCount, source, onEnterExtra) {
    var address = resolve(className, methodName, paramCount);
    if (!address) return false;
    var key = address.toString();
    if (hooked[key]) return false;
    try {
        Interceptor.attach(address, {
            onEnter: function (args) {
                var info = {
                    className: className,
                    methodName: methodName,
                    source: source,
                    address: key,
                    rva: rvaString(address),
                    self: ptrString(args[0]),
                    args: argsAbi(args, paramCount)
                };
                if (onEnterExtra) onEnterExtra(info, args);
                this.traceInfo = info;
                emit("enter", info);
            },
            onLeave: function (retval) {
                var info = this.traceInfo || {};
                info.retval = rawValue(retval);
                emit("leave", info);
            }
        });
        hooked[key] = true;
        emit("hook_installed", {
            className: className, methodName: methodName, source: source,
            address: key, rva: rvaString(address), paramCount: paramCount
        });
        return true;
    } catch (e) {
        emit("hook_error", { className: className, methodName: methodName, error: String(e) });
        return false;
    }
}

var multichestFields = [
    "openCardsButton", "_rewardCount", "_freeCount", "_actualCount", "_currentCount",
    "_openAllActive", "_pressedOpenAll", "_cardsAppearAnimDone", "_openAllLock", "_buttonsActive"
];

function snapshotMultichest(selfPtr) {
    var out = { self: ptrString(selfPtr), fields: {} };
    if (!selfPtr || selfPtr.isNull()) return out;
    for (var i = 0; i < multichestFields.length; i++) {
        try {
            var field = R.getField(MULTICHEST_CLASS, multichestFields[i]);
            if (!field || field.isStatic) { out.fields[multichestFields[i]] = { missing: true }; continue; }
            var off = field.offset;
            if (/_Count$/.test(multichestFields[i])) {
                out.fields[multichestFields[i]] = { offset: "0x" + off.toString(16), i32: selfPtr.add(off).readS32() };
            } else if (/^_/.test(multichestFields[i])) {
                out.fields[multichestFields[i]] = { offset: "0x" + off.toString(16), u8: selfPtr.add(off).readU8() };
            } else {
                out.fields[multichestFields[i]] = { offset: "0x" + off.toString(16), ptr: ptrString(selfPtr.add(off).readPointer()) };
            }
        } catch (e) {
            out.fields[multichestFields[i]] = { error: String(e) };
        }
    }
    return out;
}

// Click: resolve name once per listener for the click event.
var clickTargets = [
    [CLICK_CLASS, "HandleClick", 0, "click"]
];
for (var i = 0; i < clickTargets.length; i++) {
    hookMethod(clickTargets[i][0], clickTargets[i][1], clickTargets[i][2], clickTargets[i][3],
        function (info, args) {
            info.buttonName = buttonName(args[0]);
        });
}

var multichestTargets = [
    ["ShowMultichestWindow", 6], ["OnEnable", 0], ["Start", 0],
    ["OnOpenAll", 0], ["OpenAll", 0], ["OpenCard", 1], ["OpenCards", 1],
    ["UpdateButtons", 0], ["UpdateData", 0], ["Refresh", 0],
    ["CloseWindow", 0], ["OnCloseAction", 0], ["WindowHidden", 0]
];
for (var m = 0; m < multichestTargets.length; m++) {
    hookMethod(MULTICHEST_CLASS, multichestTargets[m][0], multichestTargets[m][1], "multichest",
        function (info, args) {
            info.before = snapshotMultichest(args[0]);
        });
}

var matchTargets = [
    ["ShowWindow", 4], ["OnEnable", 0], ["UpdateButtons", 0], ["Claim", 0], ["ClaimAll", 0]
];
for (var n = 0; n < matchTargets.length; n++) {
    hookMethod(MATCH_COMPLETED_CLASS, matchTargets[n][0], matchTargets[n][1], "match_completed", null);
}

hookMethod(JOURNEY_CLASS, "ClaimRewardsOnWin", 0, "journey", null);

emit("trace_ready", {
    hooked: Object.keys(hooked).length,
    click: clickTargets.length,
    multichest: multichestTargets.length,
    matchCompleted: matchTargets.length,
    journey: 1
});
