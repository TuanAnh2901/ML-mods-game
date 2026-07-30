'use strict';

let gameModule = null;
let runtimeThreadId = null;
let runtimeActive = false;

const candidateRvas = [
    0x430bd0, 0x456bd0, 0x492c40, 0x43b690, 0x451ec0,
    0x484840, 0x4a8420, 0x43f330, 0x4a8390, 0x4a4500,
    0x4a74d0, 0x497460, 0x47ac20, 0x435040
];
const seen = new Set();
const allocations = [];
let monitoredAccesses = 0;

function inRuntime() {
    return runtimeActive && Process.getCurrentThreadId() === runtimeThreadId;
}

function rva(address) {
    if (gameModule === null || address.compare(gameModule.base) < 0 ||
        address.compare(gameModule.base.add(gameModule.size)) >= 0) {
        return null;
    }
    return address.sub(gameModule.base).toString();
}

function emitOnce(key, payload) {
    if (seen.has(key)) {
        return;
    }
    seen.add(key);
    send(payload);
}

function nativeSize(value) {
    return BigInt(value.toString());
}

function hexPrefix(address, size) {
    const bytes = new Uint8Array(address.readByteArray(Number(size < 32n ? size : 32n)));
    return Array.from(bytes, byte => byte.toString(16).padStart(2, '0')).join('');
}

function inspectAllocations() {
    for (const allocation of allocations) {
        let prefix = null;
        try {
            prefix = hexPrefix(allocation.address, allocation.size);
        } catch (error) {
            send({event: 'allocation_inspect_error', address: allocation.address.toString(), error: String(error)});
            continue;
        }
        send({event: 'allocation_inspect', address: allocation.address.toString(), size: allocation.size.toString(),
            api: allocation.api, returnRva: allocation.returnRva, prefix: prefix});
    }
}

function traceEncryptedMetadataAccess() {
    for (const protection of ['r--', 'rw-', 'r-x']) {
        for (const range of Process.enumerateRanges(protection)) {
            try {
                if (range.size < 8 || range.base.readU32() !== 0xfe6cb0e1) {
                    continue;
                }
                send({event: 'encrypted_metadata_mapping', base: range.base.toString(), size: range.size, protection: protection});
                MemoryAccessMonitor.enable([{base: range.base, size: 4096}], {
                    onAccess(details) {
                        if (monitoredAccesses++ >= 24) {
                            MemoryAccessMonitor.disable();
                            return;
                        }
                        send({event: 'encrypted_metadata_access', operation: details.operation, from: details.from.toString(),
                            fromRva: rva(details.from), address: details.address.toString()});
                    }
                });
                return;
            } catch (_) {
                // A concurrent unmap or inaccessible range is not relevant to the trace.
            }
        }
    }
    send({event: 'encrypted_metadata_mapping_not_found'});
}

function installAllocationHook(moduleName, exportName, sizeIndex) {
    const address = Module.findGlobalExportByName(exportName);
    if (address === null) {
        return;
    }
    Interceptor.attach(address, {
        onEnter(args) {
            this.capture = inRuntime();
            if (!this.capture) {
                return;
            }
            this.size = nativeSize(args[sizeIndex]);
            this.returnRva = rva(this.returnAddress);
        },
        onLeave(retval) {
            if (!this.capture || this.size < 1048576n) {
                return;
            }
            emitOnce('alloc:' + exportName + ':' + this.size + ':' + this.returnRva, {
                event: 'large_allocation', api: exportName, module: moduleName,
                size: this.size.toString(), result: retval.toString(), returnRva: this.returnRva
            });
            allocations.push({address: ptr(retval.toString()), size: this.size, api: exportName, returnRva: this.returnRva});
        }
    });
}

function installCopyHook(exportName) {
    const address = Module.findGlobalExportByName(exportName);
    if (address === null) {
        return;
    }
    Interceptor.attach(address, {
        onEnter(args) {
            if (!inRuntime()) {
                return;
            }
            const size = nativeSize(args[2]);
            if (size < 1048576n) {
                return;
            }
            emitOnce('copy:' + exportName + ':' + size + ':' + rva(this.returnAddress), {
                event: 'large_copy', api: exportName, size: size.toString(),
                destination: args[0].toString(), source: args[1].toString(),
                returnRva: rva(this.returnAddress)
            });
        }
    });
}

function installFreeHook() {
    const address = Module.findGlobalExportByName('RtlFreeHeap');
    if (address === null) {
        return;
    }
    Interceptor.attach(address, {
        onEnter(args) {
            if (!inRuntime()) {
                return;
            }
            const target = args[2].toString();
            const allocation = allocations.find(item => item.address.toString() === target);
            if (allocation === undefined) {
                return;
            }
            try {
                const prefix = hexPrefix(allocation.address, allocation.size);
                const payload = {event: 'allocation_pre_free', address: target, size: allocation.size.toString(),
                    returnRva: allocation.returnRva, prefix: prefix};
                if (prefix.startsWith('af1bb1fa')) {
                    send({...payload, event: 'metadata_magic_pre_free'},
                        allocation.address.readByteArray(Number(allocation.size)));
                } else {
                    send(payload);
                }
            } catch (error) {
                send({event: 'allocation_pre_free_error', address: target, error: String(error)});
            }
        }
    });
}

function install(module) {
    gameModule = module;
    const runtimeInit = module.base.add(0x4268d0);
    send({event: 'module', base: module.base.toString(), runtimeInit: runtimeInit.toString()});

    Interceptor.attach(runtimeInit, {
        onEnter() {
            runtimeThreadId = this.threadId;
            runtimeActive = true;
            send({event: 'runtime_init_enter', threadId: runtimeThreadId});
        },
        onLeave(retval) {
            send({event: 'runtime_init_leave', threadId: this.threadId, retval: retval.toString()});
            runtimeActive = false;
        }
    });

    for (const candidate of candidateRvas) {
        const address = module.base.add(candidate);
        Interceptor.attach(address, {
            onEnter(args) {
                this.capture = inRuntime();
                if (this.capture) {
                    send({event: 'candidate_enter', rva: '0x' + candidate.toString(16), args: [args[0].toString(), args[1].toString(), args[2].toString(), args[3].toString()]});
                    if (candidate === 0x497460) {
                        traceEncryptedMetadataAccess();
                    }
                }
            },
            onLeave(retval) {
                if (this.capture) {
                    send({event: 'candidate_leave', rva: '0x' + candidate.toString(16), retval: retval.toString()});
                    if (candidate === 0x497460) {
                        inspectAllocations();
                    }
                }
            }
        });
    }

    installAllocationHook('kernelbase', 'VirtualAlloc', 1);
    installAllocationHook('ntdll', 'RtlAllocateHeap', 2);
    installAllocationHook('ucrtbase', 'malloc', 0);
    installCopyHook('memcpy');
    installCopyHook('memmove');
    installFreeHook();
}

Process.attachModuleObserver({
    onAdded(module) {
        if (module.name.toLowerCase() === 'gameassembly.dll') {
            install(module);
        }
    }
});
