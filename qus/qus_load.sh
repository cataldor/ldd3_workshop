#!/bin/sh

ARG1=${1:-qus.ko}

# hardcoded values -- usb info can be fetched from dmesg + sysfs
echo "2-1:1.0" > /sys/bus/usb/drivers/usb-storage/unbind
insmod "$ARG1"
