# Metadata Deobfuscation Report

## Completion Update (2026-07-22)

The conclusions and next steps later in this chronology describe intermediate states. The metadata reconstruction is now complete for this build.

- `CustomMetadataHeaderDecoder.cs` contains the recovered permutation for all 31 version-31 offset/size descriptors.
- The decoder applies the loader header mutations, source bias `0x1E4`, and both sequential-XOR payload transforms before returning a standard v31 image.
- `MfuscatorSupportPlugin.cs` recognizes and rebuilds the original `global-metadata.dat` directly; Cpp2IL reports metadata version `31.1` and maps `206366` method pointers.
- Full DLL output produced `167` assemblies and recovered `113874 / 114122` method bodies.
- `Assembly-CSharp.dll` SHA-256 is `D75C503F5A5EF60A1C6438A711AF80D9101C3FD2494579E5291FF3FE9E49BF9D`.
- `metadata_v31_section_map.json` is the definitive descriptor map. `global-metadata.v31-reconstructed.dat` is the offline rebuilt artifact.
- The final corrected `exportedTypeDefinitions` entry is offset `0x0190319C`, size `0x00002BD0`, from custom fields `+0x18C` and `+0x030`.
- A fresh original-metadata `--export-method-map` run wrote `206366` entries. Its JSON is byte-identical to the earlier map, with SHA-256 `D47E61D4A3D47F18E3FE5E25D225AECAA1FA9188ECE18D9E14415370185586F7`; see `Verification/Cpp2IL-method-map.log`.

For native analysis, use `Cpp2IL-method-map/method-pointer-map.json`. RVA comments inside recovered managed DLLs are managed-PE addresses, not `GameAssembly.dll` RVAs.

## Inputs

- `global-metadata.dat`: 26,239,236 bytes, SHA-256 `07A2F0486D9EE56528DB594F9C5C7567002C5C5A6AF5600EB97582135BC5DD42`
- `GameAssembly.dll`: 105,260,032 bytes, SHA-256 `B95BA8FDCA3A0FD771C70CD46021E0981E607550164160A42149E0857297DDF9`
- Unity version detected from `globalgamemanagers`: `6000.1.4f1`

The metadata begins `E1 B0 6C FE 1F 8E 7D B9`, not the expected IL2CPP magic `AF 1B B1 FA`.

## Methods Run

1. Stock Cpp2IL 2022.0.7: failed at the magic validation (`0xFE6CB0E1`).
2. Current Cpp2IL 2022.1.0 with an out-of-tree Mfuscator plugin build:
   - The plugin was loaded successfully.
   - It derived a candidate XOR key `0x2E`, but failed to determine a valid Mfuscator header size.
   - The game does not match the Mfuscator scheme supported by that plugin.
3. Runtime read-only process-memory scan:
   - Scanned committed readable pages of the running game for `AF 1B B1 FA` followed by a plausible metadata version.
   - No complete, standard IL2CPP metadata image was resident in memory.
4. Native analysis:
   - Ghidra project and auto-analysis were created under `Analysis/Ghidra`.
   - `il2cpp_init` is at `0x1804473D0` and calls `0x1804268D0` after its setup call.
   - The normal metadata path strings and both encrypted/plain header constants are absent from `GameAssembly.dll`.
   - Direct `ReadFile` uses at `0x180521...` are stream/text parsing; `0x1804A5DDC` is a thin Win32 read wrapper, not a proven metadata decryptor.

## Current Conclusion

No valid `global-metadata.decrypted.dat` was produced. The protection is not standard metadata, the supported Mfuscator format, or a persistent plaintext metadata buffer. The next static target is the call tree rooted at `0x1804268D0`; the corresponding loader may also be in `UnityPlayer.dll`.

## Frida Loader Trace (2026-07-22)

- Frida 17.16.3 was installed for Python 3.13. The trace runners spawn their own game process and always terminate that same PID after collection.
- `global-metadata.dat` is opened through `CreateFileW`, mapped with `CreateFileMappingW`/`MapViewOfFile`, and starts at `E1 B0 6C FE 1F 8E 7D B9` in the mapped view.
- The first observed read of the mapped header occurs at `GameAssembly.dll` RVA `0x434AD7`, inside loader RVA `0x434A60`.
- Loader `0x434A60` creates a 676-byte transformed header by calling RVA `0x43A040` from `0x434B48`. The post-transform header was captured as `global-metadata.post-transform.dat`.
- After the transform, the loader applies direct mutations at header offsets `+0x05` and `+0x09`, then section-specific XOR loops. It uses the header to copy a 935,380-byte section from file offset `0x427CC` and a 3,734,008-byte section from file offset `0x126DA0`; the latter begins with `Assembly-CSharp` after decoding.
- The transformed header yields a verified 16-section contiguous physical layout from `0x1A20` through `0x1902FE8`. See `metadata_section_layout.json`; the remaining `0x30DC` bytes are not part of that layout.
- `0x497460` is the following metadata-consumer initialization routine; its large allocations are parsed metadata tables, not a conventional full plaintext metadata image.

