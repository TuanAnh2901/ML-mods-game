# ANALYSIS WORKSPACE

**Generated:** 2026-07-22
**Scope:** Reverse engineering workspace for Everlusting Life's Mfuscator-obfuscated IL2CPP metadata.

## SCRIPT INDEX

| Script | Purpose | Run |
|--------|---------|-----|
| `metadata_header_xxtea.py` | Offline XXTEA decoder + XOR section decryption for custom header | `py -3.13 metadata_header_xxtea.py` |
| `extract_custom_metadata_header.py` | Extract raw custom header bytes from `global-metadata.dat` | `py -3.13 extract_custom_metadata_header.py` |
| `decrypt_known_loader_sections.py` | Decrypt sections identified by the Loader (RVA 0x434A60) | `py -3.13 decrypt_known_loader_sections.py` |
| `build_partial_standard_metadata.py` | Assemble partially reconstructed v31 metadata from decoded sections | `py -3.13 build_partial_standard_metadata.py` |
| `run_frida_custom_header_access_trace.py` | Frida: trace runtime accesses to custom header bytes | `py -3.13 run_frida_custom_header_access_trace.py` |
| `run_frida_custom_header_memory_access.py` | Frida: trace memory reads/writes on custom header region | `py -3.13 run_frida_custom_header_memory_access.py` |
| `run_frida_il2cpp_trace.py` | Frida: trace `il2cpp_init` and metadata initialization sequence | `py -3.13 run_frida_il2cpp_trace.py` |
| `run_frida_metadata_file_trace.py` | Frida: trace file read operations on `global-metadata.dat` | `py -3.13 run_frida_metadata_file_trace.py` |
| `run_frida_metadata_loader_trace.py` | Frida: trace the Loader function at 0x434A60 region | `py -3.13 run_frida_metadata_loader_trace.py` |
| `test_metadata_header_xxtea.py` | Python unittest for XXTEA decode/encode round-trip | `py -3.13 test_metadata_header_xxtea.py` |
| `test_metadata_reconstruction.py` | Validate the full 31-section v31 reconstruction | `py -3.13 -m unittest test_metadata_reconstruction.py -v` |
| `Scan-Il2CppMetadata.ps1` | PowerShell: scan game binary for metadata-related signatures | `powershell ./Scan-Il2CppMetadata.ps1` |

## FRIDA HOOKS

| Script | Target |
|--------|--------|
| `frida_custom_header_access_trace.js` | Instrument reads of custom header region after init |
| `frida_custom_header_memory_access.js` | Memory access breakpoints on custom header pages |
| `frida_il2cpp_init_trace.js` | Hook `il2cpp_init` + metadata global setup |
| `frida_metadata_file_trace.js` | Hook `CreateFile`/`ReadFile` on the `.dat` file |
| `frida_metadata_loader_trace.js` | Hook the Loader function at `GameAssembly.dll+0x434A60` |

Each `.js` has a paired `.jsonl` trace output from the last run.

## ARTIFACTS

| File | Stage | What it is |
|------|-------|------------|
| `global-metadata.custom-header.offline.dat` | Offline | Raw custom header extracted statically from `.dat` file |
| `global-metadata.custom-header-after-loader.dat` | Loader | Custom header after Loader processes it (Frida dump) |
| `global-metadata.custom-header-after-init.dat` | Init | Custom header after full init (Frida dump) |
| `global-metadata.custom-header-transform-input.dat` | Transform | Header state fed to the transform function at 0x43A040 |
| `global-metadata.custom-header-transform-output.dat` | Transform | Header state after transform function runs |
| `global-metadata.partially-decoded.dat` | Partial | XXTEA-decoded custom header, some sections decoded |
| `global-metadata.post-transform.dat` | Post-transform | Metadata after the 0x43A040 transform completes |
| `global-metadata.v31-partial.dat` | Historical | Earlier incomplete reconstruction retained as evidence |
| `global-metadata.v31-reconstructed.dat` | Complete | Valid v31 reconstruction matching the plugin's section map |

