#!/usr/bin/env bash
# Configure and/or build the OpenSpace "windows-llvm" preset (clang-cl + lld-link,
# Ninja Multi-Config) from Git Bash on Windows.
#
# Everything runs inside the VS x64 developer environment (vcvars64): clang-cl needs the
# Windows SDK + MSVC headers/libs, and the webbrowser POST_BUILD manifest step needs the
# SDK's mt.exe on PATH. A bare `cmake --build` outside that env fails at mt.exe.
#
# Usage:
#   ./build-llvm.sh                  # configure if needed, then build (RelWithDebInfo)
#   ./build-llvm.sh configure        # configure only
#   ./build-llvm.sh build            # build only
#   ./build-llvm.sh rebuild          # configure, then build
#   ./build-llvm.sh clean            # remove the build/ directory
#   ./build-llvm.sh build -c Debug           # build the Debug config
#   ./build-llvm.sh build -t OpenSpace -j 8  # specific target / parallel jobs
#   ./build-llvm.sh build -- --verbose       # pass remaining args to `cmake --build`
#
# Override the VS install with: VCVARS="/c/path/to/vcvars64.bat" ./build-llvm.sh
set -euo pipefail

PRESET="windows-llvm"
CONFIG="RelWithDebInfo"
TARGET=""
JOBS=""
PASSTHRU=()

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_WIN="$(cygpath -w "$SCRIPT_DIR")"
BUILD_DIR="$SCRIPT_DIR/build"

# Locate vcvars64.bat (override with the VCVARS env var)
VCVARS="${VCVARS:-}"
if [[ -z "$VCVARS" ]]; then
  for v in \
    "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" \
    "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat" \
    "C:/Program Files/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat" \
    "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Auxiliary/Build/vcvars64.bat"; do
    [[ -f "$v" ]] && { VCVARS="$v"; break; }
  done
fi
[[ -n "$VCVARS" && -f "$VCVARS" ]] || {
  echo "error: vcvars64.bat not found; set VCVARS=/path/to/vcvars64.bat" >&2; exit 1
}
VCVARS_WIN="$(cygpath -w "$VCVARS")"

ACTION="all"
case "${1:-}" in
  configure|build|rebuild|clean|all) ACTION="$1"; shift ;;
esac

while [[ $# -gt 0 ]]; do
  case "$1" in
    -c|--config) CONFIG="$2"; shift 2 ;;
    -t|--target) TARGET="$2"; shift 2 ;;
    -j|--jobs)   JOBS="$2"; shift 2 ;;
    --) shift; PASSTHRU=("$@"); break ;;
    *) echo "error: unknown option '$1'" >&2; exit 1 ;;
  esac
done

# Run a command line inside the dev environment, at the repo root. The sequence is written
# to a temp .bat and executed as a single file, which avoids cmd /C's nested-quote mangling
# of the (space-containing) vcvars and repo paths.
devrun() {
  local bat rc
  bat="$(mktemp --suffix=.bat)"
  {
    echo "@echo off"
    echo "call \"$VCVARS_WIN\" >nul || exit /b 1"
    echo "cd /d \"$REPO_WIN\" || exit /b 1"
    echo "$*"
    echo "exit /b %ERRORLEVEL%"
  } > "$bat"
  MSYS_NO_PATHCONV=1 cmd /c "$(cygpath -w "$bat")"
  rc=$?
  rm -f "$bat"
  return $rc
}

do_configure() {
  echo ">> configure: preset '$PRESET'"
  devrun cmake --preset "$PRESET"
}

do_build() {
  echo ">> build: preset '$PRESET' ($CONFIG)${TARGET:+ target=$TARGET}"
  local cmd="cmake --build --preset $PRESET --config $CONFIG"
  [[ -n "$TARGET" ]] && cmd+=" --target $TARGET"
  [[ -n "$JOBS" ]]   && cmd+=" -j $JOBS"
  [[ ${#PASSTHRU[@]} -gt 0 ]] && cmd+=" ${PASSTHRU[*]}"
  devrun "$cmd"
}

case "$ACTION" in
  configure) do_configure ;;
  build)     do_build ;;
  rebuild)   do_configure; do_build ;;
  clean)     echo ">> clean: removing $BUILD_DIR"; rm -rf "$BUILD_DIR" ;;
  all)       [[ -f "$BUILD_DIR/CMakeCache.txt" ]] || do_configure; do_build ;;
esac
echo ">> done"
