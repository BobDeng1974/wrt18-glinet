#!/bin/sh

[ -e "$1" ] && {
	gcom -d "$1" -s /etc/gcom/ec20agpsend.gcom
	gcom -d "$1" -s /etc/gcom/clearcfun.gcom
}
echo Reboot > /tmp/sim-info
echo 0 > /tmp/sig
echo 1 > /sys/class/leds/modpwr/brightness
/etc/init.d/net4g stop
reboot
