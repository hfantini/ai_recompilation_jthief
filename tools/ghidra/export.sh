#!/usr/bin/env bash
# Exports the analyzed JWLTHIEF.EXE from the Ghidra project as plain text:
# decompiled C per function and TSV indexes of functions, globals and strings.
# Run tools/ghidra/analyze.sh first. The project is opened read-only.
#
# Usage: tools/ghidra/export.sh [output-dir] [project-dir]
#
# Output lives outside the repository by default (<repo>.re/export next to
# it): decompiled code derives from the original and must never be committed.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
repo_name="$(basename "$repo_root")"
parent_dir="$(dirname "$repo_root")"
output_dir="${1:-$parent_dir/$repo_name.re/export}"
project_dir="${2:-$parent_dir/$repo_name.ghidra}"

# Stale files from functions renamed since the last export would linger.
rm -rf "$output_dir"
mkdir -p "$output_dir"

exec ghidra-headless "$project_dir" "$repo_name" \
	-process JWLTHIEF.EXE \
	-noanalysis \
	-readOnly \
	-scriptPath "$repo_root/tools/ghidra/scripts" \
	-postScript ExportProgram.java "$output_dir"
