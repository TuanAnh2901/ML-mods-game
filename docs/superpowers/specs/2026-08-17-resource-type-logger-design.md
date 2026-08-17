# ResourceType Logger Design

**Status:** Approved design — August 17, 2026

## Goal

Replace hard-coded `ResourceType` probing in `ResourceDump` with runtime enum discovery, while preserving deterministic fallback output when managed discovery is unavailable.

## Scope

This feature runs once after IL2CPP runtime initialization. It enumerates resource enum values, resolves each value to its symbolic resource ID and display name, writes structured output, and reports runtime status to existing feature UI.

This feature does not enumerate item catalog data and does not change remote logger, injector, hook, or UI architecture.

## Current Evidence

Existing implementation: `el_native/features/resource_dump.cpp`.

Existing dump behavior:

- Resolves `UserResourcesUtil::GetStringByResourceType` and `UserResourcesUtil::GetResourceName`.
- Probes integer values `0` through `400`.
- Writes `id<TAB>name` rows.
- Stops feature after one successful invocation.

Verified Cpp2IL method RVAs:

| Method | RVA | Purpose |
|---|---:|---|
| `UserResourcesUtil::GetAllResourceTypes` | `0xBCC820` | Returns managed `List<ResourceType>` for runtime discovery. |
| `UserResourcesUtil::GetStringByResourceType` | `0xBCD090` | Maps `ResourceType` enum value to symbolic resource ID. |
| `UserResourcesUtil::GetResourceName` | `0xBCCB00` | Maps `ResourceType` enum value to display/localized name. |
| `ItemsDataHelper::GetItem` | `0x6AFC30` | Item catalog lookup. Out of first-pass scope. |
| `ItemData::get_UniqueId` | `0x5CAFA0` | Item metadata accessor. Out of first-pass scope. |
| `ItemData::GetTitle` | `0x695E10` | Item metadata accessor. Out of first-pass scope. |

`ItemsDataHelper` and `ItemData` are deliberately excluded from first pass: their item-catalog semantics differ from `ResourceType` enumeration.

## Architecture

`ResourceDump` uses hybrid discovery:

1. Resolve and call `GetAllResourceTypes`.
2. Validate returned managed `List<ResourceType>` using IL2CPP list layout helpers.
3. Enumerate real `ResourceType` values from list storage.
4. For each value, resolve resource ID and display name through existing string/name methods.
5. If any runtime discovery prerequisite fails, enumerate a generated static fallback table from Cpp2IL artifacts.
6. Normalize, deduplicate, sort, write output, then publish execution status.

Runtime discovery is preferred. Static data exists only as fallback.

## Components

### `ResourceDump`

Owns one-shot lifecycle, function resolution, discovery-source selection, output writing, error isolation, and UI status.

### Managed list reader

Small helper local to `resource_dump.cpp`, or private helper module if source complexity requires it.

Responsibilities:

- Validate non-null list pointer.
- Read `size` and backing-array pointer using project IL2CPP offsets/layout conventions.
- Reject negative, implausibly large, or backing-array-inconsistent counts.
- Copy integer enum entries to native storage before invoking per-entry managed methods.

The helper never trusts managed list count blindly. A capped maximum prevents corrupt pointers from causing unbounded reads.

### Fallback resource table

Compile-time generated `ResourceType` integer table derived from Cpp2IL artifact data for target build.

Each generated table carries:

- Target artifact/build identifier.
- Generator/source revision metadata.
- Ordered integer enum values.

Static fallback must not invent names. It only supplies candidate enum values; name and symbolic-ID resolution still use runtime methods when available.

### Entry resolver

For each candidate `ResourceType`:

1. Call `GetStringByResourceType(value)`.
2. Convert returned managed string to UTF-8 through existing project helpers.
3. Call `GetResourceName(value)`.
4. Convert returned managed string to UTF-8.
5. Mark entry skipped when symbolic ID is empty or conversion fails.

Display name may be empty and remains representable in output when symbolic ID exists.

### Output writer

Writes two files under `logs/`:

- Compatibility file: `resource_dump.tsv` with `enum_value<TAB>resource_id` rows.
- Detail file: `resource_dump_detail.tsv` with `enum_value<TAB>resource_id<TAB>display_name<TAB>source<TAB>status` rows.

