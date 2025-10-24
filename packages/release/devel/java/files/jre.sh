#!/bin/sh
#
# /etc/profile.d/jre.sh - enviroment and variables for the Java package
#

JAVA_HOME=/usr/lib/java

MANDIR="$MANDIR:$JAVA_HOME/man"

PATH="$PATH:$JAVA_HOME/jre/bin"

export JAVA_HOME MANDIR PATH

