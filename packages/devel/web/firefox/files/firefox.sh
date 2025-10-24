#!/bin/bash
#
# /etc/profile.d/firefox.sh - enviroment variables for firefox
#

FIREFOXDIR="/opt/firefox"

if [ -n "$LD_LIBRARY_PATH" ]
then
  LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$FIREFOXDIR/lib"
else
  LD_LIBRARY_PATH="$FIREFOXDIR/lib"
fi

if [ -n "$PKG_CONFIG_PATH" ]
then
  PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$FIREFOXDIR/lib/pkgconfig"
else
  PKG_CONFIG_PATH="$FIREFOXDIR/lib/pkgconfig"
fi

PATH="$PATH:$FIREFOXDIR/bin"

export PATH LD_LIBRARY_PATH
