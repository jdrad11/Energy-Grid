# test the python client

from grid_client import request_power

status = request_power("127.0.0.1", 502, "secret.txt", 50)

print(status)
