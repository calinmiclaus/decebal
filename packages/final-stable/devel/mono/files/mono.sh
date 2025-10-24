#
# /etc/profile.d/mono.sh - enviroment variables for mono
#

PATH="$PATH:/opt/mono/bin"
MANPATH="$MANPATH:/opt/mono/share/man"
MONO_PATH=/opt/mono/lib

if [ -n "$PKG_CONFIG_PATH" ]
then
  PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/opt/mono/lib/pkgconfig"
else
  PKG_CONFIG_PATH="/opt/mono/lib/pkgconfig"
fi

export PATH MANPATH MONO_PATH PKG_CONFIG_PATH
