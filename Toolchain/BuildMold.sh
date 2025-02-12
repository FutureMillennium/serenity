#!/usr/bin/env bash

# This script builds the mold linker that can optionally be used for linking
# the SerenityOS userland.
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# shellcheck source=/dev/null
. "${DIR}/../Meta/shell_include.sh"

exit_if_running_as_root "Do not run BuildMold.sh as root, parts of your Toolchain directory will become root-owned"

NPROC="nproc"
SYSTEM_NAME="$(uname -s)"

if [ "$SYSTEM_NAME" = "OpenBSD" ]; then
    NPROC="sysctl -n hw.ncpuonline"
elif [ "$SYSTEM_NAME" = "FreeBSD" ]; then
    NPROC="sysctl -n hw.ncpu"
elif [ "$SYSTEM_NAME" = "Darwin" ]; then
    NPROC="sysctl -n hw.ncpu"
fi

[ -z "$MAKEJOBS" ] && MAKEJOBS=$($NPROC)

mkdir -p "$DIR"/Tarballs
pushd "$DIR"/Tarballs

if [ "$1" = "--git" ]; then
    [ ! -d mold ] && git clone https://github.com/rui314/mold.git

    cd mold

    git pull
else
    VERSION=1.5.1
    [ ! -e mold-$VERSION.tar.gz ] && curl -L "https://github.com/rui314/mold/archive/refs/tags/v$VERSION.tar.gz" -o mold-$VERSION.tar.gz
    [ ! -e mold-$VERSION ] && tar -xzf mold-$VERSION.tar.gz
    cd mold-$VERSION
fi

MOLD_BUILD="$DIR"/Build/mold 
cmake -B "$MOLD_BUILD" -S. -DCMAKE_INSTALL_PREFIX="$DIR"/Local/mold
make -C "$MOLD_BUILD" install -j"$MAKEJOBS"

popd
