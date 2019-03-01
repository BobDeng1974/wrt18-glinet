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