Fields use TSV escaping for tab, carriage return, and newline. UTF-8 without BOM.

Rows sort ascending by `enum_value`, then lexicographically by `resource_id`. Output remains deterministic independent of managed-list ordering.

## Data Model

Native entry model:

```cpp
struct ResourceDumpEntry {
    int32_t enum_value;
    std::string resource_id;
    std::string display_name;
    std::string source; // "runtime" or "fallback"
    std::string status; // "resolved" or explicit skip/failure label
};
```

Deduplication:

- Primary key: `enum_value`.
- Secondary collision key: non-empty `resource_id`.
- If same enum value appears multiple times, retain first successfully resolved entry.
- If distinct enum values resolve to same `resource_id`, retain both rows and log collision count; enum identity remains authoritative.

## Failure Handling

### Runtime discovery fallback triggers

Select fallback table when any condition occurs:

- `GetAllResourceTypes` cannot resolve.
- Managed invocation throws a structured exception.
- Returned list pointer is null.
- List validation rejects size/backing array.
- List produces zero valid candidate values.

### Per-entry failures

Each entry resolves independently inside structured exception handling. A bad enum value, managed exception, null string, or string-conversion fault increments skipped count and does not abort entire dump.

### No usable output

If runtime discovery and fallback both produce zero resolvable entries:

- Write detail file with header and terminal status row.
- Do not write a partial compatibility file without a headerless valid row set.
- Set UI status to failure with source and error category.
- Keep one-shot feature disabled after reporting; repeated polling must not spam logs.

## Feature Status

Existing `ResourceDump` UI status includes:

- `source=runtime` or `source=fallback`.
- `candidates=<count>`.
- `resolved=<count>`.
- `skipped=<count>`.
- `resource-id-collisions=<count>` when non-zero.
- Output paths.
- Terminal failure category when no usable entries resolve.

Examples:

```text
Resource dump: source=runtime candidates=87 resolved=86 skipped=1
Resource dump: resource_dump.tsv, resource_dump_detail.tsv
```

```text
Resource dump: source=fallback candidates=92 resolved=92 skipped=0
```

## Generation and Versioning

Fallback-table generator is a development-time tool, not injected runtime code.

Inputs:

- Current target Cpp2IL dump/metadata.
- Explicit `ResourceType` enum declaration or equivalent recovered enum-value source.

Output:

- Checked-in generated C++ header/table.
- Stable source/build metadata comment.

Regenerate fallback data when game metadata changes or `GetAllResourceTypes` becomes unavailable/changes signature. Runtime discovery remains primary so minor enum additions work without regeneration when method and list layout remain valid.

## Testing Strategy

### Native unit tests or seams

Extract pure logic into testable native helpers where project test infrastructure permits:

- TSV field escaping.
- Sorting and deduplication.
- Candidate source selection.
- List validation boundaries.
- Per-entry result aggregation.

### Runtime smoke checks

Run injected build against target runtime and verify:

1. Runtime source selected when `GetAllResourceTypes` resolves.
2. Compatibility and detail files exist.
3. Detail rows have five escaped TSV fields.
4. No duplicate enum values.
5. Detail/source counts match UI terminal status.
6. Forced missing/invalid discovery method selects fallback source.
7. One deliberately invalid candidate increments skipped count but preserves resolved rows.

### Regression checks

- No `0..400` hard-coded discovery loop remains.
- Item catalog accessors remain unused in first-pass resource logger.
- Existing build succeeds with generated fallback header included.

## Non-Goals

- Mapping resource IDs to `ItemData` titles or unique IDs.
- Dynamic API scanning beyond specified runtime method pointers.
- Long-running watcher, periodic refresh, or UI redesign.
- Changing existing hook initialization sequence.

## Acceptance Criteria

- Runtime `GetAllResourceTypes` discovery replaces range probing.
- Fallback candidate table activates only on invalid/unavailable runtime discovery.
- Each candidate resolves resource ID and display name independently.
- Output is deterministic, UTF-8 TSV, deduplicated by enum value, and has compatibility plus detail files.
- UI reports source, candidate/resolved/skipped counts, output paths, and terminal failure category when needed.
- One bad value cannot terminate entire dump.
- First pass does not call `ItemsDataHelper::GetItem`, `ItemData::get_UniqueId`, or `ItemData::GetTitle`.
