## Energy Grid Control Protocol (EGCP) Specification
Protocol Version: v2.0

Status: Active

### Overview
The energy grid infrastructure communicates with and accepts requests from clients according to this protocol specification.

The energy grid service sits behind a proxy, which forwards requests from clients and responses from the grid. The proxy also acts as a logger.

### Network Connections
Transport Layer ports:
* TCP 8080 (energy grid)
* TCP 502 (proxy)

The socket used for the energy grid service binds only locally on port 8080, requiring external requests to be sent through the proxy on port 502.

### Packet Structure
Energy Grid Control Protocol is a fixed-width binary protocol utilizing packed structures with no padding. It interprets requests according to the following specification.

#### **Requests**
53-byte requests can be sent to the energy grid. The `pwr_amount` field must be sent in network byte order.

**Fields and Options**
* **req_type (1 byte):**
    * `0x01` : power request
* **pwr_amount (4 bytes):**
    * `0x00000001` (1) - `0x00000258` (600) : expected quantity range
* **nonce (16 bytes):**
    * Randomized bytes to prevent replay attacks
* **hmac (32 bytes):**
    * HMAC-SHA256 signature calculated over the `req_type`, `pwr_amount`, and `nonce` fields using the shared secret

Below is the request layout:

| Byte Offset | Field | Size | Notes |
| :--- | :--- | :--- | :--- |
| `0x00` | `req_type` | 1 byte | Request Type Code |
| `0x01` - `0x04` | `pwr_amount` | 4 bytes | # of Power Units Requested |
| `0x05` - `0x14` | `nonce` | 16 bytes | Random bytes for replay protection |
| `0x15` - `0x34` | `hmac` | 32 bytes | HMAC-SHA256 signature |

#### **Responses**
The grid will respond to requests with a 54-byte response. 

**Fields and Options**

The response will mirror the `req_type` and `pwr_amount` bytes of the request to help operators ensure the request was received correctly and for logging purposes. 

* **req_type (1 byte):**
    * `0x01` : power request
* **pwr_amount (4 bytes):**
    * `0x00000001` (1) - `0x00000258` (600) : expected quantity range
* **status_code (1 byte):**
    * `0x00` : Failed Request - Not Enough Power Available
    * `0x01` : Successful Request
    * `0x02` : Malformed or Illegal Request (e.g., zero power, overflowing max bounds, or unknown type)
    * `0x03` : Authentication Failure (HMAC mismatch) or Replay Attack Detected
* **nonce (16 bytes):**
    * Newly generated randomized bytes from the server
* **hmac (32 bytes):**
    * HMAC-SHA256 signature calculated over the preceding response fields to authenticate the server's reply

Below is the response layout:

| Byte Offset | Field | Size | Notes |
| :--- | :--- | :--- | :--- |
| `0x00` | `req_type` | 1 byte | Request Type Code |
| `0x01` - `0x04` | `pwr_amount` | 4 bytes | # of Power Units Requested |
| `0x05` | `status_code` | 1 byte | Response Code |
| `0x06` - `0x15` | `nonce` | 16 bytes | Server-generated random bytes |
| `0x16` - `0x35` | `hmac` | 32 bytes | HMAC-SHA256 signature |

### Considerations
1) **Concurrency:** The TCP socket that handles requests and responses utilizes multithreading via `pthread` to spawn a dedicated handler for every client connection. The server can handle multiple concurrent connections without blocking.
2) **Authentication & Security:** Requests mandate HMAC-SHA256 authentication utilizing a shared secret file loaded at service startup. The service enforces replay protection by tracking a rotating buffer of the last 4096 nonces. 
3) **Routing:** Port mappings should be kept static to ensure clients know where to send requests.
