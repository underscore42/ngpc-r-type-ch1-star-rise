#!/bin/bash
# install.sh — Extract R-Type Neo into the NGPC devkit build tree
# Run from: ~/projects/ngpc/devkit/

set -e

PROJ="ngpc-rtype"

echo "Installing R-Type Neo..."

# Create directory structure
mkdir -p "${PROJ}/src"
mkdir -p "${PROJ}/common"
mkdir -p "${PROJ}/lib"
mkdir -p "${PROJ}/lcf"
mkdir -p "${PROJ}/tools"
mkdir -p "${PROJ}/assets"
mkdir -p "${PROJ}/build"
mkdir -p "${PROJ}/bin"

echo "Directories created."
echo ""
echo "Now copy the framework files from the ameliandev template:"
echo "  cp ../ngpc-template/library.c   ${PROJ}/common/"
echo "  cp ../ngpc-template/library.h   ${PROJ}/common/"
echo "  cp ../ngpc-template/ngpc.h      ${PROJ}/common/"
echo "  cp ../ngpc-template/library.inc ${PROJ}/common/"
echo "  cp ../ngpc-template/system.lib  ${PROJ}/lib/"
echo ""
echo "Or from an existing project (e.g. ngp-asteroids):"
echo "  cp ngp-asteroids/common/*       ${PROJ}/common/"
echo "  cp ngp-asteroids/lib/system.lib ${PROJ}/lib/"
echo ""
echo "Then build:"
echo "  cd ${PROJ}"
echo "  make clean && make && make run"
