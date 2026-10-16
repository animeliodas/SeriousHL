#!/bin/bash
# Build SeriousHL.dll (Serious Sam plugin) with the VS2022 v143 toolset against the Classics Patch 1.9.4 SDK.
# Usage: ./build.sh [Release_TFE105|Release_TSE107|all] [run]   (run: the speedrun build, dist/Run_<config>/)
# SDK: $SUPERPROJECT, or ../../_deps/SuperProject (git clone --branch 1.9.4 --recurse-submodules https://github.com/SamClassicPatch/SuperProject.git)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
SP_POSIX="$(cd "${SUPERPROJECT:-$HERE/../../_deps/SuperProject}" && pwd)"
SP="$(cygpath -w "$SP_POSIX")\\"
# the SeriousBridge core: $SERIOUSBRIDGE, or the folder "seriousbridge" next to this repository
SB_POSIX="$(cd "${SERIOUSBRIDGE:-$HERE/../../seriousbridge}" && pwd)"
SB="$(cygpath -w "$SB_POSIX")\\"
MSB="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/MSBuild.exe"
CFGS="${1:-all}"
RUN=false
[ "$2" = run ] && RUN=true
FLAVOR=""
[ "$RUN" = true ] && FLAVOR="Run_"
[ "$CFGS" = all ] && CFGS="Release_TFE105 Release_TSE107"

for CFG in $CFGS; do
  # import library of ClassicsCore.dll where the linker looks for it (pragma in Core/Core.h)
  mkdir -p "$SP_POSIX/Bin/$CFG"
  cp -n "$SP_POSIX/API/lib/classicscore.lib" "$SP_POSIX/Bin/$CFG/ClassicsCore.lib" 2>/dev/null || true
  "$MSB" "$(cygpath -w "$HERE/SeriousHL.vcxproj")" -p:Configuration="$CFG" -p:Platform=Win32 \
    -p:PlatformToolset=v143 -p:SuperProjectDir="$SP" -p:SolutionDir="$SP" -p:SeriousBridgeDir="$SB" -p:SeriousHLRun=$RUN -m -v:m -nologo
  echo "== built $FLAVOR$CFG: $HERE/dist/$FLAVOR$CFG/SeriousHL.dll"
done
