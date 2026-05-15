# NNRT API Patch Creation

This folder contains artifacts used to restore test-only NNRT headers under `3rdparty/nnrt_api`.

- `nnrt_api.patch`: Git-style patch that recreates `3rdparty/nnrt_api`.
- `gen_nnrt_patch.sh`: Script that regenerates `nnrt_api.patch` from headers in the `vpux-plugin` repository.

## Why this exists

The project does not keep NNRT headers checked in as regular source files. For test and local tooling flows, CMake restores them from `cmake/patches/nnrt_api.patch` when needed.

In top-level CMake, this happens when one of the following options is enabled:

- `ENABLE_LOCAL_NNRT_API`
- `ENABLE_ELF_TESTS`
- `ENABLE_NPU_LOADER`

## Prerequisites

- `patch` utility installed (used by CMake to apply `nnrt_api.patch`).
- Access to a `vpux-plugin` checkout.
- `VPUX_ROOT` environment variable set to the root of that checkout.

Example:

```bash
export VPUX_ROOT=/path/to/applications.ai.vpu-accelerators.vpux-plugin
```

## Regenerate `nnrt_api.patch`

Run from any directory:

```bash
bash cmake/patches/gen_nnrt_patch.sh
```

What the script does:

1. Removes any previous `3rdparty/nnrt_api` content.
2. Copies headers from both known firmware-header API locations in `vpux-plugin` into `3rdparty/nnrt_api/api`.
3. Generates a binary-safe git-style diff into `cmake/patches/nnrt_api.patch`.
4. Removes temporary files and deletes the restored `3rdparty/nnrt_api` tree after the patch is produced.

## Verify patch application manually

Note: `gen_nnrt_patch.sh` does not apply the patch. It only creates `nnrt_api.patch`.
Applying the patch is done either by top-level CMake or manually with `patch`.

From repository root:

```bash
patch --forward -p1 -i cmake/patches/nnrt_api.patch
```

`-p1` is required because the patch uses git-style `a/` and `b/` prefixes.

To remove restored files afterward:

```bash
rm -rf 3rdparty/nnrt_api
```

## Troubleshooting

- `VPUX_ROOT environment variable not set`:
  Export `VPUX_ROOT` before running the script.
- `header source directories not found`:
  Verify `VPUX_ROOT` points to a compatible `vpux-plugin` checkout.
- `Failed to apply nnrt_api patch` during CMake:
  Ensure `patch` is installed and run from a clean tree where `3rdparty/nnrt_api` does not already contain conflicting content.
