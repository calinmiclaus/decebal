#!/bin/sh
#
# /etc/profile.d/enlightenment - enviroment variables for enlightenment
#

ENLIGHTENMENT_PREFIX=/opt/enlightenment
PATH="$PATH:$ENLIGHTENMENT_PREFIX/bin:$ENLIGHTENMENT_PREFIX/sbin"
MANPATH="$MANPATH:$ENLIGHTENMENT_PREFIX/share/man"

if [ -n "$PKG_CONFIG_PATH" ]
then
  PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$ENLIGHTENMENT_PREFIX/lib/pkgconfig"
else
  PKG_CONFIG_PATH="$ENLIGHTENMENT_PREFIX/lib/pkgconfig"
fi

export ENLIGHTENMENT_PREFIX PATH MANPATH PKG_CONFIG_PATH    