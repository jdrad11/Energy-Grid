#!/bin/bash

GENERATOR_LOG="/var/log/energy-grid/generator.log"
REQUESTS_LOG="/var/log/energy-grid/requests.log"

./energy-grid >> "$GENERATOR_LOG" 2>&1 &

exec /usr/bin/socat -x TCP4-LISTEN:5020,fork,reuseaddr TCP4:127.0.0.1:8080 2>> "$REQUESTS_LOG"
