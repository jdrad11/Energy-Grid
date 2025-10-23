# setup script for energy grid systemd service (run with sudo)
# assumes files are in the current directory (run from repo directory)

# create grid service user (restrict permissions later or something idk)
useradd -r -s /bin/false -M grid
echo "grid service user added..."

# create service folder and files
mkdir /opt/energy-grid
cp grid-api.py grid.py /opt/energy-grid/
cp energy-grid.service /etc/systemd/system/
chown -R grid:grid /opt/energy-grid
echo "service files copied..."

# set up logging
mkdir /var/log/energy-grid
touch /var/log/energy-grid/grid.log
chown -R grid:grid /var/log/energy-grid
echo "service logging configured..."

echo "done!"
