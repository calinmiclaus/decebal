#!/bin/sh

METISSEDIR=/opt/metisse

PATH="$PATH:$METISSEDIR/bin"

if [ -n "$PKG_CONFIG_PATH" ]                                                                                                                  
then                                                                                                                                          
  PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$METISSEDIR/lib/pkgconfig                                                                                  
else                                                                                                                                          
  PKG_CONFIG_PATH=$METISSEDIR/lib/pkgconfig                                                                                                   
fi

export PATH PKG_CONFIG_PATH
