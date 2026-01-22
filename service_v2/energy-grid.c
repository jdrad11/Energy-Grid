// energy grid service rewritten in c
// built out of Tristan's basic chat server code

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdint.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

// definitions for socket
#define PORT 8080
#define BUFFER_SIZE 1024

// definitions for power generator
#define PRODUCTION_RATE 100
#define CYCLE_DURATION 5
#define BATTERY_MAX 500

// secrets for authentication
#define SECRET "Password1!" // yes i am hardcoding this for now don't @ me it will be changed later to be different per team
#define SECRET_LEN 10

// global variables
pthread_mutex_t lock;
int cycle_power;
int battery_power;

// stucture for receiving requests over socket protocol
// expects type 1 for power draw request
typedef struct __attribute__((packed)) {
	uint8_t req_type;
	uint32_t pwr_amount;
	uint8_t nonce[16];
	uint8_t hmac[32];
} PowerRequest;

// structure for sending responses over socket protocol
// mirrors request fields and sends 0 for failure, 1 for success, 2 for illegal or malformed request, 3 for authentication failure
typedef struct __attribute__((packed)) {
	uint8_t req_type;
	uint32_t pwr_amount;
	uint8_t status_code;
	uint8_t nonce[16];
	uint8_t hmac[32];
} PowerResponse;

// helper for printing hex bytes for testing auth
void print_hex(const char *data_label, uint8_t *data, size_t len) {
	printf("%s: ", data_label);
	for (size_t i = 0; i < len; i++) {
		printf("%02x", data[i]);
	}
	printf("\n");
}

// generate a nonce for auth
void generate_nonce(uint8_t *nonce) {
	if (RAND_bytes(nonce, 16) != 1) {
		printf("Error generating nonce\n");
		*nonce = 0;
	}
}

/**
 * calculate HMAC for auth
 * HMAC calculation process:
 * 1) fill req_type, pwr_amount, and status_code (if applicable)
 * 2) fill nonce with generate_nonce
 * 3) fill hmac with null bytes
 * 4) fill hmac with calculate_hmac on the PowerRequest/PowerResponse structure
**/
void calculate_hmac(const uint8_t *request, size_t request_len, uint8_t *hmac) {
	unsigned int len = 32;
	HMAC(EVP_sha256(), SECRET, SECRET_LEN, request, request_len, hmac, &len);
}

// power generator function (ran as a thread)
void* power_generator(void* arg) {
	printf("[GENERATOR] Energy grid generation service started. Producing %d power every %d seconds.\n", PRODUCTION_RATE, CYCLE_DURATION);

	// this cycle will run continuously to act as the power generator
	while (true) {
		// use the lock to prevent race conditions
		pthread_mutex_lock(&lock);
		cycle_power += 100;
		printf("[GENERATOR] Generated %d power.\n", PRODUCTION_RATE);
		pthread_mutex_unlock(&lock);

		// services will request power during this period
		sleep(CYCLE_DURATION);

		// enforce the maximum battery capacity
		pthread_mutex_lock(&lock);
		if (battery_power + cycle_power < BATTERY_MAX) {
			battery_power += cycle_power;
		}
		else {
			battery_power = BATTERY_MAX;
		}
		pthread_mutex_unlock(&lock);
		printf("[GENERATOR] Used %d power this cycle. Backup battery now storing %d power.\n", PRODUCTION_RATE-cycle_power, battery_power);
		cycle_power = 0;
	}
}

// attempts to draw <amount> power from the grid and returns a status code (1 for success, 0 for failure)
uint8_t draw_power(uint32_t amount) {
	// use the lock to prevent race conditions
	pthread_mutex_lock(&lock);
	
	// enough power available from just this cycle's production
	if (amount <= cycle_power) {
		cycle_power -= amount;
		printf("[SERVICE] Drew %d power from current cycle. Remaining in cycle: %d power.\n", amount, cycle_power);
		pthread_mutex_unlock(&lock);
		return 1;
	}

	// not fully enough power from just this cycle's production, use some battery power
	else if (amount <= cycle_power + battery_power) {
		battery_power -= amount - cycle_power;
		cycle_power = 0;
		printf("[SERVICE] Falling back to backup battery power. Current cycle exhausted. Remaining in battery: %d power.\n", battery_power);
		pthread_mutex_unlock(&lock);
		return 1;
	}

	// not enough power available from this cycle and backup battery, fail the request
	else {
		printf("[SERVICE] Failed to draw power. Insufficient power available.\n");
		pthread_mutex_unlock(&lock);
		return 0;
	}
}

