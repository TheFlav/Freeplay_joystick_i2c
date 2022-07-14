#!/usr/bin/env bash

dialog --title "Freeplay Joystick Configuration" \
	--yesno "Yes or No?" \
	15 60

RESP=$?
case $RESP in
	0) dialog --title "You Picked" --infobox "YES" 10 60;
		echo "Affirmative!";;
	1) dialog --title "You Picked" --infobox "NO" 10 60;
		echo "Negative!";;
	255) ;;
esac
