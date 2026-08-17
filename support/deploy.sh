#!/usr/bin/env bash
##########################################################################################
#                                                                                        #
# OpenSpace                                                                              #
#                                                                                        #
# Copyright (c) 2026                                                                     #
#                                                                                        #
# Permission is hereby granted, free of charge, to any person obtaining a copy of this   #
# software and associated documentation files (the "Software"), to deal in the Software  #
# without restriction, including without limitation the rights to use, copy, modify,     #
# merge, publish, distribute, sublicense, and/or sell copies of the Software, and to     #
# permit persons to whom the Software is furnished to do so, subject to the following    #
# conditions:                                                                            #
#                                                                                        #
# The above copyright notice and this permission notice shall be included in all copies  #
# or substantial portions of the Software.                                               #
#                                                                                        #
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,    #
# INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A          #
# PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT     #
# HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF   #
# CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE   #
# OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                          #
##########################################################################################
#
# Build and package a distributable Windows OpenSpace release from Git Bash.
#
# The bash counterpart to deploy.ps1. Differences:
#   - builds through ../build-llvm.sh (windows-llvm preset: clang-cl + lld-link,
#     Ninja Multi-Config, wrapped in vcvars64) instead of configuring a second
#     Visual Studio tree in build-deploy/
#   - non-destructive: stages into dist/stage/ and never touches the dev tree's bin/
#   - copies an explicit set out of bin/<config> rather than everything, because a dev
#     checkout accumulates .pdb, crash dumps and cefcache/ in there
#
# The shipped file list below is ported from deploy.ps1 and is the authoritative answer
# to "what does a runnable OpenSpace install contain".
#
# Usage:
#   support/deploy.sh                    # build, then package to dist/OpenSpace-<ver>.zip
#   support/deploy.sh --skip-build       # package whatever is already in bin/<config>
#   support/deploy.sh --verify           # ...and smoke-test the resulting archive
#   support/deploy.sh --stage-only       # produce dist/stage/ and stop
#
# The package contains no sync/ folder: a fresh install downloads its data on first run.
#
# Manual end-to-end test of a finished archive (not automated here):
#   - unzip somewhere writable, outside this repo and outside C:\Program Files
#     (OpenSpace does not survive Program Files, and it writes cache/, logs/ and
#     settings.json next to openspace.cfg)
#   - unset OPENSPACE_SYNC so the install downloads into its own sync/
#   - run bin\OpenSpace.exe, let the default profile load, then check logs/log.html for
#     missing-file or shader-compile errors and confirm ${BASE} points at the extracted
#     root rather than the development checkout
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

CONFIG="RelWithDebInfo"
TARGETS="OpenSpace AssetBuilder TaskRunner"
JOBS=""
SKIP_BUILD=0
NO_REDIST=0
WITH_PDB=0
PRUNE=0
STAGE_ONLY=0
KEEP_STAGE=0
VERIFY=0
OUT=""

REDIST_URL="http://aka.ms/vs/17/release/vc_redist.x64.exe"
# deploy.ps1's settings. Most of the payload is already-compressed media (data/assets is
# largely mp4/mp3), so -mx=1 finishes far faster for a few percent more size.
LEVEL=7

usage() {
  sed -n '/^# Build and package/,/^set -euo/p' "${BASH_SOURCE[0]}" | sed '$d;s/^# \?//'
  cat <<'EOF'

Options:
  --skip-build          package the existing bin/<config> without building
  -c, --config CFG      build config (default: RelWithDebInfo)
  -t, --targets "..."   build targets (default: "OpenSpace AssetBuilder TaskRunner")
  -j, --jobs N          parallel jobs, forwarded to build-llvm.sh
  --no-redist           skip downloading vc_redist.x64.exe
  --with-pdb            include *.pdb in the archive (adds roughly 2 GB)
  --prune               also drop codegen-tool.exe, Qt6Svg.dll, iconengines/,
                        imageformats/, networkinformation/ (deploy.ps1 behavior)
  --stage-only          produce dist/stage/ and stop, no archive
  --keep-stage          do not delete dist/stage/ after archiving
  --verify              extract the finished archive and smoke-test the binaries
  -o, --out PATH        output archive (default: dist/OpenSpace-<version>.zip)
  --level N             zip compression level 0-9 (default: 7, as deploy.ps1). Most of
                        the payload is already-compressed media, so --level 1 is much
                        faster for a few percent more size
  -h, --help            this text

Environment:
  SEVENZIP    path to 7z.exe (default: C:/Program Files/7-Zip/7z.exe, or 7z on PATH)
  VCVARS      forwarded to build-llvm.sh
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --skip-build)  SKIP_BUILD=1; shift ;;
    -c|--config)   CONFIG="$2"; shift 2 ;;
    -t|--targets)  TARGETS="$2"; shift 2 ;;
    -j|--jobs)     JOBS="$2"; shift 2 ;;
    --no-redist)   NO_REDIST=1; shift ;;
    --with-pdb)    WITH_PDB=1; shift ;;
    --prune)       PRUNE=1; shift ;;
    --stage-only)  STAGE_ONLY=1; shift ;;
    --keep-stage)  KEEP_STAGE=1; shift ;;
    --verify)      VERIFY=1; shift ;;
    -o|--out)      OUT="$2"; shift 2 ;;
    --level)       LEVEL="$2"; shift 2 ;;
    -h|--help)     usage; exit 0 ;;
    *) echo "error: unknown option '$1' (try --help)" >&2; exit 1 ;;
  esac
