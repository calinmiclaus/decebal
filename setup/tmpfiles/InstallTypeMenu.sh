#!/bin/bash
touch /tmp/setup/itm.sel
/usr/bin/dialog --backtitle "Decebal Linux 0.9.0rc2 (Agenor)" --title " Instalation Type " \
--default-item "`cat /tmp/setup/itm.sel`" \
--cancel-label "Back" --menu "\nSelect the instalation type. Note that \
instalation types differ by the packages selected to be installed. \
So, if you select an instalation type, the setup script will select \
or deselect the packages to be installed.\nIf you manually selected or \
deselected some packages and now you choose an instalation type, your \
selection will be lost.\n\n" 0 0 0 \
Desktop-Gnome "Gnome desktop" \
Desktop-KDE "KDE desktop" \
Desktop-Xfce "Xfce desktop" \
Desktop-Englightenment "Enlightenment desktop (beta)" \
Desktop "All the stuff you need as a home-user" \
Server "All you need for an all-purposed server" \
Full "Install everything" \
Minimal "Just the basic stuff (very basic)" \
2>/tmp/setup/itm.sel
echo $?>/tmp/setup/itm.but
