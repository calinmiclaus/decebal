#!/bin/bash

BLACKBOXDIR=/opt/backbox

if [ -z "$PKG_CONFIG_PATH" ]
then
  PKG_CONFIG_PATH=/opt/blackbox/lib/pkgconfig
else
  PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/opt/blackbox/lib/pkgconfig"
fi

PATH="$PATH:$BLACKBOXDIR/bin"
MANPATH="$MANPATH:$BLACKBOXDIR/share/man"

export PATH PKG_CONFIG_PATH MANPATH

