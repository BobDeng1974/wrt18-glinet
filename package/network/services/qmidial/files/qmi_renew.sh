#!/bin/sh
enb="$(uci -q get qmicfg.qmicfg.enable)"
/etc/init.d/qmidial stop
sleep 1
[ ${enb} -gt 0 ] && /etc/init.d/qmidial start
