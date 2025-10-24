#!/bin/bash
#
# /etc/profile.d/xorg.sh - enviroment and variables for xorg
#

XORGDIR=/usr/X11R6

PATH="$PATH:$XORGDIR/bin"
MANPATH="$MANPATH:$XORGDIR/man"

XDG_DATA_HOME=$HOME/.local/share
XDG_CONFIG_HOME=$HOME/.config
XDG_CACHE_HOME=$HOME/.cache

if [ -z "$PKG_CONFIG_PATH" ]
then
  PKG_CONFIG_PATH="$XORGDIR/lib/pkgconfig"
else
  PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$XORGDIR/lib/pkgconfig"
fi
  
if [ -z "$XDG_DATA_DIRS" ]
then
  XDG_DATA_DIRS=/usr/share
else
  XDG_DATA_DIRS="/usr/share:$XDG_DATA_DIRS"
fi

if [ -z "$XDG_CONFIG_DIRS" ]
then
  XDG_CONFIG_DIRS="/etc/xdg"
else
  XDG_CONFIG_DIRS="/etc/xdg:$XDG_CONFIG_DIRS"
fi

export XORGDIR PATH MANPATH PKG_CONFIG_PATH XDG_DATA_HOME \
  XDG_CONFIG_HOME XDG_CACHE_HOME XDG_DATA_DIRS XDG_CONFIG_DIRS
