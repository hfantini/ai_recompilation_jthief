#!/usr/bin/env bash
# Imports original/JWLTHIEF.EXE into a Ghidra project, replacing any previous
# import, and runs the Win16 fixups around auto-analysis.
#
# Usage: tools/ghidra/analyze.sh [project-dir]
#
# The project lives outside the repository by default (<repo>.ghidra next to
# it): it embeds the original binary, which must never be committed.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
repo_name="$(basename "$repo_root")"
project_dir="${1:-$(dirname "$repo_root")/$repo_name.ghidra}"

exec ghidra-headless "$project_dir" "$repo_name" \
	-import "$repo_root/original/JWLTHIEF.EXE" \
	-overwrite \
	-scriptPath "$repo_root/tools/ghidra/scripts" \
	-preScript PrepareWin16.java \
	-postScript FixupWin16.java
