# Metadata Deobfuscation Handoff

## Goal

Recover and consume the Mfuscator-obfuscated IL2CPP v31 metadata shipped with the game, then use the recovered metadata for native-code analysis.

## Current State

Metadata reconstruction is complete for this build.

- Game root: `D:\SteamLibrary\steamapps\common\Everlusting Life`
- Cpp2IL source: `D:\VSCode\Cpp2IL\Cpp2IL`
- Unity version: `6000.1.4f1`; metadata version: `31.1`
- Original metadata: `Everlusting Life_Data\il2cpp_data\Metadata\global-metadata.dat`
- Original prefix: `E1 B0 6C FE`, rather than standard `AF 1B B1 FA`
- The plugin recognizes the original file, decodes its custom 676-byte header, maps all 31 v31 sections, decrypts the two protected payload sections, and returns standard metadata bytes to Cpp2IL.
- Cpp2IL processes the original obfuscated metadata directly. The verified DLL export contains `167` assemblies and recovers `113874 / 114122` methods.
- `Analysis\Cpp2IL-v31-output\Assembly-CSharp.dll` SHA-256: `D75C503F5A5EF60A1C6438A711AF80D9101C3FD2494579E5291FF3FE9E49BF9D`.

## Decoder

Definitive implementation:

```text
D:\VSCode\Cpp2IL\Cpp2IL\Cpp2IL.Plugin.Mfuscator\CustomMetadataHeaderDecoder.cs
```

`MfuscatorSupportPlugin.cs` calls `CustomMetadataHeaderDecoder.TryRebuildMetadata` before the generic Mfuscator path. The decoder:

1. Decrypts 676 bytes from file offset `+0x4` with the XXTEA-family transform and key `fc48e86b730833ef`.
2. Applies the loader mutations: payload-end swap, byte `+0x09 XOR 0x27`, and byte `+0x05 XOR 0x59`.
3. Reads the 31 custom offset/size field pairs and applies source bias `0x1E4`.
4. Writes a standard version-31 metadata header.
5. Decrypts `stringLiteralData` with sequential XOR starting at `0x0D` and decrementing.
6. Decrypts the metadata `string` section with sequential XOR starting at `0x5F` and incrementing.

The complete descriptor mapping, including physical offsets and sizes, is in `metadata_v31_section_map.json`. The final corrected section is:

```text
exportedTypeDefinitions: offset=0x0190319C, size=0x00002BD0
custom offset field=+0x18C, custom size field=+0x030
```

## Verified Loader Evidence

`GameAssembly.dll + 0x434A60` calls the header transform at `+0x43A040`. The loader behavior and XXTEA key were reproduced offline by `metadata_header_xxtea.py`. The reference Mfuscator source confirms the transform family, but its descriptor permutation does not match this build and is not authoritative for section names.

The two protected sections are:

- `stringLiteralData`: file offset `0x427CC`, size `0xE45D4`
- metadata `string`: file offset `0x126DA0`, size `0x38F9F8`

## Primary Artifacts

| Artifact | Purpose |
|---|---|
| `metadata_v31_section_map.json` | All 31 standard sections, custom header fields, physical offsets, and sizes |
| `global-metadata.v31-reconstructed.dat` | Offline reconstructed metadata candidate |
| `Cpp2IL-original-custom-fixed.log` | Successful load of the original obfuscated metadata through the plugin |
| `Cpp2IL-v31-output.log` | Full 167-DLL export and method-recovery result |
| `Cpp2IL-v31-output\` | Recovered managed assemblies |
| `Cpp2IL-method-map\method-pointer-map.json` | `206366` managed-signature to native-pointer/RVA entries |
| `Cpp2IL-method-map.runtimeconfig.json` | Enables reflection serialization for the net10 method-map export |
| `Verification\Cpp2IL-method-map.log` | Fresh original-metadata load/export verification log |
| `Verification\Cpp2IL-method-map\method-pointer-map.json` | Fresh `206366`-entry export, byte-identical to the primary map |
| `GameplayEconomy\README.md` | Current native gameplay-economy findings and evidence index |
| `ghidra-gameplay-economy-native.log` | Ghidra decompile at verified native targets |
| `METADATA_DEOBFUSCATION_REPORT.md` | Chronology and decision record |

## RVA Rule

Do not use RVA comments from recovered managed DLLs as `GameAssembly.dll` RVAs. Export or query `Cpp2IL-method-map\method-pointer-map.json`, then use its `rva` field for native extraction and Ghidra work. The old `ghidra-gameplay-economy.log` used managed RVAs and is superseded by `ghidra-gameplay-economy-native.log`.

## Verification

Run reconstruction tests:

```powershell
Set-Location 'D:\SteamLibrary\steamapps\common\Everlusting Life'
py -3.13 -m unittest .\Analysis\test_metadata_reconstruction.py -v
```

Run plugin tests:

```powershell
Set-Location 'D:\VSCode\Cpp2IL\Cpp2IL'
dotnet test .\Cpp2IL.Plugin.Mfuscator.Tests\Cpp2IL.Plugin.Mfuscator.Tests.csproj --no-restore
```

Fresh load of the original metadata:

```powershell
Set-Location 'D:\SteamLibrary\steamapps\common\Everlusting Life'
& 'D:\VSCode\Cpp2IL\Cpp2IL\Cpp2IL\bin\Debug\net10.0\Cpp2IL.exe' `
  --force-binary-path '.\GameAssembly.dll' `
  --force-metadata-path '.\Everlusting Life_Data\il2cpp_data\Metadata\global-metadata.dat' `
  --force-unity-version '6000.1.4f1'
```

The fresh verification export uses `Cpp2IL-method-map.runtimeconfig.json` with `dotnet exec` and `--export-method-map`. It wrote `206366` entries and matched the primary map byte-for-byte at SHA-256 `D47E61D4A3D47F18E3FE5E25D225AECAA1FA9188ECE18D9E14415370185586F7`.

## Next Analysis Work

Metadata deobfuscation itself has no remaining mapping task. Continue focused native recovery from the method-pointer map. Current gameplay work is documented under `GameplayEconomy\`; generated Cpp2IL IL is context only when a method has decompiler warnings, with native instructions treated as authoritative.

Frida remains installed only for Python 3.13. Use `py -3.13` for any existing trace runner.
