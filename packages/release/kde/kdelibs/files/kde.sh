#!/bin/sh

KDEDIR=/opt/kde
PATH=$PATH:$KDEDIR/bin

if [ -n "$PKG_CONFIG_PATH" ]
then
  PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$KDEDIR/lib/pkgconfig"
else
  PKG_CONFIG_PATH="$KDEDIR/lib/pkgconfig"
fi

if [ -n "$LIBGLADE_MODULE_PATH" ]
then
  LIBGLADE_MODULE_PATH="$LIBGLADE_MODULE_PATH:$KDEDIR/lib/libglade/2.0"
else
  LIBGLADE_MODULE_PATH="$KDEDIR/lib/libglade/2.0"
fi

XDG_DATA_HOME=$HOME/.local/share
XDG_CONFIG_HOME=$HOME/.config
XDG_CACHE_HOME=$HOME/.cache

if [ -n "$XDG_DATA_DIRS" ]
then
  XDG_DATA_DIRS="$XDG_DATA_DIRS:$KDEDIR/share"
else
  XDG_DATA_DIRS="/usr/share:$KDEDIR/share"
fi

if [ -n "$XDG_CONFIG_DIRS" ]
then
  XDG_CONFIG_DIRS="$XDG_CONFIG_DIRS:/etc/opt/kde/xdg"
else
  XDG_CONFIG_DIRS="/usr/share:/etc/opt/kde/xdg"
fi

export MANPATH PKG_CONFIG_PATH LIBGLADE_MODULE_PATH KDEDIR PATH \
  XDG_DATA_HOME XDG_CONFIG_HOME XDG_CACHE_HOME XDG_DATA_DIRS XDG_CONFIG_DIRS
