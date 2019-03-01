#!/bin/sh
enb="$(uci -q get netset.@netset[0].enable)"
[ $enb = 0 ] && { 
	echo timer > /sys/class/leds/heart/trigger
	/etc/init.d/net4g stop
	return 0
}
echo none > /sys/class/leds/heart/trigger

/etc/init.d/net4g stop
sleep 1
/etc/init.d/net4g start

