#!/bin/sh
#
# /etc/profile.d/xfce.sh - bash profile file for openbox
#

XFCEDIR=/opt/xfce

PATH="$PATH:$XFCEDIR/bin"
MANPATH="$MANPATH:$XFCEDIR/share/man"

BROWSER="/opt/firefox/firefox"
TERMCMD="/usr/X11R6/bin/xterm"

if [ -n "$PKG_CONFIG_PATH" ]
then
  PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$XFCEDIR/lib/pkgconfig
else
  PKG_CONFIG_PATH=$XFCEDIR/lib/pkgconfig
fi

if [ -n "$LIBGLADE_MODULE_PATH" ]
then
  LIBGLADE_MODULE_PATH="$LIBGLADE_MODULE_PATH:$XFCEDIR/lib/libglade/2.0"
else
  LIBGLADE_MODULE_PATH="$XFCEDIR/lib/libglade/2.0"
fi

XDG_DATA_HOME=$HOME/.local/share
XDG_CONFIG_HOME=$HOME/.config
XDG_CACHE_HOME=$HOME/.cache

if [ -n "$XDG_DATA_DIRS" ]
then
  XDG_DATA_DIRS="$XDG_DATA_DIRS:$XFCEDIR/share"
else
  XDG_DATA_DIRS="/usr/share:$XFCEDIR/share"
fi

if [ -n "$XDG_CONFIG_DIRS" ]
then
  XDG_CONFIG_DIRS="$XDG_CONFIG_DIRS:/etc/opt/openbox/xdg"
else
  XDG_CONFIG_DIRS="/usr/share:/etc/opt/openbox/xdg"
fi

export LIBGLADE_MODULE_PATH PATH MANPATH BROWSER TERMCMD PKG_CONFIG_PATH XFCEDIR \
  XDG_DATA_HOME XDG_CONFIG_HOME XDG_CACHE_HOME XDG_DATA_DIRS XDG_CONFIG_DIRS
