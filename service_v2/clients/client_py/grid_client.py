import socket
import struct
import hmac
import hashlib
import os

MAX_SECRET_LEN = 64

# define formats for requests and reponses
REQ_FMT = "!B I 16s 32s"
RESP_FMT = "!B I B 16s 32s"

REQ_SIZE = struct.calcsize(REQ_FMT)
RESP_SIZE = struct.calcsize(RESP_FMT)

# load the shared secret from a file
def _load_secret(path):
    with open(path, "rb") as f:
        secret = f.read(MAX_SECRET_LEN)

    if not secret:
        raise RuntimeError("Empty secret")

    if secret.endswith(b"\n"):
        secret = secret[:-1]

    return secret

# receive the full responses from the server
def _recv_exact(sock, size):
    buf = b""
    while len(buf) < size:
        chunk = sock.recv(size - len(buf))
        if not chunk:
            raise RuntimeError("Connection closed")
        buf += chunk
    return buf

# calculate the hmac for authenticated requests and responses
def _calculate_hmac(secret, data):
    return hmac.new(secret, data, hashlib.sha256).digest()

# make a power request to the server
def request_power(ip, port, secret_file, amount):
    """
    Returns status code:
      1 = success
      0 = insufficient power
      2 = malformed
      3 = auth failure

    Raises RuntimeError on client/protocol failure.
    """

    secret = _load_secret(secret_file)

    # set up the socket with a timeout
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(10)
    sock.connect((ip, port))

    req_type = 1
    nonce = os.urandom(16)

    # create the request structure for hmac calculation
    packed = struct.pack(
        REQ_FMT,
        req_type,
        amount,
        nonce,
        b"\x00" * 32
    )

    # caculate the hmac for auth
    mac = _calculate_hmac(secret, packed[:-32])

    # create the final request structure
    packed = struct.pack(
        REQ_FMT,
        req_type,
        amount,
        nonce,
        mac
    )

    sock.sendall(packed)

    # receive and unpack the response
    resp_raw = _recv_exact(sock, RESP_SIZE)
    sock.close()

    r_type, pwr_amount, status, rnonce, rhmac = struct.unpack(RESP_FMT, resp_raw)

    # authenticate the server response
    zeroed = struct.pack(
        RESP_FMT,
        r_type,
        pwr_amount,
        status,
        rnonce,
        b"\x00" * 32
    )

    expected = _calculate_hmac(secret, zeroed[:-32])

    # fraudulent responses received
    if not hmac.compare_digest(rhmac, expected):
        # print("Bad response HMAC")
        status = 3

    # zero the secret and return status code received from the server
    secret = b'\x00' * len(secret)
    return status

