#!/bin/bash

SECRET_FILE="/opt/energy-grid/secret.env"

SERVER_LOG="/var/log/energy-grid/server.log"
TRAFFIC_LOG="/var/log/energy-grid/traffic.log"

LOGGING=0

no_secret() {
    echo "[FATAL] $1" >&2
    echo "$(date) [FATAL] $1" >> "$SERVER_LOG"
    exit 1
}

# ensure secret exists
if [ ! -f "$SECRET_FILE" ]; then
    no_secret "Secret file missing: $SECRET_FILE"
fi

# add starting secret
# you should probably change this - use the grid user
echo -n "Password1!" > "$SECRET_FILE"

# start service
# add -x and restart service if you want byte-level logging in the traffic log (make sure not to fill your whole system storage)
if [ "$LOGGING" = "1" ]; then
    ./energy-grid "$SECRET_FILE" >> "$SERVER_LOG" 2>&1 &
    exec /usr/bin/socat -T 1 TCP4-LISTEN:502,fork,reuseaddr,max-children=10,so-keepalive TCP4:127.0.0.1:8080 2>> "$TRAFFIC_LOG"
else
    ./energy-grid "$SECRET_FILE" &
    exec /usr/bin/socat -T 1 TCP4-LISTEN:502,fork,reuseaddr,max-children=10,so-keepalive TCP4:127.0.0.1:8080
fi

