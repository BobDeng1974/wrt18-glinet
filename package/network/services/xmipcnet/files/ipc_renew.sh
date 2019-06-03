#!/bin/sh

EXEC=xmipcnet

enb="$(uci -q get ipc_cfg.cfg.enable)"
/etc/init.d/$(EXEC) stop
sleep 1
[ ${enb} -gt 0 ] && /etc/init.d/$(EXEC) start
