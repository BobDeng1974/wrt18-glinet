#!/bin/sh

[ -n "$INCLUDE_ONLY" ] || {
	. ../netifd-proto.sh
	. ./ppp.sh
	init_proto "$@"
}

proto_3g_init_config() {
	no_device=1
	available=1
	ppp_generic_init_config
	# vars in /etc/config/network.ppp
	# json_load(jshn -r xx) to shell env
	proto_config_add_string "device:device"
	proto_config_add_string "apn"
	proto_config_add_string "service"
	proto_config_add_string "pincode"
	proto_config_add_string "dialnumber"
	proto_config_add_string "username"
	proto_config_add_string "password"
	proto_config_add_string "at_port"
	proto_config_add_string "private_dial"
	proto_config_add_string "u9300_gps"
	proto_config_add_string "ec20_atport"
	proto_config_add_string "ec20_device"
	proto_config_add_string "ec20_gps"
}

proto_3g_setup() {
	local interface="$1"
	local chatcfg
	local modem_type="NONE"
	
	json_get_var device device
	json_get_var apn apn
	json_get_var service service
	json_get_var pincode pincode
	json_get_var dialnumber dialnumber
	# add by wind
	#for u9300
	json_get_var username username
	json_get_var password password
	json_get_var at_port at_port
	json_get_var private_dial private_dial
	
	#for ec20
	json_get_var ec20_atport ec20_atport
	json_get_var ec20_device ec20_device
	# end

	[ -z "$at_port" ] && at_port="/dev/ttyUSB2"
	[ -z "$private_dial" ] && private_dial="0"
	
	[ -n "$dat_device" ] && device=$dat_device
	[ -e "$device" ] || {
		proto_set_available "$interface" 0
		return 1
	}
	nettype="NONE"
	# every time pppd to be called
	case "$service" in
		cdma|evdo)
			chatcfg="/etc/chatscripts/evdo.chat"
		;;
		*)
			rm -f /tmp/dialok
			date >> /tmp/dialcount
			cardinfo=$(gcom -d "$at_port" -s /etc/gcom/getcardinfo.gcom)
			if echo "$cardinfo" | grep -q Novatel; then
				case "$service" in
					umts_only) CODE=2;;
					gprs_only) CODE=1;;
					*) CODE=0;;
				esac
				export MODE="AT\$NWRAT=${CODE},2"
			elif echo "$cardinfo" | grep -q Option; then
				case "$service" in
					umts_only) CODE=1;;
					gprs_only) CODE=0;;
					*) CODE=3;;
				esac
				export MODE="AT_OPSYS=${CODE}"
			elif echo "$cardinfo" | grep -q "Sierra Wireless"; then
				SIERRA=1
			elif echo "$cardinfo" | grep -qi huawei; then
				case "$service" in
					umts_only) CODE="14,2";;
					gprs_only) CODE="13,1";;
					*) CODE="2,2";;
				esac
				export MODE="AT^SYSCFG=${CODE},3FFFFFFF,2,4"
			elif echo "$cardinfo" | grep -q LONGSUNG; then
				modem_type="LONGSUNG"
				case "$service" in
					ulong1) CODE=1;;
					ulong2) CODE=2;;
					ulong3) CODE=3;;
					ulong4) CODE=4;;
					ulong5) CODE=5;;
					ulong6) CODE=6;;
					ulong7) CODE=7;;
					ulong8) CODE=8;;
					ulong9) CODE=9;;
					ulong10) CODE=10;;
					ulong11) CODE=11;;
					ulong12) CODE=12;;
					ulong13) CODE=13;;
					ulong14) CODE=14;;
					ulong15) CODE=15;;
					*) CODE=2;;
				esac
				export MODE="AT+MODODREX=${CODE}"
			elif echo "$cardinfo" | grep -q EC20; then
				modem_type="EC20"
				device="/dev/ttyUSB3"
			else
				#unknow USB_Module
				logger -t ppp "unknow usb module:$cardinfo"
				sleep 1
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
				echo none > /sys/class/leds/dialstatus/trigger
				return 1
			fi
			
