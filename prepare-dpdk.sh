#!/usr/bin/expect -f

# Drop all the system caches so that we can reserve huge pages
exec echo 3 | sudo tee /proc/sys/vm/drop_caches

# Setup the dpdk
spawn "tools/dpdk-setup.sh"

# Compile the kernel module
expect "Option:"
send "15\r"
expect "Press enter to continue ..."
send "\r"

# Reset everything
expect "Option:"
send "31\r"
expect "Press enter to continue ..."
send "\r"

# Reset everything
expect "Option:"
send "34\r"
expect "Press enter to continue ..."
send "\r"

# Setup the kernel module
expect "Option:"
send "18\r"
expect "Press enter to continue ..."
send "\r"

# Setup the NIC
expect "Option:"
send "24\r"
expect "Enter PCI address of device to bind to IGB UIO driver:"
send "0000:04:00.0\r"
expect "Press enter to continue ..."
send "\r"

# Setup the NIC
expect "Option:"
send "24\r"
expect "Enter PCI address of device to bind to IGB UIO driver:"
send "0000:04:00.1\r"
expect "Press enter to continue ..."
send "\r"

# Setup the huge pages
expect "Option:"
send "22\r"
expect "Number of pages for node0:"
send "2048\r"
expect "Press enter to continue ..."
send "\r"


expect "Option:"
send "35\r"