#!/bin/sh

[ -n "$INCLUDE_ONLY" ] || {
	. /lib/functions.sh
	. ../netifd-proto.sh
	init_proto "$@"
}

proto_qmi_init_config() {
	available=1
	no_device=1
	proto_config_add_string "device:device"
	proto_config_add_string apn
	proto_config_add_string auth
	proto_config_add_string username
	proto_config_add_string password
	proto_config_add_string pincode
	proto_config_add_int delay
	proto_config_add_string modes
	proto_config_add_string pdptype
	proto_config_add_int profile
	proto_config_add_boolean dhcpv6
	proto_config_add_boolean autoconnect
	proto_config_add_int plmn
	proto_config_add_defaults
}

_get_info_status_by_at() {
	rm -f /tmp/dialok
	date >> /tmp/dialcount
	local at_port="$1"
	local modem_type nettype
	local cardinfo=$(gcom -d "$at_port" -s /etc/gcom/getcardinfo.gcom)
	if echo "$cardinfo" | grep -q EC20; then
		modem_type="EC20"
	else
		#unknow USB_Module
		logger -t qmi-at "unknow usb module:$cardinfo"
		return 1
	fi
	
	[ ! -e "/tmp/modem_type" ] && echo $modem_type > /tmp/modem_type
			
	# check sim card
	local simst=$(gcom -d "$at_port" -s /etc/gcom/checkpin.gcom)
	echo "simcard=$simst" > /tmp/3g-info
	echo $simst > /tmp/sim-info

	if echo "$simst" | grep -qi Ready; then
		# sim card ready.to dial ppp
		[ ! -e "/tmp/sim_ready" ] && touch /tmp/sim_ready
		# LED blink,gpio43
		echo timer > /sys/class/leds/dialstatus/trigger
	else
		#SIM ready to no
		rm -f /tmp/sim_ready
		logger -t qmi-at "!!! Has NO SIM Card !!!"
		echo none > /sys/class/leds/dialstatus/trigger
		return 1
	fi
	
	local sysinfo=$(gcom -d "$at_port" -s /etc/gcom/sysinfo.gcom)
	if echo "$sysinfo" | grep -qi 'NO,'; then
		logger -t qmi-at "!!! SYSINFO FAILED !!! $sysinfo"
		# check CSQ  SIMCARD PSRAR  Creg CEREG
		gcom -d "$at_port" -s /etc/gcom/check_status.gcom > /tmp/at_failed_ret
		return 1
	fi
	
	if [ "$modem_type" = "EC20" ]; then
		nettype=$(gcom -d "$at_port" -s /etc/gcom/ec20net.gcom)
	fi
	
	if echo "$nettype" |grep -qi NONE; then
		logger -t qmi-at "Do not find Network!"
		return 1
	fi
	
	logger -t qmi-at "Reg Network:$nettype"
	
	# to check CSQ & Creg & Cops & IMEI & IMSI .etc
	if echo "$cardinfo" | grep -qi IMEI; then
		local imei=$(echo "$cardinfo" | grep -i IMEI)
		[ ! -s "/tmp/devimeiid" ] && echo "${imei:14:6}" > /tmp/devimeiid
		echo "imei=${imei#IMEI:}" >> /tmp/3g-info
		local imsi=$(gcom -d "$at_port" -s /etc/gcom/getimsi.gcom)
		echo $imsi >> /tmp/3g-info
		local sig=$(gcom -d "$at_port" -s /etc/gcom/getstrength.gcom)
		echo "signal=$sig" >> /tmp/3g-info
		echo $sig > /tmp/sig
		echo "mac=$(cat /sys/class/net/eth0/address)" >> /tmp/3g-info
	fi
	
	echo "Default" > /tmp/pub_info
	cat /tmp/3g-info >> /tmp/pub_info
	#cat /tmp/module_status_file >> /tmp/pub_info
	
	if [ "$modem_type" = "EC20" ]; then
		gcom -d "$at_port" -s /etc/gcom/ec20agps.gcom > /tmp/agps_status
	fi
	
	return 0
}

proto_qmi_setup() {
	local interface="$1"
	local auth_int=0
	local pincfg
	local exe_cmd="/bin/qmidial"
	local device apn auth username password pincode delay modes pdptype profile dhcpv6 autoconnect plmn $PROTO_DEFAULT_OPTIONS
	local ip_6 ip_prefix_length gateway_6 dns1_6 dns2_6
	json_get_vars device apn auth username password pincode delay modes pdptype profile dhcpv6 autoconnect plmn $PROTO_DEFAULT_OPTIONS
	
	[ "$metric" = "" ] && metric="0"

	[ -n "$ctl_device" ] && device=$ctl_device
	
	[ -n "$device" ] || {
		echo "No control device specified"
		proto_notify_error "$interface" NO_DEVICE
		proto_set_available "$interface" 0
		return 1
	}

	device="$(readlink -f $device)"
	[ -c "$device" ] || {
		echo "The specified control device does not exist"
		proto_notify_error "$interface" NO_DEVICE
		proto_set_available "$interface" 0
		return 1
	}

	devname="$(basename "$device")"
	devpath="$(readlink -f /sys/class/usbmisc/$devname/device/)"
	ifname="$( ls "$devpath"/net )"
	[ -n "$ifname" ] || {
		echo "The interface could not be found."
		proto_notify_error "$interface" NO_IFACE
		proto_set_available "$interface" 0
		return 1
	}

	_get_info_status_by_at "/dev/ttyUSB2"
	[ $? != 0 ] && {
		echo "Check AT failed !"
		proto_notify_error "$interface" CALL_FAILED
		proto_set_available "$interface" 0
		return 1
	}
	
	[ -n "$delay" ] && sleep "$delay"
	
	#proto_notify_error "$interface" CALL_FAILED

	[ "$auth" = "none" ] && auth_int=0
	[ -n "$auth" -a "$auth" != "none" ] && auth_int=1
	
	logger -t qmi "Setting up $ifname with APN-$apn [$username-$password] pin-$pincode auth-$auth-$auth_int"
	
	[ -n "$apn" ] && {
		exe_cmd="$exe_cmd -s $apn"
		[ -n "$username" -a -n "$password" -a -n ${auth_int} ] && exe_cmd="$exe_cmd $username $password $auth_int"
		[ -n "$pincode" ] && exe_cmd="$exe_cmd -p $pincode"
	}
	exe_cmd="$exe_cmd &"
	/bin/pidof qmidial && killall -INT qmidial && sleep 1
	eval ${exe_cmd}
	echo "exec dial: $exe_cmd"
	sleep 1
	
	/bin/pidof qmidial && {
		json_init
		json_add_string name "${interface}_4"
		json_add_string ifname "@$interface"
		json_add_string proto "dhcp"
		proto_add_dynamic_defaults
		json_close_object
		ubus call network add_dynamic "$(json_dump)"
	}
	
	proto_init_update "$ifname" 1
	proto_send_update "$interface"
	
	/etc/init.d/net4g restart
	return 0
}

proto_qmi_teardown() {
	local interface="$1"
	logger -t qmi "Stopping network $interface"
	json_load "$(ubus call network.interface.$interface status)"
	json_select data
	
	killall -INT qmidial
	
	proto_init_update "*" 0
	proto_send_update "$interface"
}

[ -n "$INCLUDE_ONLY" ] || {
	add_protocol qmi
}
