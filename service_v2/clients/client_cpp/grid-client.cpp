#include "grid-client.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>

#define MAX_SECRET_LEN 64

// define data structures for requests and responses
#pragma pack(push, 1)
struct PowerRequest {
    uint8_t req_type;
    uint32_t pwr_amount;
    uint8_t nonce[16];
    uint8_t hmac[32];
};

struct PowerResponse {
    uint8_t req_type;
    uint32_t pwr_amount;
    uint8_t status_code;
    uint8_t nonce[16];
    uint8_t hmac[32];
};
#pragma pack(pop)

// load the shared secret from a filepath
static int load_secret(const char *path, uint8_t *secret, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    *len = fread(secret, 1, MAX_SECRET_LEN, f);
    fclose(f);

    if (!*len) return -1;
    if (secret[*len - 1] == '\n') (*len)--;

    return 0;
}

// generate a nonce for hmac auth
static void generate_nonce(uint8_t *n) {
    RAND_bytes(n, 16);
}

// calculate hmac for auth
static void calculate_hmac(const uint8_t *buf, size_t len, const uint8_t *secret, size_t secret_len, uint8_t *out) {

    unsigned int l = 32;
    HMAC(EVP_sha256(), secret, secret_len, buf, len, out, &l);
}

// make a power request to the server and return thr response
int request_power(const char *server_ip, uint16_t port, const char *secret_file, uint32_t amount) {

	// load the secret in from the provided filepath
    uint8_t secret[MAX_SECRET_LEN];
    size_t secret_len;

    if (load_secret(secret_file, secret, &secret_len) != 0)
        return -1;

	// open the socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

	// add timout to the socket to prevent hanging
	struct timeval tv = {10, 0};
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

	// open the connection to the server
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

	// fill the power request data structure pre-auth
    PowerRequest req{};
    req.req_type = 1;
    req.pwr_amount = htonl(amount);
    generate_nonce(req.nonce);

	// calculate the request hmac for auth
    memset(req.hmac, 0, 32);
    calculate_hmac(reinterpret_cast<uint8_t*>(&req), sizeof(req) - 32, secret, secret_len, req.hmac);

	// send all bytes of the request (prevent partial sends)
    size_t sent = 0;
	while (sent < sizeof(req)) {
		ssize_t n = send(sock, ((char*)&req) + sent, sizeof(req) - sent, 0);
		if (n <= 0) return -1;
		sent += n;
	}

    PowerResponse resp{};

	// receive all bytes of the response (prevent partial receives)
    size_t got = 0;
	while (got < sizeof(resp)) {
		ssize_t n = recv(sock, ((char*)&resp) + got, sizeof(resp) - got, 0);
		if (n <= 0) return -1;
		got += n;
	}

/** This section checks the server response to prevent man in the middle. It has been commented out to allow this type of attack.

	// validate the server hmac auth
    uint8_t recv_hmac[32];
    memcpy(recv_hmac, resp.hmac, 32);
    memset(resp.hmac, 0, 32);

    uint8_t calc_hmac[32];
    calculate_hmac(reinterpret_cast<uint8_t*>(&resp), sizeof(resp) - 32, secret, secret_len, calc_hmac);

    close(sock);

    if (CRYPTO_memcmp(recv_hmac, calc_hmac, 32) != 0)
        return 3;

**/

	// remove this if man in the middle protected is reenabled later to prevent trying to close the socket twice
	close(sock);

	// remove the secret from memory and return the server response
	OPENSSL_cleanse(secret, secret_len);
    return resp.status_code;
}

