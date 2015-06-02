#!/bin/bash

HAS_KDE=0
HAS_GNOME=0

if ! type "ring-kde" > /dev/null 2> /dev/null; then
  HAS_KDE=1
fi

if ! type "ring-gnome" > /dev/null 2> /dev/null; then
  HAS_GNOME=1
fi

#Only one client is installed
if [ $HAS_KDE == "0" ] && [ $HAS_GNOME == "1" ]; then
  ring-kde
  exit $?
elif [ $HAS_GNOME == "0" ]; then
  gnome-ring
  exit $?
fi

#Too bad, then check if KDE is running, else use the Gnome one
if [ "$(ps aux | grep kwin | grep -v grep)" != "" ]; then
  ring-kde
  exit $?
else
  gnome-ring
  exit $?
fi

echo "Ring not found" >&2
exit 1
