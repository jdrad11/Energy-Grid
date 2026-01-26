To make a power request:
add '#include "grid-client.h"' to your service code file
request power using:
request_power(IP, port, secret_filepath, amount);
example:
request_power("127.0.0.1", 502, "/opt/energy-grid/secret.env", 15);
returns:
0 - failed: insufficient power
1 - succeeded
2 - failed: illegal/malformed request
3 - failed - authentication failure