## Current Next Step

Treat `global-metadata.post-transform.dat` as the authoritative custom header. Reconstruct the standard metadata section order from its descriptor fields and the original mapped file, then add a game-specific metadata-fixup path to Cpp2IL rather than attempting a memory dump for `AF 1B B1 FA`.

## Mfuscator libil2cpp Cross-check (2026-07-22)

- Both supplied source trees were cloned under `Analysis/References/` and examined locally.
- `mfuscator_libil2cpp` confirms this game's loader family and the exact loader mutations observed in `GameAssembly.dll + 0x434A60`: swap header byte `0` with the final byte, XOR byte `+0x09` with `0x27`, XOR byte `+0x05` with `0x59`, use a section-source bias of `0x1E4`, then decode the two string-related payloads with sequential XOR.
- The game-specific values are independently verified: header `+0x0B4` / `+0x26C` selects and sizes the string-literal-data section (`0x427CC`, `0xE45D4`); header `+0x19C` / `+0x0AC` selects and sizes the metadata-string section (`0x126DA0`, `0x38F9F8`).
- The same trace identifies `+0x140` as `imagesSize` (`0x1A18`), `+0x080` as `assembliesSize` (`0x29C0`), `+0x248` as `typeDefinitionsSize` (`0x2B5C08`), and `+0x040` as `methodsSize` (`0x715C38`).
- The leaked custom-header struct has the same 672-byte payload layout plus a four-byte trailer, but its descriptor permutation is not this game's permutation. Applying those field names directly produces invalid offsets and sizes, so it is evidence for the transform and loader behavior, not a valid v31 section map.
- Static RIP-relative reference analysis finds 48 native references to the header global and 36 distinct field displacements. The trace script `frida_custom_header_access_trace.js` now records only accesses whose base register equals the decoded-header allocation; its closure lifetime bug was fixed and the direct loader mapping above was captured reproducibly.

## Offline Header Decoder (2026-07-22)

- The transform at `GameAssembly.dll + 0x43A040` is a 169-word XXTEA decrypt routine with a custom first key word. It receives 676 bytes from `global-metadata.dat + 0x4` and returns the custom metadata header.
- A narrow Frida run captured the transform input, 16-byte key, and output before the loader mutations. The key is stable for this build.
- `Analysis/metadata_header_xxtea.py` reproduces the native decrypt path. `Analysis/extract_custom_metadata_header.py` emits `Analysis/global-metadata.custom-header.offline.dat` from the installed metadata file without launching the game.
- `Analysis/test_metadata_header_xxtea.py` verifies both the captured transform input and the raw metadata prefix against the pre-loader runtime output. The generated offline header and runtime capture have the same SHA-256: `600D8093D4A13204187C8F2B10288CA400EF5BBEA1CFC2E41C137CA73BA4161A`.
- The header is still a game-specific, permuted descriptor structure rather than `Il2CppGlobalMetadataHeader`. Replacing the file magic or passing this header directly to Cpp2IL would be invalid. Cpp2IL integration must first recover the mapping of this structure's descriptors to the 31 version-31 logical sections.
- The Frida runner now force-terminates the spawned process tree after capture. It was verified that no game process remains after a trace.

## Artifacts

- `cpp2il-stock.log`: stock-tool failure evidence.
- `Cpp2IL-current/cpp2il-mfuscator.log`: plugin load and Mfuscator failure evidence.
- `Cpp2IL-current/Plugins/Cpp2IL.Plugin.Mfuscator.dll`: isolated plugin build used for the test.
- `Scan-Il2CppMetadata.ps1`: read-only runtime scanner.
- `Ghidra/Everlusting.gpr` and `Ghidra/Everlusting.rep/`: native-analysis database.
- `GhidraScripts/MetadataTriage.java` and `ghidra-triage.log`: reproducible Ghidra triage.
- `frida_metadata_file_trace.js`, `run_frida_metadata_file_trace.py`, and `frida_metadata_file_trace.jsonl`: file-map and loader-call evidence.
- `global-metadata.post-transform.dat`: 676-byte transformed custom header captured after RVA `0x43A040`.
- `global-metadata.custom-header-transform-input.dat` and `global-metadata.custom-header-transform-output.dat`: reproducible input/output pair for the native header transform.
- `global-metadata.custom-header.offline.dat`: offline extraction output, byte-identical to the captured pre-loader header.
- `metadata_header_xxtea.py`, `extract_custom_metadata_header.py`, and `test_metadata_header_xxtea.py`: offline decoder, CLI, and regression tests.
- `MetadataLoader434A60/`: extracted native loader bytes, disassembly, and evidence for RVA `0x434A60`.
- `metadata_section_layout.json`: verified contiguous section chain reconstructed from the transformed header.

## Next Trace

1. Create/decompile a function at `0x1804268D0` and trace its callees until the metadata-sized buffer is allocated or transformed.
2. If the call tree does not load the file, repeat the same trace from the UnityPlayer initialization path.
3. Capture the buffer immediately after the transform and validate its first eight bytes plus section offsets before giving it to Cpp2IL.
