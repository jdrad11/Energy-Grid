#!/bin/bash
# teardown script for energy grid systemd service (run as root)
# # assumes files are in the current directory (run from repo directory)

if [ "$EUID" -ne 0 ]; then
	echo "Cannot teardown service without root privileges"
	exit 1
fi

echo "trying to stop energy-grid.service..."
systemctl stop energy-grid.service

if [[ $? -ne 0 ]]; then
	echo "Failed to stop service, exiting..."
	exit 1
fi

echo "removing service and logging files..."
rm -rf /opt/energy-grid/
rm /etc/systemd/system/energy-grid.service
rm -rf /var/log/energy-grid/

systemctl daemon-reload
systemctl reset-failed

echo "removing energy grid service user..."
userdel grid || true

echo "done!"
