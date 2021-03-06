#!/bin/bash
if ( echo -e "test123\nexit"  | qemu-system-arm -nographic -M lm3s6965evb --kernel ex1) | grep 321tset; then
	echo -e "\e[1;32mSerial passed.\e[0m"
fi

modprobe tun
tunctl -t tap0
ifconfig tap0 172.32.0.1 netmask 255.255.255.0
dnsmasq -d --interface=tap0 --dhcp-range=172.32.0.2,172.32.0.2 &
DPID=$?
qemu-system-arm -nographic -M lm3s6965evb --kernel ex2 -net nic -net tap,ifname=tap0,script=no < /dev/null &
QPID=$?

if ((echo -e "test123"; sleep 10; echo "exit") | nc 172.32.0.2 9000) | grep 321tset; then
	echo -e "\e[1;32mNetworking passed.\e[0m"
fi
kill -KILL $DPID $QPID
stty echo
