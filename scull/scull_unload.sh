#!/bin/sh
module="scull"
device="scull"

# invoke rmmod with all arguments we got
/sbin/rmmod $module $* || exit 1

# remove slate nodes

rm -f /dev/${device}
rm -f /dev/${device}[0-3]

