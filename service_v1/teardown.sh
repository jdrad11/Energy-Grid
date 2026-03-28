#!/bin/bash
# teardown script for energy grid systemd service (run as root)
# # assumes files are in the current directory (run from repo directory)

echo "trying to stop energy-grid.service..."
systemctl stop energy-grid.service

echo "removing service and logging files..."
rm -rf /opt/energy-grid/
rm /etc/systemd/system/energy-grid.service
rm -rf /var/log/energy-grid/

echo "removing energy grid service user..."
userdel grid

echo "done!"