// handle requests to the socket
void handle_power_request(int client_socket) {

	// prevent DOS on single-threaded socket
	struct timeval tv;
	tv.tv_sec = 3;
	tv.tv_usec = 0;

	if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
		close(client_socket);
		return;
	}

	PowerRequest req;
	PowerResponse resp;
	int bytes_read;

	// collect bytes to protocol spec (currently vulnerable to DOS by hanging because of MSG_WAITALL flag)
	bytes_read = recv(client_socket, &req, sizeof(PowerRequest), 0);

	// ensure correct number of request bytes
	if (bytes_read != sizeof(PowerRequest)) {
		// send an invalid request response with zeroed user-provided fields
		resp.req_type = 0;
		resp.pwr_amount = 0;
		resp.status_code = 2;
		generate_nonce(resp.nonce);
		memset(resp.hmac, 0, 32);
		calculate_hmac((uint8_t*)&resp, sizeof(PowerResponse) - 32, resp.hmac);
		send(client_socket, &resp, sizeof(PowerResponse), 0);
		close(client_socket);
		return;
	}

	// save the received HMAC and calculate the expected HMAC
	uint8_t received_hmac[32];
	memcpy(received_hmac, req.hmac, 32);
	memset(req.hmac, 0, 32);

	uint8_t calculated_hmac[32];
	calculate_hmac((uint8_t*)&req, sizeof(PowerRequest) - 32, calculated_hmac);

	/**
	 * debugging stuff
	 * print_hex("received_hmac", received_hmac, 32);
	 * print_hex("calculated_hmac", calculated_hmac, 32);
	**/

	// mirror user-provided fields
	resp.req_type = req.req_type;
	resp.pwr_amount = req.pwr_amount;

	// use secure memcmp to check HMAC authentication
	if (CRYPTO_memcmp(received_hmac, calculated_hmac, 32) != 0) {
		// authentication failure, received and calculated HMAC did not match
		resp.status_code = 3;

		generate_nonce(resp.nonce);
		memset(resp.hmac, 0, 32);
		calculate_hmac((uint8_t*)&resp, sizeof(PowerResponse) - 32, resp.hmac);

		send(client_socket, &resp, sizeof(PowerResponse), 0);
		close(client_socket);
		return;
	}

	// authentication was successful, received and calculated HMAC matched
	// translate bytes from network order
	uint32_t req_amount = ntohl(req.pwr_amount);

	// handle type 1 - power draw request
	if (req.req_type == 1) {
		// prevent negative requests and integer overflows
		if (req_amount <= 0 || req_amount > PRODUCTION_RATE + BATTERY_MAX) {
			resp.status_code = 2;
		} else {
			// attempt the power draw
			resp.status_code = draw_power(req_amount);
		}

	} else {
		// unimplemented request type was used
		resp.status_code = 2;
	}

	// send signed server reponse to authenticated request
	generate_nonce(resp.nonce);
	memset(resp.hmac, 0, 32);
	calculate_hmac((uint8_t*)&resp, sizeof(PowerResponse) - 32, resp.hmac);

	send(client_socket, &resp, sizeof(PowerResponse), 0);

	close(client_socket);
}

int main() {
	setvbuf(stdout, NULL, _IOLBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);
	int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Create the server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up the server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(PORT);

    // Bind the socket to the address
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Start listening for client connections
    if (listen(server_socket, 10) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server started on port %d. Waiting for clients to connect...\n", PORT);

	// initialize variables for power tracking
	cycle_power = 0;
	battery_power = 0;
	
	// initialize lock for preventing race conditions
	pthread_mutex_init(&lock, NULL);

	// create and start the power generator thread
	pthread_t generator;
	pthread_create(&generator, NULL, power_generator, NULL);

	// keep the service listening for connections infinitely
    while (1) {
		// Accept a single client connection (single-threaded)
		client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
		if (client_socket < 0) {
	    	perror("Client accept failed");
	    	close(server_socket);
	    	exit(EXIT_FAILURE);
		}

		printf("Client connected!\n");

		// Handle communication with the connected client
		handle_power_request(client_socket);
    }

    // Close the server socket
    close(server_socket);

    return 0;
}
