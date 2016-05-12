#!/bin/bash

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Copies a framework to its new home and keep its version for MAS

set -e

RUN_INSTALL_NAME_TOOL=1
if [ $# -eq 3 ] && [ "${1}" = "-I" ] ; then
  shift
  RUN_INSTALL_NAME_TOOL=
fi

if [ $# -ne 2 ] ; then
  echo "usage: ${0} [-I] FRAMEWORK DESTINATION_DIR" >& 2
  exit 1
fi

# FRAMEWORK should be a path to a versioned framework bundle, ending in
# .framework.  DESTINATION_DIR is the directory that the unversioned framework
# bundle will be copied to.

FRAMEWORK="${1}"
DESTINATION_DIR="${2}"

FRAMEWORK_NAME="$(basename "${FRAMEWORK}")"
if [ "${FRAMEWORK_NAME: -10}" != ".framework" ] ; then
  echo "${0}: ${FRAMEWORK_NAME} does not end in .framework" >& 2
  exit 1
fi
FRAMEWORK_NAME_NOEXT="${FRAMEWORK_NAME:0:$((${#FRAMEWORK_NAME} - 10))}"

# Find the current version.
VERSIONS="${FRAMEWORK}/Versions"
CURRENT_VERSION_LINK="${VERSIONS}/Current"
CURRENT_VERSION_ID="$(readlink "${VERSIONS}/Current")"
CURRENT_VERSION="${VERSIONS}/${CURRENT_VERSION_ID}"

# Make sure that the framework's structure makes sense as a versioned bundle.
if [ ! -e "${CURRENT_VERSION}/${FRAMEWORK_NAME_NOEXT}" ] ; then
  echo "${0}: ${FRAMEWORK_NAME} does not contain a dylib" >& 2
  exit 1
fi

DESTINATION="${DESTINATION_DIR}/${FRAMEWORK_NAME}"

# Copy the entire framework to its destination location.
mkdir -p "${DESTINATION_DIR}"
rsync -acC --delete --exclude Headers --exclude PrivateHeaders \
    --include '*.so' "${FRAMEWORK}/" "${DESTINATION}"

if [[ -n "${RUN_INSTALL_NAME_TOOL}" ]]; then
  # Adjust the Mach-O LC_ID_DYLIB load command in the framework.  This does not
  # change the LC_LOAD_DYLIB load commands in anything that may have already
  # linked against the framework.  Not all frameworks will actually need this
  # to be changed.  Some frameworks may already be built with the proper
  # LC_ID_DYLIB for use as an unversioned framework.  Xcode users can do this
  # by setting LD_DYLIB_INSTALL_NAME to
  # $(DYLIB_INSTALL_NAME_BASE:standardizepath)/$(WRAPPER_NAME)/$(PRODUCT_NAME)
  # If invoking ld via gcc or g++, pass the desired path to -Wl,-install_name
  # at link time.
  FRAMEWORK_DYLIB="${DESTINATION}/${FRAMEWORK_NAME_NOEXT}"
  LC_ID_DYLIB_OLD="$(otool -l "${FRAMEWORK_DYLIB}" |
                         grep -A10 "^ *cmd LC_ID_DYLIB$" |
                         grep -m1 "^ *name" |
                         sed -Ee 's/^ *name (.*) \(offset [0-9]+\)$/\1/')"
  VERSION_PATH="/Versions/${CURRENT_VERSION_ID}/${FRAMEWORK_NAME_NOEXT}"
  LC_ID_DYLIB_NEW="$(echo "${LC_ID_DYLIB_OLD}" |
                     sed -Ee "s%${VERSION_PATH}$%/${FRAMEWORK_NAME_NOEXT}%")"

  if [ "${LC_ID_DYLIB_NEW}" != "${LC_ID_DYLIB_OLD}" ] ; then
    echo "install_name_tool -id ${LC_ID_DYLIB_NEW} ${FRAMEWORK_DYLIB}" >& 2
    install_name_tool -id "${LC_ID_DYLIB_NEW}" "${FRAMEWORK_DYLIB}"
  fi
fi
