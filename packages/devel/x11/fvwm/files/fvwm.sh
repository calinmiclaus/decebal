#!/bin/sh
#
# /etc/profile.d/fvwm.sh - enviroment and variables for fvwm
#

FVWMDIR=/opt/fvwm

PATH="$PATH:$FVWMDIR/bin"
MANPATH="$MANPATH:$FVWMDIR/share/man"

export PATH MANPATH
