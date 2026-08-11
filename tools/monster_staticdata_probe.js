// Probe MonsterStaticData field offsets + read a sample object.
// il2cpp_resolve.js is prepended by run_ui_click_frida.py.
var TARGET_CLASS = "MonsterStaticData";

function cstr(p) {
    try { return p.readUtf8String(); } catch (e) { return null; }
}
function readUtf16(p) {
    if (!p || p.isNull()) return null;
    try {
        var len = p.add(0x10).readS32();
        if (len < 0 || len > 4096) return null;
        return p.add(0x14).readUtf16String(len);
    } catch (e) { return null; }
}
function emit(kind, details) {
    details = details || {};
    details.event = kind;
    details.pid = Process.id;
    details.tsMs = Date.now();
    send(details);
}

var klass = null;
try {
    klass = classMap[TARGET_CLASS];
} catch (e) {}
emit("class_lookup", { className: TARGET_CLASS, found: !!klass });
if (!klass) throw new Error("class not found");

// Enumerate all fields with offsets
var fields = [];
var iter = Memory.alloc(Process.pointerSize);
iter.writePointer(ptr(0));
var field;
while (!(field = il2cpp.class_get_fields(klass, iter)).isNull()) {
    var name = cstr(il2cpp.field_get_name(field));
    var off = il2cpp.field_get_offset(field);
    fields.push({ name: name, offset: off });
}
emit("field_catalog", { className: TARGET_CLASS, fields: fields });

// Get a sample instance: hook get_MonstersList and read element 0's string fields.
var getter = resolve("NewAssets.Scripts.Data_Helpers.MonsterDataHelper", "get_MonstersList", 0);
emit("getter_resolve", { address: getter ? getter.toString() : null });
if (!getter) throw new Error("getter not found");

Interceptor.attach(getter, {
    onLeave: function (retval) {
        // List<T>: +0x10 items array, +0x18 size
        var items = retval.add(0x10).readPointer();
        var size = retval.add(0x18).readS32();
        emit("list_snapshot", { size: size, items: items.toString() });
        if (size > 0) {
            var first = items.add(0x20).readPointer();
            emit("sample_object", { self: first.toString() });
            var probe = {};
            for (var i = 0; i < fields.length; i++) {
                var off = fields[i].offset;
                if (off < 0) continue;
                try {
                    var raw = first.add(off).readPointer();
                    probe[fields[i].name + "@0x" + off.toString(16)] = {
                        ptr: raw.toString(),
                        utf16: readUtf16(raw),
                        i32: first.add(off).readS32(),
                    };
                } catch (e) {
                    probe[fields[i].name + "@0x" + off.toString(16)] = { error: String(e) };
                }
            }
            emit("sample_fields", probe);
        }
    }
});
emit("probe_ready", {});