#			local netreg=$(gcom -d "$at_port" -s /etc/gcom/check_reg.gcom)
#			if echo "$netreg" | grep -qi '0,1'; then
#				logger -t ppp "Registered Home Network"
#			elif echo "$netreg" | grep -qi '0,5'; then
#				logger -t ppp "Registered Remote Network"
#			else
#				logger -t ppp "$netreg"
#				# check CSQ  SIMCARD PSRAR  Creg CEREG
#				gcom -d "$at_port" -s /etc/gcom/check_status.gcom > /tmp/at_failed_ret
#				echo "$netreg" >> /tmp/at_failed_ret
#				return 1
#			fi
			
			local sysinfo=$(gcom -d "$at_port" -s /etc/gcom/sysinfo.gcom)
			if echo "$sysinfo" | grep -qi 'NO,'; then
				logger -t ppp "!!! SYSINFO FAILED !!!"
				# check CSQ  SIMCARD PSRAR  Creg CEREG
				date >> /tmp/at_failed_ret
				gcom -d "$at_port" -s /etc/gcom/check_status.gcom >> /tmp/at_failed_ret
				return 1
			fi
			
			if [ "$modem_type" = "LONGSUNG" ]; then
				nettype=$(gcom -d "$at_port" -s /etc/gcom/nettype.gcom)
			else
				nettype=$(gcom -d "$at_port" -s /etc/gcom/ec20net.gcom)
			fi
			
			if echo "$nettype" |grep -qw EVDO; then
				username="ctnet@mycdma.cn"
				password="vnet.mobi"
				dialnumber="#777"
			elif echo "$nettype" |grep -qw CDMA; then
				username="ctnet@mycdma.cn"
				password="vnet.mobi"
				dialnumber="#777"
			elif echo "$nettype" |grep -qi NONE; then
				logger -t ppp "Do not find Network!"
				return 1
			else
				if [ "$private_dial" != "1" ]; then
					#ignore web vars，pubilc dial do not need user&pwd
					username=""
					password=""
				fi
				dialnumber="*99#"
			fi
			
			logger -t ppp "Reg Network:$nettype"
			
			chatcfg="/etc/chatscripts/3g.chat"
			connect="DIALNUMBER=$dialnumber /usr/sbin/chat -t5 -v -E -f $chatcfg"
			
			if [ "$private_dial" = "1" ]; then
				if [ "$modem_type" = "LONGSUNG" ]; then
					#only for 9300 private dial
					chatcfg="/etc/chatscripts/3gprivate.chat"
					#cgdcont ,3GNET CTNET $QCPDPP=1,1,"user","pwd"
					export CGDCONT="AT+CGDCONT=1,\"IP\",\"$apn\"";
					export USER_PWD="AT\$QCPDPP=1,1,\"$password\",\"$username\"";
					gcom -d "$at_port" -s /etc/gcom/initfirst.gcom
					
				elif [ "$modem_type" = "EC20" ]; then
					#only for ec20 private dial
					chatcfg="/etc/chatscripts/ec20private.chat"
					#QCFG="cdmaruim",1';QICSGP=1,1,"$USE_APN","$USER","$PWD"';
					export CGDCONT="AT+QCFG=\"cdmaruim\",1";
					export USER_PWD="AT+QICSGP=1,1,\"$apn\",\"$username\",\"$password\"";
					#gcom -d "$at_port" -s /etc/gcom/initfirst.gcom
					logger -t ppp "EC20 Private Dial......"
				fi
				
				connect="${apn:+USE_APN=$apn} DIALNUMBER=$dialnumber USER=$username PWD=$password /usr/sbin/chat -t5 -v -E -f $chatcfg"
			else
				# auto setting APN
				export USE_APN="$apn";
				gcom -d "$at_port" -s /etc/gcom/autoinitapn.gcom
			fi
			
			# set searching network order
 			[ -n "$MODE" ] && gcom -d "$at_port" -s /etc/gcom/setmode.gcom
			
			if [ "$modem_type" = "LONGSUNG" ]; then
				# close ehrpd and psdialind
				logger -t ppp "LONGSUNG, to close ehprd and psdialind"
				gcom -d "$at_port" -s /etc/gcom/ehrpdclose.gcom
				gcom -d "$at_port" -s /etc/gcom/psdialindclose.gcom
			fi
			
			# to check CSQ & Creg & Cops & IMEI & IMSI .etc
			if echo "$cardinfo" | grep -qi IMEI; then
				imei=$(echo "$cardinfo" | grep -i IMEI)
				[ ! -s "/tmp/devimeiid" ] && echo "${imei:14:6}" > /tmp/devimeiid
				echo "imei=${imei#IMEI:}" >> /tmp/3g-info
				imsi=$(gcom -d "$at_port" -s /etc/gcom/getimsi.gcom)
				echo $imsi >> /tmp/3g-info
				sig=$(gcom -d "$at_port" -s /etc/gcom/getstrength.gcom)
				echo "signal=$sig" >> /tmp/3g-info
				echo $sig > /tmp/sig
				echo "mac=$(cat /sys/class/net/eth0/address)" >> /tmp/3g-info

			fi
			
			echo "Default" > /tmp/pub_info
			cat /tmp/3g-info >> /tmp/pub_info
			#cat /tmp/module_status_file >> /tmp/pub_info
			
			echo "Default" > /tmp/dialcfg
			echo "4Gdial=$private_dial" >> /tmp/dialcfg
			echo "apn=$apn" >> /tmp/dialcfg
			echo "user=$username" >> /tmp/dialcfg
			echo "password=$password" >> /tmp/dialcfg
			echo "dialnumber=$dialnumber" >> /tmp/dialcfg
			
			if [ "$modem_type" = "LONGSUNG" ]; then
				gcom -d "$at_port" -s /etc/gcom/longsangagps.gcom > /tmp/agps_status
			elif [ "$modem_type" = "EC20" ]; then
				gcom -d "$at_port" -s /etc/gcom/ec20agps.gcom > /tmp/agps_status
			fi
		;;
	esac
	ppp_generic_setup "$interface" \
		noaccomp \
		${username:+user "$username" password "$password"} \
		nopcomp \
		novj \
		nobsdcomp \
		noauth \
		lock \
		crtscts \
		modem \
		115200 "$device"
	return 0
}

proto_3g_teardown() {
	proto_kill_command "$interface"
}

[ -n "$INCLUDE_ONLY" ] || {
	add_protocol 3g
}

