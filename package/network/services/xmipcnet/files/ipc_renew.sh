#!/bin/sh

EXEC=xmipcnet

start() {
	enb="$(uci -q get ipc_cfg.cfg.enable)"
	ip="$(uci -q get ipc_cfg.cfg.ipaddr)"
	port="$(uci -q get ipc_cfg.cfg.port)"
	devid="$(uci -q get netset.@netset[0].deviceid)"
	[ -z "$devid" ] && {
		imei="$(cat /tmp/devimeiid)"
		if [ -s "/tmp/devimeiid" ]; then
			devid="$imei"
		else
			macaddr=$(cat /sys/class/net/eth0/address|awk -F ":" '{print $4""$5""$6 }'| tr a-z A-Z)
			[ -z "$macaddr" ] && macaddr="123456"
			devid="$macaddr"
		fi
	}
	# -d, run as daemon; -f, run foregroud
	[ ${enb} -gt 0 ] && /bin/${EXEC} $devid $ip $port -f &
}

stop() {
	killall -INT ${EXEC}
	sleep 1
	killall -9 ${EXEC}
}

stop
start
