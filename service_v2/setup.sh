#!/bin/bash
# setup script for energy grid systemd service (run with sudo)
# assumes files are in the current directory (run from repo directory)

if [ "$EUID" -ne 0 ]; then
	echo "Cannot setup service without root privileges"
	exit 1
fi

if ! command -v socat &> /dev/null; then
	echo "ncat dependency is not installed, please install before deploying service"
	exit 1
fi

# create grid service user (restrict permissions later or something idk)
id -u grid &>/dev/null || useradd -r -s /bin/false -M grid
echo "grid service user added..."

# create service folder and files
mkdir -p /opt/energy-grid
gcc -o "energy-grid" energy-grid.c
cp energy-grid /opt/energy-grid/
cp run-grid.sh /opt/energy-grid/
chmod +x /opt/energy-grid/run-grid.sh
cp energy-grid.service /etc/systemd/system/
chown -R grid:grid /opt/energy-grid
echo "service files copied..."

# set up logging
mkdir /var/log/energy-grid
touch /var/log/energy-grid/generator.log
touch /var/log/energy-grid/requests.log
chown -R grid:grid /var/log/energy-grid
echo "service logging configured..."

echo "trying to start energy-grid.service..."
systemctl daemon-reload
systemctl start energy-grid.service

if [[ $? -eq 0 ]]; then
	echo "Energy grid service successfully deployed and started"
else
	echo "Failed to start service. Please check that you have the necessary files."
fi

echo "done!"