done

step() { echo; echo "=== $* ==="; }
die()  { echo "error: $*" >&2; exit 1; }

##########################################################################################
# 1. Preflight
##########################################################################################
step "Preflight"

[[ -f "$REPO_ROOT/openspace.cfg" ]] || die "'$REPO_ROOT' is not an OpenSpace checkout"

if [[ -n "${SEVENZIP:-}" ]]; then
  :
elif [[ -f "C:/Program Files/7-Zip/7z.exe" ]]; then
  SEVENZIP="C:/Program Files/7-Zip/7z.exe"
elif command -v 7z >/dev/null 2>&1; then
  SEVENZIP="$(command -v 7z)"
else
  die "7-Zip not found. Install it, or set SEVENZIP=/path/to/7z.exe"
fi
[[ -x "$SEVENZIP" || -f "$SEVENZIP" ]] || die "SEVENZIP='$SEVENZIP' is not executable"

# 7-Zip is a native Windows binary: keep MSYS from rewriting its arguments and hand it
# Windows paths explicitly.
sevenzip() { MSYS_NO_PATHCONV=1 "$SEVENZIP" "$@"; }

VERSION="$(git -C "$REPO_ROOT" describe --tags --always --dirty 2>/dev/null || true)"
if [[ -z "$VERSION" ]]; then
  VERSION="$(git -C "$REPO_ROOT" rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)"
  VERSION="$VERSION-$(git -C "$REPO_ROOT" log -1 --format=%h 2>/dev/null || echo 0000000)"
fi
VERSION="$(printf '%s' "$VERSION" | tr '/ ' '--')"

