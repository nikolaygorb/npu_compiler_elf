#!/bin/bash
# Fetch NNRT headers from vpux-plugin repo and create patch for 3rdparty/nnrt_api

# Usage: run from any directory, with VPUX_ROOT env variable set to vpux-plugin repo root
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)

if [ -z "$VPUX_ROOT" ]; then
  echo "Error: VPUX_ROOT environment variable not set."
  echo "Set VPUX_ROOT to vpux-plugin repo root, e.g. export VPUX_ROOT=/path/to/applications.ai.vpu-accelerators.vpux-plugin"
  exit 1
fi

SRC1="$VPUX_ROOT/src/vpux_compiler/include/vpux/compiler/NPU37XX/dialect/NPUReg37XX/firmware_headers/details/api"
SRC2="$VPUX_ROOT/src/vpux_compiler/include/vpux/compiler/NPU40XX/dialect/NPUReg40XX/firmware_headers/details/api"
DST_ROOT="$REPO_ROOT/3rdparty/nnrt_api"
DST="$DST_ROOT/api"
PATCH_OUT="$SCRIPT_DIR/nnrt_api.patch"

# Clean old
rm -rf "$DST_ROOT"

# Copy headers from both sources if exist
mkdir -p "$DST"
FOUND_SRC=0
if [ -d "$SRC1" ]; then
  cp -v "$SRC1"/*.h* "$DST" || true
  FOUND_SRC=1
fi
if [ -d "$SRC2" ]; then
  cp -v "$SRC2"/*.h* "$DST" || true
  FOUND_SRC=1
fi

if [ "$FOUND_SRC" -eq 0 ]; then
  echo "Error: header source directories not found under VPUX_ROOT=$VPUX_ROOT"
  exit 1
fi

# Create patch
TMPDIR=$(mktemp -d)

git diff --no-index --binary "$TMPDIR" "$DST_ROOT" > "$PATCH_OUT" || true

test -s "$PATCH_OUT" || { echo "Patch creation failed or empty"; exit 1; }

# Clean up
rm -rf "$TMPDIR"

# Remove 3rdparty after patch generation
rm -rf "$DST_ROOT"

echo "Patch created at $PATCH_OUT"
