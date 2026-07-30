'use strict';

let gameModule = null;
let customHeader = null;
const metadataLoaderRva = 0x434a60;
const metadataLoaderEndRva = 0x43606e;
let headerAccesses = 0;
const maxHeaderAccesses = 128;
const observedHeaderAccesses = new Set();

function rva(address) {
    if (gameModule === null || address.compare(gameModule.base) < 0 ||
        address.compare(gameModule.base.add(gameModule.size)) >= 0) {
        return null;
    }

    return gameModule.base.add(0).equals(address) ? '0x0' : address.sub(gameModule.base).toString();
}

function emitHeader(event) {
    const bytes = customHeader.base.readByteArray(customHeader.size);
    send({ event: event, base: customHeader.base.toString(), size: customHeader.size }, bytes);
}

Process.attachModuleObserver({
    onAdded(module) {
        if (module.name.toLowerCase() !== 'gameassembly.dll') {
            return;
        }

        gameModule = module;
        const transform = module.base.add(0x43a040);
        const loader = module.base.add(metadataLoaderRva);

        Interceptor.attach(transform, {
            onEnter(args) {
                this.isMetadataHeader = rva(this.returnAddress) === '0x434b4d';
                this.size = this.isMetadataHeader ? Number(BigInt(args[1].toString())) : 0;
                if (this.isMetadataHeader && this.size > 0) {
                    this.keyHex = Array.from(new Uint8Array(args[2].readByteArray(16)))
                        .map(byte => byte.toString(16).padStart(2, '0'))
                        .join('');
                    send({ event: 'custom_header_transform_input', size: this.size, key: this.keyHex },
                        args[0].readByteArray(this.size));
                }
            },
            onLeave(retval) {
                if (!this.isMetadataHeader || retval.isNull() || this.size <= 0) {
                    return;
                }

                customHeader = { base: ptr(retval.toString()), size: this.size };
                send({ event: 'custom_header_transform_output', base: customHeader.base.toString(), size: customHeader.size },
                    customHeader.base.readByteArray(customHeader.size));
                send({ event: 'custom_header_transform', base: customHeader.base.toString(), size: customHeader.size });
            }
        });

        Interceptor.attach(loader, {
            onLeave(retval) {
                if (retval.toInt32() !== 0 && customHeader !== null) {
                    emitHeader('custom_header_after_loader');
                }
            }
        });

        const init = module.findExportByName('il2cpp_init');
        if (init !== null) {
            Interceptor.attach(init, {
                onEnter() {
                    const threadId = this.threadId;
                    Stalker.follow(threadId, {
                        transform(iterator) {
                            let instruction;
                            while ((instruction = iterator.next()) !== null) {
                                const instructionRva = rva(instruction.address);
                                const numericRva = instructionRva === null ? -1 : Number(instructionRva);
                                const memoryOperand = instruction.operands.find(operand => operand.type === 'mem');

                                if (memoryOperand !== undefined && numericRva >= 0) {
                                    const baseRegister = memoryOperand.value.base;
                                    const displacement = memoryOperand.value.disp;
                                    if (baseRegister !== null && displacement >= 0 && displacement < 0x2a4) {
                                        const capturedRva = instructionRva;
                                        const capturedInstruction = instruction.mnemonic + ' ' + instruction.opStr;
                                        const capturedBaseRegister = baseRegister;
                                        const capturedDisplacement = displacement;
                                        iterator.putCallout(context => {
                                            if (customHeader === null || headerAccesses >= maxHeaderAccesses) {
                                                return;
                                            }

                                            const base = context[capturedBaseRegister];
                                            if (base === undefined || !ptr(base).equals(customHeader.base)) {
                                                return;
                                            }

                                            const accessKey = capturedRva + ':' + capturedDisplacement;
                                            if (observedHeaderAccesses.has(accessKey)) {
                                                return;
                                            }

                                            observedHeaderAccesses.add(accessKey);
                                            headerAccesses++;
                                            send({
                                                event: 'custom_header_field_access',
                                                rva: capturedRva,
                                                offset: capturedDisplacement,
                                                instruction: capturedInstruction,
                                                value: customHeader.base.add(capturedDisplacement).readU32()
                                            });
                                        });
                                    }
                                }

                                iterator.keep();
                            }
                        }
                    });
                },
                onLeave(retval) {
                    Stalker.unfollow(this.threadId);
                    Stalker.garbageCollect();
                    if (customHeader !== null) {
                        emitHeader('custom_header_after_init');
                    }
                    send({ event: 'runtime_init_leave', retval: retval.toString() });
                }
            });
        }
    }
});
