cmake_minimum_required(VERSION 3.20)
project(dumpDotInterpolate CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# OpenFOAM environment
# Source /home/arv/OpenFOAM_Flavours/OpenFOAM-13-GCC-Opt-G/etc/bashrc
# in your shell before launching CLion, so these env vars are populated.
set(FOAM_SRC "/home/arv/OpenFOAM_Flavours/OpenFOAM-13-GCC-Opt-G/src")
set(WM_PROJECT_DIR "/home/arv/OpenFOAM_Flavours/OpenFOAM-13-GCC-Opt-G")

if(NOT FOAM_SRC)
    message(WARNING
            "FOAM_SRC is empty. Source the OpenFOAM bashrc before launching CLion:\n"
            "  source /home/arv/OpenFOAM_Flavours/OpenFOAM-13-GCC-Opt-G/etc/bashrc")
endif()

# OpenFOAM standard compile definitions
# These mirror what wmake passes; check $WM_PROJECT_DIR/wmake/rules/General/general
# if anything looks off for your build.
add_compile_definitions(
        WM_LABEL_SIZE=32
        WM_DP
        NoRepository
        OPENFOAM=2306                     # adjust to your OpenFOAM version
        linux64
)

# Include paths — OpenFOAM's lnInclude directories per module
include_directories(SYSTEM
        ${FOAM_SRC}/OpenFOAM/lnInclude
        ${FOAM_SRC}/OSspecific/POSIX/lnInclude
        ${FOAM_SRC}/finiteVolume/lnInclude
        ${FOAM_SRC}/meshTools/lnInclude
)

# The "executable" — CLion only uses this for indexing.
# It will fail to link (no OpenFOAM .so's referenced) — that's fine, ignore the error.
add_executable(dumpDotInterpolate
        dumpDotInterpolate.C
        dumpDotInterpolate.C
        dumpDotInterpolate.C
        dumpDotInterpolate.C
)