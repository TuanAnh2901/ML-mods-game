'use strict';

let installed = false;
let gameModule = null;

function inGameAssembly(address) {
    return address.compare(gameModule.base) >= 0 &&
        address.compare(gameModule.base.add(gameModule.size)) < 0;
}

function summarizeCalls(summary) {
    const calls = [];
    for (const [target, count] of Object.entries(summary)) {
        const address = ptr(target);
        if (inGameAssembly(address)) {
            calls.push({
                rva: address.sub(gameModule.base).toString(),
                address: address.toString(),
                count: count
            });
        }
    }
    calls.sort((left, right) => right.count - left.count);
    return calls.slice(0, 200);
}

function install(module) {
    if (installed) {
        return;
    }
    installed = true;
    gameModule = module;

    const init = module.findExportByName('il2cpp_init');
    const runtimeInit = module.base.add(0x4268d0);
    send({event: 'module', base: module.base.toString(), size: module.size, init: init ? init.toString() : null, runtimeInit: runtimeInit.toString()});

    Interceptor.attach(runtimeInit, {
        onEnter(args) {
            send({event: 'runtime_init_enter', threadId: this.threadId, args: [args[0].toString(), args[1].toString(), args[2].toString(), args[3].toString()]});
        },
        onLeave(retval) {
            send({event: 'runtime_init_leave', threadId: this.threadId, retval: retval.toString()});
        }
    });

    if (init === null) {
        send({event: 'error', message: 'il2cpp_init export was not found'});
        return;
    }

    Interceptor.attach(init, {
        onEnter(args) {
            this.tid = this.threadId;
            const tid = this.tid;
            send({event: 'il2cpp_init_enter', threadId: tid, arg0: args[0].toString()});
            Stalker.follow(tid, {
                events: {call: true},
                onCallSummary(summary) {
                    send({event: 'il2cpp_init_call_summary', threadId: tid, calls: summarizeCalls(summary)});
                }
            });
        },
        onLeave(retval) {
            Stalker.unfollow(this.tid);
            Stalker.garbageCollect();
            send({event: 'il2cpp_init_leave', threadId: this.tid, retval: retval.toString()});
        }
    });
}

Process.attachModuleObserver({
    onAdded(module) {
        if (module.name.toLowerCase() === 'gameassembly.dll') {
            install(module);
        }
    }
});
