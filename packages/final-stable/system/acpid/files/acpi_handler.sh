#!/bin/sh

set $*

case "$1" in
  button)
    case "$2" in
      power)
	/sbin/init 0
    	;;
      sleep)
	logger "ACPI action sleep is still in development"
	;;
      *)
        logger "ACPI action $2 is not defined"
    	;;
    esac
    ;;
  *)
   logger "ACPI group $1 / action $2 is not defined"
   ;;
esac
