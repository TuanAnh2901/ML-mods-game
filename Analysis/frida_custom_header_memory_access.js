'use strict';

let gameModule = null;
let customHeader = null;
let monitorEnabled = false;
let accesses = 0;
const maxAccesses = 2000;

function rva(address) {
    if (gameModule === null || address.compare(gameModule.base) < 0 ||
        address.compare(gameModule.base.add(gameModule.size)) >= 0) {
        return null;
    }

    return address.sub(gameModule.base).toString();
}

function enableHeaderMonitor() {
    if (customHeader === null || monitorEnabled) {
        return;
    }

    monitorEnabled = true;
    MemoryAccessMonitor.enable([{ base: customHeader.base, size: customHeader.size }], {
        onAccess(details) {
            if (accesses++ >= maxAccesses) {
                MemoryAccessMonitor.disable();
                send({ event: 'custom_header_monitor_limit', accesses: accesses });
                return;
            }

            const address = ptr(details.address);
            if (address.compare(customHeader.base) < 0 ||
                address.compare(customHeader.base.add(customHeader.size)) >= 0) {
                return;
            }

            send({
                event: 'custom_header_memory_access',
                operation: details.operation,
                offset: Number(address.sub(customHeader.base)),
                from: details.from.toString(),
                rva: rva(details.from)
            });
        }
    });
}

Process.attachModuleObserver({
    onAdded(module) {
        if (module.name.toLowerCase() !== 'gameassembly.dll') {
            return;
        }

        gameModule = module;
        const transform = module.base.add(0x43a040);
        const loader = module.base.add(0x434a60);

        Interceptor.attach(transform, {
            onEnter(args) {
                this.isMetadataHeader = rva(this.returnAddress) === '0x434b4d';
                this.size = this.isMetadataHeader ? Number(BigInt(args[1].toString())) : 0;
            },
            onLeave(retval) {
                if (!this.isMetadataHeader || retval.isNull() || this.size <= 0) {
                    return;
                }

                customHeader = { base: ptr(retval.toString()), size: this.size };
                enableHeaderMonitor();
            }
        });

        Interceptor.attach(loader, {
            onLeave(retval) {
                if (monitorEnabled) {
                    MemoryAccessMonitor.disable();
                    send({ event: 'custom_header_loader_complete', retval: retval.toString(), accesses: accesses });
                }
            }
        });

        const init = module.findExportByName('il2cpp_init');
        if (init !== null) {
            Interceptor.attach(init, {
                onLeave(retval) {
                    if (monitorEnabled) {
                        MemoryAccessMonitor.disable();
                    }
                    send({ event: 'runtime_init_leave', retval: retval.toString(), accesses: accesses });
                }
            });
        }
    }
});
