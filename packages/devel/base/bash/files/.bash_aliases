#
# ~/.bash_aliases - bash aliases (user defined)
#

[ -r ~/.dircolors ] || eval `dircolors -b /etc/dircolors`
  
alias ls='ls --color=auto $LS_OPTIONS'                                                                                                                    
alias 'la'='ls -la'                                                                                                                                       
alias 'cls'='clear'                                                                                                                                       
alias 'cd..'='cd ..'                                                                                                                                      
alias '..'='cd ..'                                                                                                                                        
alias '...'='cd ../..'                                                                                                                                    
alias '~'='cd'
alias 'mc'='mc --color'
