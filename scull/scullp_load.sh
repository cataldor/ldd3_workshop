#!/bin/sh
module="scullp"
device="scullp"
mode="664"

# remove stale dnodes
rm -f /dev/${device}[0-3]

major=$(awk "\$2==\"$module\" {print \$1}" /proc/devices)

mknod /dev/${device}0 c $major 5
mknod /dev/${device}1 c $major 6
mknod /dev/${device}2 c $major 7
mknod /dev/${device}3 c $major 8

# give appropriate group/permissions, and change the group.
# No all distribtuions have staff, some have "wheel" instead.
group="staff"
grep -q '^staff:' /etc/group || group="wheel"

chgrp $group /dev/${device}[0-3]
chmod $mode /dev/${device}[0-3]
