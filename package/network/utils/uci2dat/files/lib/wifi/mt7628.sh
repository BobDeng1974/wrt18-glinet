#!/bin/sh
append DRIVERS "mt7628"

. /lib/wifi/ralink_common.sh

prepare_mt7628() {
	prepare_ralink_wifi mt7628
}

scan_mt7628() {
	scan_ralink_wifi mt7628 mt7628
}


disable_mt7628() {
	disable_ralink_wifi mt7628
}

enable_mt7628() {
	enable_ralink_wifi mt7628 mt7628
}

detect_mt7628() {
#	detect_ralink_wifi mt7628 mt7628
	echo "detect_mt7628($1,$2,$3,$4)" >>/tmp/wifi.log
	ssid=ZYRH_APV4-`ifconfig eth0 | grep HWaddr | cut -c 51- | sed 's/://g'`
	[ -d /sys/module/mt7628 ] || return
	[ -s /etc/config/wireless ] && return
	cat >> /etc/config/wireless <<EOF
config wifi-device 'mt7628'
	option type 'mt7628'
	option htmode 'HT40'
	option txpower '20'
	option hwmode '11ng'
	option band '2G'
	option country 'CN'
	option region '1'
	option noscan '1'
	option channel '1'

config wifi-iface
	option device 'mt7628'
	option network 'lan'
	option mode 'ap'
	option key 'zyrh3gvs.net'
	option ssid ${ssid}
	option encryption 'psk2'
	option ifname 'ra0'
	option disabled '0'

config wifi-iface 'sta'
	option device 'mt7628'
	option mode 'sta'
	option network 'wwan'
	option ifname 'apcli0'
	option encryption 'psk2'
	option ssid 'upwifi'
	option key 'upwifi-pwd'
	option disabled '0'

EOF

}
