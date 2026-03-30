## The Energy Grid
This is an implementation of a power grid simulation program. It operates via a Linux systemd service, which acts as the server. It communicates on the network using a custom developed binary network protocol, which has been designed as an example of a potential upgrade to existing industrial control system protocols. The service was deployed as a part of the environment in DakotaCon's Cyber Conquest 2026 purple team competition environment, an attack/defend cyber competition built from scratch by Dakota State University student volunteers.

Information about Cyber Conquest can be found here: https://cyberconquest.org


## How Do We Secure Industrial Control Systems?
A problem continuing to plague today's industrial control systems is the use of legacy protocols, in particular Modbus, that do not implement any form of authentication and are therefore vulnerable to a plethora of attacks if left improperly exposed. This project implements an industrial control systems protocol that seeks to mitigate this issue by utilizing basic HMAC-SHA256 authentication alongside nonce buffers. It is intentionally not fully secure due to its purpose of being a system used in a cyber competition environment where it was intended to be attacked. However the security features present in the protocol greatly mitigate typical attack methods on ICS protocols, such as injection and replay attacks.

## Features:
- Energy grid server, written in C
- Custom binary network protocol implemented with HMAC authentication and nonce buffers
- A technical specification outlining the grid protocol
- Client packages written in C/C++/Python for communicating with the server
- A proxy set up in front of the energy grid, which can be used for logging, etc.
- Shell script utilities for automatic secure installation and configuration of the program as a systemd service in Linux sandboxes
- A legacy "v1" version of the server written in Python, which was later replaced with the C version

The energy grid project was developed by myself and a couple friends over the course of six months alongside our schooling and other obligations, and this was used as a collaboration repository. We may have left behind some old code and notes that we used to collaborate and brainstorm. Feel free to take a look around!

## Try It!
To try out the service, clone this repo onto your Linux sandbox and run `sudo ./setup.sh` from the `service_v2` directory to deploy the grid. You will need a C installation with `OpenSSL` and `pthread` packages, as well as the `Socat` proxy package.

You can now control the energy grid service using the `energy-grid` systemd service. Try sending different requests to the grid using the client packages to see how it responds, and enable logging in `run-grid.sh` to see traffic at the byte level.

When you're done, simply run `sudo ./teardown.sh` from the `service_v2` directory to remove the energy grid service from your system.