DIST="$REPO_ROOT/dist"
STAGE="$DIST/stage"
[[ -n "$OUT" ]] || OUT="$DIST/OpenSpace-$VERSION.zip"
case "$OUT" in /*|?:*) ;; *) OUT="$PWD/$OUT" ;; esac

[[ "$LEVEL" =~ ^[0-9]$ ]] || die "--level must be 0-9, got '$LEVEL'"
ZIP_FLAGS=(-tzip "-mx=$LEVEL")
if [[ "$LEVEL" -ge 5 ]]; then
  ZIP_FLAGS+=(-mfb=128 -mpass=5)
fi

echo "repo      : $REPO_ROOT"
echo "version   : $VERSION"
echo "config    : $CONFIG"
echo "7-Zip     : $SEVENZIP"
echo "archive   : $OUT"

##########################################################################################
# 2. Build
##########################################################################################
if [[ $SKIP_BUILD -eq 1 ]]; then
  step "Build (skipped)"
else
  step "Build"
  # windows-min and windows-llvm share ${sourceDir}/build, so a cache left behind by the
  # Visual Studio preset would be silently clobbered. Refuse instead.
  cache="$REPO_ROOT/build/CMakeCache.txt"
  if [[ -f "$cache" ]]; then
    gen="$(sed -n 's/^CMAKE_GENERATOR:INTERNAL=//p' "$cache" | head -1)"
    if [[ -n "$gen" && "$gen" != "Ninja Multi-Config" ]]; then
      die "build/ holds a '$gen' cache (the windows-min preset). Run './build-llvm.sh clean' first, or pass --skip-build."
    fi
  fi

  build_args=(all -c "$CONFIG" -t "$TARGETS")
  [[ -n "$JOBS" ]] && build_args+=(-j "$JOBS")
  "$REPO_ROOT/build-llvm.sh" "${build_args[@]}"
fi

##########################################################################################
# 3. Verify the pieces a runnable install needs
##########################################################################################
step "Checking build products"

BIN_SRC="$REPO_ROOT/bin/$CONFIG"
missing=()
for f in \
  "bin/$CONFIG/OpenSpace.exe" \
  "bin/$CONFIG/OpenSpace_Helper.exe" \
  "config/schema/sgct.schema.json" \
  "COMMIT.md" \
  "documentation/index.html" \
  "modules/webgui/ext/nodejs/node.exe" \
  "modules/globebrowsing/gdal_data"
do
  [[ -e "$REPO_ROOT/$f" ]] || missing+=("$f")
done
if [[ ${#missing[@]} -gt 0 ]]; then
  echo "error: missing content required by a runnable install:" >&2
  printf '  %s\n' "${missing[@]}" >&2
  cat >&2 <<'EOF'
  config/schema/sgct.schema.json and COMMIT.md are generated by CMake -> configure/build.
  documentation/ and modules/webgui/ext/nodejs/ are a submodule and a configure-time
  download -> run 'git submodule update --init documentation' and/or reconfigure.
EOF
  exit 1
fi

##########################################################################################
# 4. Stage bin/<config> as a flattened bin/
##########################################################################################
step "Staging bin/"

rm -rf "$STAGE"
mkdir -p "$STAGE/bin"

# Build artifacts and runtime scratter that must never ship. cefcache_locked/ additionally
# contains ACL'd subdirectories that make a plain 'cp -r' fail, hence tar with excludes:
# tar skips excluded directories instead of descending into them.
tar_excludes=(
  --exclude=./cefcache
  --exclude=./cefcache_locked
  --exclude=*.dmp
  --exclude=*.stackdump
  --exclude=OpenSpaceTest.exe
  --exclude=GhoulTest.exe
)
[[ $WITH_PDB -eq 1 ]] || tar_excludes+=(--exclude=*.pdb)
if [[ $PRUNE -eq 1 ]]; then
  tar_excludes+=(
    --exclude=codegen-tool.exe
    --exclude=Qt6Svg.dll
    --exclude=./iconengines
    --exclude=./imageformats
    --exclude=./networkinformation
  )
fi

tar -C "$BIN_SRC" "${tar_excludes[@]}" -cf - . | tar -C "$STAGE/bin" -xf -
echo "staged $(find "$STAGE/bin" -type f | wc -l) files, $(du -sh "$STAGE/bin" | cut -f1)"

##########################################################################################
# 5. Microsoft redistributable
##########################################################################################
# windeployqt runs with --no-compiler-runtime (apps/OpenSpace/CMakeLists.txt), so a clean
# target machine has no MSVC runtime.
if [[ $NO_REDIST -eq 1 ]]; then
  step "Redistributable (skipped)"
else
  step "Downloading the Microsoft redistributable"
  curl -fL --retry 3 --progress-bar -o "$STAGE/vc_redist.x64.exe" "$REDIST_URL" \
    || die "failed to download $REDIST_URL (use --no-redist to build without it)"
fi

if [[ $STAGE_ONLY -eq 1 ]]; then
  step "Done (--stage-only)"
  echo "staging tree: $STAGE"
  exit 0
fi

##########################################################################################
# 6. Archive
##########################################################################################
step "Creating $(basename "$OUT")"

mkdir -p "$(dirname "$OUT")"
rm -f "$OUT"
OUT_WIN="$(cygpath -w "$OUT")"

# The staged bin/ and the redistributable, which live at the archive root.
(
  cd "$STAGE"
  extra=()
  [[ -f vc_redist.x64.exe ]] && extra+=(vc_redist.x64.exe)
  sevenzip a "${ZIP_FLAGS[@]}" "$OUT_WIN" bin "${extra[@]}" >/dev/null
) || die "7-Zip failed while adding bin/"

# Everything else comes straight out of the source tree. Ported from deploy.ps1; the
# wildcards are quoted so 7-Zip expands them rather than bash.
# Need to manually add any new weird paths that don't match the wildcards below.
(
  cd "$REPO_ROOT"
  sevenzip a "${ZIP_FLAGS[@]}" "$OUT_WIN" \
    'config/*' \
    'data/*' \
    'documentation/*' \
    'scripts/*' \
    'shaders/*' \
    ACKNOWLEDGMENTS.md \
    CITATION.cff \
    COMMIT.md \
    CREDITS.md \
    LICENSE.md \
    openspace.cfg \
    README.md \
    'modules/*/shaders/*' \
    'modules/*/scripts/*' \
    'modules/globebrowsing/gdal_data/*' \
    'modules/molecule/ext/mold/src/shaders/*' \
    modules/webgui/ext/nodejs/node.exe \
    '-x!documentation/.git' >/dev/null
) || die "7-Zip failed while adding the data directories"

##########################################################################################
# 7. Manifest assertions
##########################################################################################
step "Checking the archive"

LIST="$(mktemp)"
trap 'rm -f "$LIST"' EXIT
sevenzip l -ba -slt "$OUT_WIN" \
  | awk '/^Path = /  { p = substr($0, 8) }
         /^Attributes = / { if ($0 !~ /D/) print p }' \
  | tr '\\' '/' > "$LIST"

fail=0
require() {
  grep -qx -- "$1" "$LIST" 2>/dev/null || grep -q "^$1" "$LIST" 2>/dev/null || {
    echo "  MISSING: $1" >&2; fail=1
  }
}
refuse() {
  if grep -q -- "$1" "$LIST"; then
    echo "  UNEXPECTED: $1 -> $(grep -m3 -- "$1" "$LIST" | tr '\n' ' ')" >&2; fail=1
  fi
}

for p in \
  bin/OpenSpace.exe bin/OpenSpace_Helper.exe bin/libcef.dll bin/Qt6Core.dll \
  bin/platforms/qwindows.dll bin/locales/en-US.pak bin/resources.pak bin/gdal241.dll \
  openspace.cfg config/default.json config/schema/sgct.schema.json \
  data/profiles/default.profile data/fonts/Roboto/Roboto-Regular.ttf \
  scripts/core_scripts.lua documentation/index.html \
  modules/base/shaders/ modules/atmosphere/shaders/ \
  modules/globebrowsing/gdal_data/ modules/webgui/ext/nodejs/node.exe \
  modules/molecule/ext/mold/src/shaders/ shaders/
do
  require "$p"
done
[[ $NO_REDIST -eq 1 ]] || require vc_redist.x64.exe
[[ $WITH_PDB -eq 1 ]] || refuse '\.pdb$'

for p in '\.dmp$' '\.stackdump$' '^bin/cefcache' 'OpenSpaceTest\.exe' 'GhoulTest\.exe' \
         '^documentation/\.git$' '^user/' '^src/' '^ext/' '^build/' '^notes/' '^wiki/'
do
  refuse "$p"
done

# Every module shader in the tree has to be in the archive: a module whose shaders are
# missing loses its ${MODULE_<NAME>} token and fails at runtime, not at package time.
want_shaders="$(cd "$REPO_ROOT" && find modules/*/shaders -type f 2>/dev/null | wc -l)"
got_shaders="$(grep -c '^modules/[^/]*/shaders/' "$LIST" || true)"
if [[ "$want_shaders" -ne "$got_shaders" ]]; then
  echo "  MISMATCH: modules/*/shaders has $want_shaders files, archive has $got_shaders" >&2
  fail=1