## EVIDENCE DIRECTORIES (RVA-NAMED)

Each dir holds Ghidra analysis extracts, IDA/Frida notes, and partial decompilations scoped to a specific function:

| Directory | RVA | Function |
|-----------|-----|----------|
| `MetadataLoader434A60/` | `0x434A60` | Custom header loader (XXTEA decrypt, section parse) |
| `MetadataHeaderTransform43A040/` | `0x43A040` | In-place header transform between Loader and Init |
| `MetadataInitializer497460/` | `0x497460` | Metadata initialization / global setup |
| `MetadataCandidate497460/` | `0x497460` | Alternative analysis of the same init function |
| `RuntimeInit/` | - | `il2cpp_init` runtime flow traces |
| `RuntimeInitCandidates/` | - | Alternative init path hypotheses |

## CONVENTIONS

- **Python**: `py -3.13` launcher, standalone (no pip). `snake_case`, type hints, `struct` for binary I/O.
- **Frida JS**: ES5 (Duktape). No `import`/`export`. Flat procedural style.
- **PowerShell**: PS 5.1, PascalCase, `Add-Type` for P/Invoke signatures.
- **Evidence dirs**: Named `FunctionNameRVA` — the RVA is the offset in `GameAssembly.dll`.

## ANTI-PATTERNS

- Do NOT use the leaked Mfuscator field name mapping — this build uses a different permutation.
- Do NOT run Frida scripts with `python` (3.12) — always `py -3.13`.
- Do NOT treat `References/mfuscator_libil2cpp/` as authoritative — it is generic reference code, not this build.
- Do NOT edit `.jsonl` trace files — they are machine-generated, binary-safe evidence.

## EXTERNAL DEPENDENCIES

- **Cpp2IL worktree** at `D:\VSCode\Cpp2IL\Cpp2IL` — contains the Mfuscator plugin source + tests.
- **Frida** installed under Python 3.13 only (`py -3.13 -c "import frida"` to verify).
- **Ghidra** project at `Analysis/Ghidra/` with `GameAssembly.dll` imported at base 0x0.
- **GameAssembly.dll** at workspace root (100 MB IL2CPP binary, not in Analysis/).

## REPORTS

| File | Content |
|------|---------|
| `README.md` | Handoff doc: current deobfuscation state, next steps, open questions |
| `METADATA_DEOBFUSCATION_REPORT.md` | Full forensic chronology with hex dumps and decision log |
| `metadata_section_layout.json` | Observed offsets/sizes for the 31-section v31 layout |
| `metadata_v31_section_map.json` | Definitive 31-section custom-field mapping and physical layout |
| `ghidra-triage.log` | Ghidra analysis console output |
| `ghidra-gameplay-economy-native.log` | Gameplay economy functions at verified native RVAs |
| `runtime-init-triage.log` | Runtime init sequence observations |
| `cpp2il-stock.log` | Cpp2IL output with stock (vanilla) metadata decoding |
| `Cpp2IL-net10-custom-header.log` | Cpp2IL output with .NET 10 + custom header plugin |
| `Cpp2IL-v31-partial.log` | Cpp2IL output against partially reconstructed v31 metadata |
| `Verification/Cpp2IL-method-map.log` | Fresh successful fixup/load/map export from the original metadata |

## NOTES

- All `.dat` artifacts are read-only evidence. Generate new ones with the Python scripts.
- The XXTEA key `fc48e86b730833ef` is verified against the Loader at 0x434A60.
- The two XOR-encrypted payload sections and all 31 v31 descriptors are mapped; Cpp2IL consumes the original metadata through the plugin.
- `Cpp2IL-method-map/method-pointer-map.json` is authoritative for `GameAssembly.dll` RVAs. Managed DLL RVA comments are not native addresses.
- `GameplayEconomy/README.md` is the handoff for economy formulas, native RVAs, field offsets, and decoded round configuration.
- Cpp2IL-stock/ and Cpp2IL-current/ hold Cpp2IL releases used for regression comparison.
