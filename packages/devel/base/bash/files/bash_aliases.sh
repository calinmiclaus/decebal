#!/bin/sh
#
# /etc/profile.d/bash_aliases.sh - bash aliases
#

if [ -r ~/.bash_aliases ]; then
  . ~/.bash_aliases
else
  alias ls='ls --color=auto $LS_OPTIONS'
  alias 'la'='ls -la'
  alias 'cls'='clear'
  alias 'cd..'='cd ..'
  alias '..'='cd ..'
  alias '...'='cd ../..'
  alias '~'='cd'
  alias 'mc'='mc --color'
fi
