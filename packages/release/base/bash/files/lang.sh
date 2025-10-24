#!/bin/sh
#
# /etc/profile.d/lang.sh - set internationalization variables (i8n)
#

# Set the locale variable for all categories.
LANG="en_US"

# It is recommanded that you modify only LANG if needed, leave
# the settings below unchanged unless you know what you are doing.

# Localedata for classification and conversion of characters.
# LC_CTYPE=""

# Localedata for the sort order of strings.
# LC_COLLATE=""

# Localedata for translation of yes and no messages.
# LC_MESSAGES=""

# Localedata for monetary data.
# LC_MONETARY=""

# Localedata for non-monetary numerica data.
# LC_NUMERIC=""

# Localedata for date and time.
# LC_TIME=""

# All localedata setting (will overwrite every locale setting).
# LC_ALL="en_US"

# A separated list of ISO language codes.
# Ex: 
# LANGUAGE=""

for LOCALEDATA in LANG LC_CTYPE LC_COLLATE LC_MESSAGES LC_MONETARY \
                  LC_NUMERIC LC_TIME LC_ALL LANGUAGE
do
  if [ -n "$LOCALEDATA" ]
  then
    export $LOCALEDATA
  fi
done