fi

[[ $fail -eq 0 ]] || die "archive contents failed validation"
echo "manifest OK ($(wc -l < "$LIST") files, $want_shaders module shaders)"

##########################################################################################
# 8. Smoke test
##########################################################################################
if [[ $VERIFY -eq 1 ]]; then
  step "Smoke-testing the packaged binaries"
  # Only bin/ is extracted: '--help' is handled by the commandline parser before the
  # configuration file is looked up and before any engine or GL initialization, so this
  # exercises exactly the thing most likely to be broken by packaging -- whether the whole
  # DLL closure (CEF, Qt, GDAL, curl, mpv) resolves from the packaged bin/ alone.
  VERIFY_DIR="$DIST/verify"
  rm -rf "$VERIFY_DIR"
  mkdir -p "$VERIFY_DIR"
  sevenzip x -y -o"$(cygpath -w "$VERIFY_DIR")" "$OUT_WIN" 'bin/*' >/dev/null \
    || die "failed to extract bin/ from the archive"

  rc=0
  out="$("$VERIFY_DIR/bin/OpenSpace.exe" --help 2>&1)" || rc=$?
  if [[ $rc -ne 0 ]]; then
    echo "$out" >&2
    die "packaged OpenSpace.exe failed to start (exit $rc)"
  fi
  grep -q -- '--bypassLauncher' <<<"$out" \
    || die "OpenSpace.exe --help ran but printed unexpected output"
  echo "OpenSpace.exe --help OK"
  rm -rf "$VERIFY_DIR"
fi

##########################################################################################
# 9. Report
##########################################################################################
[[ $KEEP_STAGE -eq 1 ]] || rm -rf "$STAGE"

step "Done"
echo "archive : $OUT"
echo "size    : $(du -h "$OUT" | cut -f1)"
echo "sha256  : $(sha256sum "$OUT" | cut -d' ' -f1)"
