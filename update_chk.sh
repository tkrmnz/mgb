#!/bin/bash
echo "Looking for USB Drive"
if [ -e /dev/sda1 ]
then
  echo "Found USB Drive"
  if [ -e /media/usb/mbb_update/update.sh ]
  then
    	echo "Found Moonbots update files"
	sh /media/usb/mbb_update/update.sh
	read -p "wait"
  fi
fi


