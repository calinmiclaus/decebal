#!/bin/bash
#
# /etc/profile.d/thunderbird.sh - enviroment and variables for thunderbird
#

THUNDERBIRDDIR="/opt/thunderbird"

if [ -n "$LD_LIBRARY_PATH" ]
then
  LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/opt/thunderbird/lib"
else
  LD_LIBRARY_PATH="/opt/thunderbird/lib"
fi

if [ -n "$PKG_CONFIG_PATH" ]
then
  PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$THUNDERBIRDDIR/lib/pkgconfig"
else
  PKG_CONFIG_PATH="$THUNDERBIRDDIR/lib/pkgconfig"
fi

PATH="$PATH:$THUNDERBIRDDIR/bin"

export PATH LD_LIBRARY_PATH PKG_CONFIG_PATH

