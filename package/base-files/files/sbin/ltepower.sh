#!/bin/sh

resetmodule()
{
	echo Init > /tmp/sim-info
    echo 0 > /tmp/sig
    echo 1 > /sys/class/leds/modrst/brightness
	echo "$(date)-repower-4g-module" >> /tmp/repower-4g-module
	echo "$(date)-repower-4g-module" >> /tmp/at_failed_ret
    sleep 5
    echo 0 > /sys/class/leds/modrst/brightness
}

off_leds()
{
	echo none > /sys/class/leds/regstat/trigger
	echo 0 > /sys/class/leds/regstat/brightness

	echo none > /sys/class/leds/dialstatus/trigger
	echo 0 > /sys/class/leds/dialstatus/brightness
}

checkat()
{
	# other process has do at-cmd
	local num="$(ps |grep /etc/gcom |wc -l)"
	[ $num -gt 1 ] && return 1
	gcom -d "$1" -s /etc/gcom/checkpin.gcom > /tmp/sim-info
	if cat /tmp/sim-info | grep -qi 'Ready'; then
		[ ! -e "/tmp/sim_ready" ] && touch /tmp/sim_ready
	else
		logger -t ltecheck "Check:SIM-error"
		rm -f /tmp/dialok /tmp/sim_ready
		return 1
	fi
	
	num="$(ps |grep /etc/gcom |wc -l)"
	[ $num -gt 1 ] && return 1
	gcom -d "$1" -s /etc/gcom/getstrength.gcom > /tmp/sig

	num="$(ps |grep /etc/gcom |wc -l)"
	[ $num -gt 1 ] && return 1
	gcom -d "$1" -s /etc/gcom/check_status.gcom > /tmp/module_status_file
}

renew_signal_led_status()
{
	checkat "$1"
	sign=$(cat /tmp/sig)
	if [ $sign -gt 24 ]; then
		echo none > /sys/class/leds/regstat/trigger
		echo 1 > /sys/class/leds/regstat/brightness
	elif [ $sign -gt 16 ]; then
		echo timer > /sys/class/leds/regstat/trigger
		echo 500 > /sys/class/leds/regstat/delay_on
		echo 500 > /sys/class/leds/regstat/delay_off
	else
		echo timer > /sys/class/leds/regstat/trigger
		echo 200 > /sys/class/leds/regstat/delay_on
		echo 200 > /sys/class/leds/regstat/delay_off
	fi
}

rm -rf /tmp/gcom*
# ttyUSBx is not exist
[ ! -e "$1" ] && return 1

#enb="$(uci -q get qmicfg.qmicfg.enable)"
ifstatus qmi > /dev/null || return 1

renew_signal_led_status "$1"
local num="$(cat /tmp/cfuncount | wc -l)"
[ ! -e "/tmp/cfuncount" ] && num=0

[ $num -ge 20 ] && {
	logger -t ltecheck "cfuncount>20,to reset module"
	#cfun=0
	gcom -d "$1" -s /etc/gcom/ec20agpsend.gcom
	gcom -d "$1" -s /etc/gcom/clearcfun.gcom
	echo "reset module" > /tmp/cfuncount
	/etc/init.d/qmidial stop
	resetmodule
	off_leds
	sleep 20
}
