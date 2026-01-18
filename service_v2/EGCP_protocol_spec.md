## Energy Grid Control Protocol (EGCP) Specification
Protocol Version: v1.0\
Status: Active

### Overview
The energy grid infrastructure communicates with and accepts requests from clients according to this protocol specification.

The energy grid service sits behind a proxy, which forwards requests from clients and responses from the grid. The proxy also acts as a logger.

### Network Connections
Transport Layer ports:
- TCP 8080 (energy grid)
- TCP 5020 (proxy)

The socket used for the energy grid service binds only locally on port 8080, requiring external requests to be sent through the proxy on port 5020.

### Packet Structure
Energy Grid Control Protocol is a fixed-width binary protocol. It interprets requests according to the following specification.

#### **Requests**
5-byte requests can be sent to the energy grid, in network byte order, to make a power request.

#### Fields and Options
req_type (1 byte):
- 0x01 : power request

pwr_amount (4 bytes)
- 0x00000001 (1) - 0x00000258 (600) : expected quantity range

Below is the request format that would be used to request 15 power units from the grid:

| Offset | Field | Value | Notes |
| ------ | ----- | ----- | ----- |
| 0x00 | req_type | 0x01 | Request Type Code |
| 0x01 | pwr_amount | 0x00 | # of Power Units Requested |
| 0x02 | pwr_amount | 0x00 | # of Power Units Requested |
| 0x03 | pwr_amount | 0x00 | # of Power Units Requested |
| 0x04 | pwr_amount | 0x0f | # of Power Units Requested |

#### **Responses**
The grid will respond to requests with a 6-byte response, sent in network byte order.

#### Fields and Options
The response will mirror the first 5 bytes of the request, to help operators ensure the request was received correctly and for logging purposes.

If too bytes are sent in the request, the first 5 bytes of the response will be set to null bytes. The 6th byte will contain the 0x02 status code.

req_type (1 byte):
- 0x01 : power request
- 0x00 : too few bytes in request

pwr_amount (4 bytes)
- 0x00000001 (1) - 0x00000258 (600) : expected quantity range
- 0x00000000 (0) : too few bytes in request

status_code (1 byte):
- 0x00 : Failed Request - Not Enough Power Available
- 0x01 : Successful Request
- 0x02 : Malformed or Illegal Request

Below is the response format that would be sent upon a successful request for 15 power units:

| Offset | Field | Value | Notes |
| ------ | ----- | ----- | ----- |
| 0x00 | req_type | 0x01 | Request Type Code |
| 0x01 | pwr_amount | 0x00 | # of Power Units Requested |
| 0x02 | pwr_amount | 0x00 | # of Power Units Requested |
| 0x03 | pwr_amount | 0x00 | # of Power Units Requested |
| 0x04 | pwr_amount | 0x0f | # of Power Units Requested |
| 0x05 | status_code | 0x01 | Reponse Code |

### Considerations
1) The TCP socket that handles requests and responses is currently single-threaded. Extremely high traffic loads may result in a Denial of Service.

2) Requests are currently unauthenticated. A host-based firewall or some other mechanism should be used to ensure that only legitimate clients and services can make power requests. Consider that the proxy must still be able to forward requests to the internal service on port 8080.

3) Port mappings should be kept static to ensure clients know where to send requests.

