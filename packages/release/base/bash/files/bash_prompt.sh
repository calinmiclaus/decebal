#!/bin/sh
#
# /etc/profile.d/bash_prompt.sh - bash prompt configuration file
#

# Configure the bash prompt.
if [ -r ~/.bash_prompt ]
then
  . ~/.bash_prompt
else
  # PS1 is displayed when bash is executed interactively.
  # PS2 is displayed when bash needs more input to complete a command.
  
  # Posible variables (backslash-escaped special characters):
  #
  #  \a     an ASCII bell character (07)           
  #  \d     the date  in  "Weekday  Month  Date"  format (e.g., "Tue May 26")                                  
  #  \e     an ASCII escape character (033)                       
  #  \h     the hostname up to the first `.'                      
  #  \H     the hostname                                          
  #  \j     the  number of jobs currently managed by the shell                                                      
  #  \l     the basename of the shell's terminal device name                                                  
  #  \n     newline                                               
  #  \r     carriage return                                       
  #  \s     the  name  of  the shell, the basename of $0         
  #         (the portion following the final slash)               
  #  \t     the current time in 24-hour HH:MM:SS format           
  #  \T     the current time in 12-hour HH:MM:SS format           
  #  \@     the current time in 12-hour am/pm format              
  #  \u     the username of the current user                     
  #  \v     the version of bash (e.g., 2.00)                     
  #  \V     the release of bash,  version + patchlevel (e.g., 2.00.0)                                       
  #  \w     the current working directory                        
  #  \W     the  basename  of the current working directory
  #  \!     the history number of this command                   
  #  \#     the command number of this command                   
  #  \$     if the effective UID is 0, a #, otherwise a $
  #  \nnn   the  character  corresponding to the octal number nnn
  #  \\     a backslash                                          
  #  \[     begin a sequence of non-printing characters, which could be 
  #         used to embed a terminal control sequence into the prompt                        
  #  \]     end a sequence of non-printing characters            
  
  # A few colors (uncomment desired colors):
  # BLACK="\[\033[1;30m\]"
  # BLUE="\[\033[0;34m\]"
  # GREEN="\[\033[0;32m\]"
  # CYAN="\[\033[0;36m\]"
  # RED="\[\033[0;31m\]"
  # PURPLE="\[\033[0;35m\]"
  # BROWN="\[\033[0;33m\]"
  # LGRAY="\[\033[0;37m\]"
  # DGRAY="\[\033[1;30m\]"
  # LBLUE="\[\033[1;34m\]"
  # LGREEN="\[\033[1;32m\]"
  # LCYAN="\[\033[1;36m\]"
  # LRED="\[\033[1;31m\]"
  # LPURPLE="\[\033[1;35m\]"
  # YELLOW="\[\033[1;33m\]"
  # WHITE="\[\033[1;37m\]"
  # NORMAL="\[\033[0m\]"
  
  # Color BASH prompts:
  # PS1="${WHITE}[${GREEN}\u${LGRAY}@${GREEN}\h${LGRAY}:${LGREEN}\w${WHITE}]${GRAY}# "
  # PS1='${GREEN}[\u@\l:\w]${NORMAL}\$ '
  # PS1='${GREEN}[\u@\h:\W]${NORMAL}\$ '
  #
  
  [ $UID -eq 0 ] && PS1='[\u@\l:\w]\$ '
  [ $UID -ge 100 ] && PS1='[\u@\h:\W]\$ '
  
  PS2='> '
  
  export PS1 PS2
fi
