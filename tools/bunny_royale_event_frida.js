// Read-only Bunny Royale roulette trace.
// il2cpp_resolve.js is prepended by run_bunny_royale_frida.py.

var TRACE_EVENT_CLASSES = /(CardRoulette|ChooseReward|MiniEvent)/i;
var TRACE_METHODS = /(Parse|Spin|Progress|Lot|Choose|Delivery)/i;
var CARD_CLASSES = {
    CardRouletteDeckController: true,
    CardRouletteCardElement: true,
    CardRouletteBlackMarkWindow: true
};
var CARD_METHODS = /(Click|Select|Choose|Pick|Apply|Spin|Card|Open|Close|Show|Hide|Flip|Reveal|Black|Mark|Lose|Fail|Init)/i;
var TRACE_MIN_INTERVAL_MS = typeof TRACE_MIN_INTERVAL_MS === "number" ? TRACE_MIN_INTERVAL_MS : 1000;
var hooked = {};
var hookCount = 0;
var lastManagedEmitAt = {};
var lastCardSelection = null;

function safeString(value) {
    try { return value.readUtf8String(); } catch (e) { return null; }
}

function pointerString(value) {
    try { return value.isNull() ? "0x0" : value.toString(); } catch (e) { return "<unreadable>"; }
}

function smallInteger(value) {
    try {
        var number = value.toInt32();
        return number >= -1 && number <= 15 ? number : null;
    } catch (e) { return null; }
}

function argumentScalars(args, count) {
    var values = [];
    for (var i = 1; i <= Math.min(count, 7); i++) values.push(smallInteger(args[i]));
    return values;
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

function emitManaged(kind, details) {
    var now = Date.now();
    var key = kind + ":" + details.className + ":" + details.methodName;
    var last = lastManagedEmitAt[key] || 0;
    if (now - last < TRACE_MIN_INTERVAL_MS) return;
    lastManagedEmitAt[key] = now;
    emit(kind, details);
}

function hookResolved(className, methodName, paramCount, source) {
    var address = resolve(className, methodName, paramCount);
    if (!address) return false;
    var key = address.toString();
    if (hooked[key]) return false;
    try {
        Interceptor.attach(address, {
            onEnter: function (args) {
                var values = [];
                for (var i = 0; i < 8; i++) values.push(pointerString(args[i]));
                this.traceInfo = {
                    className: className,
                    methodName: methodName,
                    source: source,
                    address: address.toString(),
                    rva: rvaString(address),
                    args: values,
                    self: pointerString(args[0])
                };
                if (source === "card") {
                    this.traceInfo.cardArgs = argumentScalars(args, paramCount);
                    if (/(Click|Select|Choose|Pick)/i.test(methodName)) {
                        for (var scalarIndex = 0; scalarIndex < this.traceInfo.cardArgs.length; scalarIndex++) {
                            var candidate = this.traceInfo.cardArgs[scalarIndex];
                            if (candidate !== null && candidate >= 0 && candidate <= 3) {
                                lastCardSelection = {
                                    className: className,
                                    methodName: methodName,
                                    cardIndex: candidate,
                                    selectedAtMs: Date.now()
                                };
                                break;
                            }
                        }
                    }
                    emitManaged("card_signal", this.traceInfo);
                }
                if (className === "CardRouletteModel" && methodName === "OnSpinResponseReceived")
                    this.traceInfo.lastCardSelection = lastCardSelection;
                emitManaged("managed_enter", this.traceInfo);
            },
            onLeave: function (retval) {
                var info = this.traceInfo || {};
                info.retval = pointerString(retval);
                if (source === "card") emitManaged("card_signal_leave", info);
                emitManaged("managed_leave", info);
            }
        });
        hooked[key] = true;
        hookCount++;
        emit("hook_installed", {
            className: className,
            methodName: methodName,
            source: source,
            address: address.toString(),
            rva: rvaString(address),
            paramCount: paramCount
        });
        return true;
    } catch (e) {
        emit("hook_error", { className: className, methodName: methodName, error: String(e) });
        return false;
    }
}

var fixedTargets = [
    ["CardRouletteModel", "ParseRewards", 1],
    ["CardRouletteModel", "OnSpinResponseReceived", 2],
    ["CardRouletteModel", "OnProgressResponseReceived", 2],
    ["ChooseRewardRouletteEventModule", "GetRelevantRouletteLotDataForCategory", 1],
    ["ChooseRewardRouletteEventModule", "ChooseReward", 2],
    ["ChooseRewardRouletteEventModule", "CreateDeliveryDataAndSave", 0]
];

for (var i = 0; i < fixedTargets.length; i++)
    hookResolved(fixedTargets[i][0], fixedTargets[i][1], fixedTargets[i][2], "fixed");

var candidates = [];
Object.keys(classMap).forEach(function (className) {
    if (!TRACE_EVENT_CLASSES.test(className)) return;
    candidates.push(className);
});
emit("candidate_classes", { classes: candidates.slice(0, 200), total: candidates.length });

function enumerateMethods(klass) {
    var results = [];
    var iter = Memory.alloc(Process.pointerSize);
    iter.writePointer(ptr(0));
    var method;
    while (!(method = il2cpp.class_get_methods(klass, iter)).isNull()) results.push(method);
    return results;
}

// Card UI hooks capture card-index candidates and BlackMark transitions in
// the same timestamp sequence as spin responses.
Object.keys(CARD_CLASSES).forEach(function (className) {
    if (!classMap[className]) return;
    var methods = enumerateMethods(classMap[className]);
    var catalog = [];
    for (var m = 0; m < methods.length; m++) {
        var method = methods[m];
        var name = safeString(il2cpp.method_get_name(method));
        var parameterCount = il2cpp.method_get_param_count(method);
        if (name) catalog.push({ name: name, paramCount: parameterCount });
        if (!name || !CARD_METHODS.test(name)) continue;
        hookResolved(className, name, parameterCount, "card");
    }
    emit("card_method_catalog", { className: className, methods: catalog });
});

// Dynamic event hooks catch newly introduced roulette classes without
// requiring a new RVA. The reduced cap avoids unrelated mini-event noise.
var dynamicCount = 0;
for (var c = 0; c < candidates.length && dynamicCount < 12; c++) {
    if (CARD_CLASSES[candidates[c]]) continue;
    var methods = enumerateMethods(classMap[candidates[c]]);
    if (!methods || methods.length === 0) continue;
    for (var m = 0; m < methods.length && dynamicCount < 12; m++) {
        var method = methods[m];
        var name = safeString(il2cpp.method_get_name(method));
        if (!name || !TRACE_METHODS.test(name)) continue;
        var address = getMethodPtr(method);
        if (!address || address.isNull()) continue;
        var key = address.toString();
        if (hooked[key]) continue;
        var parameterCount = il2cpp.method_get_param_count(method);
        if (hookResolved(candidates[c], name, parameterCount, "dynamic")) dynamicCount++;
    }
}

emit("trace_ready", { fixedHooks: fixedTargets.length, dynamicHooks: dynamicCount, totalHooks: hookCount });
