#!/bin/sh
#
# /etc/profile.d/openbox.sh - bash profile file for openbox
#

OPENBOXDIR=/opt/openbox

PATH="$PATH:$OPENBOXDIR/bin"
MANPATH="$MANPATH:$OPENBOXDIR/share/man"

if [ -n "$PKG_CONFIG_PATH" ]
then
  PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$OPENBOXDIR/lib/pkgconfig
else
  PKG_CONFIG_PATH=$OPENBOXDIR/lib/pkgconfig
fi

export PATH MANPATH PKG_CONFIG_PATH

