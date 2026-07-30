'use strict';

const fileHandles = new Set();
const mappingHandles = new Set();
let monitored = false;
let accessCount = 0;
let gameModule = null;
let transformedMetadata = null;

function hexPrefix(address) {
    const bytes = new Uint8Array(address.readByteArray(32));
    return Array.from(bytes, byte => byte.toString(16).padStart(2, '0')).join('');
}

function rva(address) {
    if (gameModule === null || address.compare(gameModule.base) < 0 ||
        address.compare(gameModule.base.add(gameModule.size)) >= 0) {
        return null;
    }
    return address.sub(gameModule.base).toString();
}

function watchBuffer(base, source) {
    if (monitored || base.readU32() !== 0xfe6cb0e1) {
        return;
    }
    monitored = true;
    send({event: 'metadata_buffer', source: source, base: base.toString(), prefix: hexPrefix(base)});
    MemoryAccessMonitor.enable([{base: base, size: 4096}], {
        onAccess(details) {
            if (accessCount++ >= 40) {
                MemoryAccessMonitor.disable();
                return;
            }
            send({event: 'metadata_access', operation: details.operation, from: details.from.toString(),
                fromRva: rva(details.from), address: details.address.toString()});
        }
    });
}

function hookCreateFile() {
    const address = Module.findGlobalExportByName('CreateFileW');
    if (address === null) return;
    Interceptor.attach(address, {
        onEnter(args) {
            this.track = false;
            try {
                this.path = args[0].readUtf16String();
                this.track = this.path.toLowerCase().endsWith('global-metadata.dat');
            } catch (_) {}
        },
        onLeave(retval) {
            if (!this.track || retval.isNull() || retval.toString() === '0xffffffffffffffff') return;
            const handle = retval.toString();
            fileHandles.add(handle);
            send({event: 'metadata_file_open', api: 'CreateFileW', handle: handle, path: this.path});
        }
    });
}

function hookReadFile() {
    const address = Module.findGlobalExportByName('ReadFile');
    if (address === null) return;
    Interceptor.attach(address, {
        onEnter(args) {
            this.track = fileHandles.has(args[0].toString());
            this.buffer = args[1];
            this.requested = args[2].toUInt32();
        },
        onLeave(retval) {
            if (!this.track || retval.toInt32() === 0) return;
            try {
                send({event: 'metadata_file_read', size: this.requested, buffer: this.buffer.toString(), prefix: hexPrefix(this.buffer)});
                watchBuffer(this.buffer, 'ReadFile');
            } catch (error) {
                send({event: 'metadata_read_error', error: String(error)});
            }
        }
    });
}

function hookCreateFileMapping() {
    const address = Module.findGlobalExportByName('CreateFileMappingW');
    if (address === null) return;
    Interceptor.attach(address, {
        onEnter(args) {
            this.track = fileHandles.has(args[0].toString());
        },
        onLeave(retval) {
            if (!this.track || retval.isNull()) return;
            mappingHandles.add(retval.toString());
            send({event: 'metadata_file_mapping', handle: retval.toString()});
        }
    });
}

function hookMapView() {
    const address = Module.findGlobalExportByName('MapViewOfFile');
    if (address === null) return;
    Interceptor.attach(address, {
        onEnter(args) {
            this.track = mappingHandles.has(args[0].toString());
        },
        onLeave(retval) {
            if (!this.track || retval.isNull()) return;
            try {
                send({event: 'metadata_file_map_view', base: retval.toString(), prefix: hexPrefix(retval)});
                watchBuffer(retval, 'MapViewOfFile');
            } catch (error) {
                send({event: 'metadata_map_error', error: String(error)});
            }
        }
    });
}

hookCreateFile();
hookReadFile();
hookCreateFileMapping();
hookMapView();

Process.attachModuleObserver({
    onAdded(module) {
        if (module.name.toLowerCase() === 'gameassembly.dll') {
            gameModule = module;
            const loader = module.base.add(0x434a60);
            const transform = module.base.add(0x43a040);
            Interceptor.attach(transform, {
                onEnter(args) {
                    this.capture = rva(this.returnAddress) === '0x434b4d';
                    if (this.capture) {
                        this.size = Number(BigInt(args[1].toString()));
                    }
                },
                onLeave(retval) {
                    if (!this.capture || retval.isNull() || this.size <= 0) return;
                    transformedMetadata = {base: ptr(retval.toString()), size: this.size};
                    send({event: 'metadata_transform_complete', base: retval.toString(), size: this.size, prefix: hexPrefix(retval)});
                }
            });
            Interceptor.attach(loader, {
                onEnter() {
                    transformedMetadata = null;
                },
                onLeave(retval) {
                    if (retval.toInt32() === 0 || transformedMetadata === null) return;
                    try {
                        const data = transformedMetadata.base.readByteArray(transformedMetadata.size);
                        send({event: 'metadata_post_transform', base: transformedMetadata.base.toString(), size: transformedMetadata.size,
                            prefix: hexPrefix(transformedMetadata.base)}, data);
                    } catch (error) {
                        send({event: 'metadata_dump_error', error: String(error)});
                    }
                }
            });
            const init = module.findExportByName('il2cpp_init');
            if (init !== null) {
                Interceptor.attach(init, {
                    onLeave(retval) {
                        send({event: 'runtime_init_leave', retval: retval.toString()});
                    }
                });
            }
        }
    }
});
