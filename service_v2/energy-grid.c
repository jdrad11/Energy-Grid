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
#include <sys/time.h>
#include <signal.h>

// definitions for socket
#define PORT 8080
#define BUFFER_SIZE 1024

// definitions for power generator
#define PRODUCTION_RATE 100
#define CYCLE_DURATION 120
#define BATTERY_MAX 500

// secrets for authentication
#define MAX_SECRET_LEN 64
uint8_t SECRET[MAX_SECRET_LEN];
size_t SECRET_LEN = 0;
const char *SECRET_FILEPATH;

// values for replay protection
#define NONCE_HISTORY_SIZE 4096
uint8_t nonce_history[NONCE_HISTORY_SIZE][16];
int nonce_history_head = 0;
pthread_mutex_t nonce_lock;

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
		memset(nonce, 1, 16);
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

// load the secret for authentication
// returns 1 on successful load, 0 on failure
int load_secret(const char *filepath) {
	memset(SECRET, 0, MAX_SECRET_LEN);

	FILE *f = fopen(filepath, "rb");
	if (!f) {
		fprintf(stderr, "Failed to open secret file at %s\n", filepath);
		return 0;
	}

	SECRET_LEN = fread(SECRET, 1, MAX_SECRET_LEN, f);
	fclose(f);

	if (SECRET_LEN == 0) {
		fprintf(stderr, "No secret found in file %s\n", filepath);
		return 0;
	}

	if (SECRET[SECRET_LEN - 1] == '\n') {
		SECRET_LEN--;
	}

	return 1;
}

// power generator function (ran as a thread)
void* power_generator(void* arg) {
	printf("[GENERATOR] Energy grid generation service started. Producing %d power every %d seconds.\n", PRODUCTION_RATE, CYCLE_DURATION);

	// this cycle will run continuously to act as the power generator
	while (1) {
		// use the lock to prevent race conditions
		pthread_mutex_lock(&lock);
		cycle_power += PRODUCTION_RATE;
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
		printf("[GENERATOR] Used %d power this cycle. Backup battery now storing %d power.\n", PRODUCTION_RATE-cycle_power, battery_power);
		cycle_power = 0;
		pthread_mutex_unlock(&lock);
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

	// prevent DOS on socket
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	// prevent hanging on read
	if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
		close(client_socket);
		return;
	}

	// prevent hanging on send
	if (setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
		close(client_socket);
		return;
	}

	PowerRequest req = {0};
	PowerResponse resp = {0};

	// collect bytes to protocol spec and reject hanging requests
	size_t got = 0;
	while (got < sizeof(PowerRequest)) {
    	ssize_t r = recv(client_socket, ((char*)&req) + got, sizeof(PowerRequest) - got, 0);
    	if (r <= 0) {
        	close(client_socket);
        	return;
    	}
    	got += r;
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

		size_t sent = 0;
		while (sent < sizeof(PowerResponse)) {
    		ssize_t s = send(client_socket, ((char*)&resp) + sent, sizeof(PowerResponse) - sent, 0);
    		if (s <= 0) {
        		close(client_socket);
        		return;
    		}
    		sent += s;
		}
		close(client_socket);
		return;
	}

	// store the request nonce

	pthread_mutex_lock(&nonce_lock);

	// check if nonce has been seen before
	int is_replay = 0;
	for (int i = 0; i < NONCE_HISTORY_SIZE; i++) {
		if (memcmp(nonce_history[i], req.nonce, 16) == 0) {
			is_replay = 1;
			break;
		}
	}

	// replay attack detected
	if (is_replay) {
		print_hex("Replay detected with nonce ", req.nonce, 16);

		resp.status_code = 3;
		generate_nonce(resp.nonce);
		memset(resp.hmac, 0, 32);
		calculate_hmac((uint8_t*)&resp, sizeof(PowerResponse) - 32, resp.hmac);
		size_t sent = 0;
		while (sent < sizeof(PowerResponse)) {
    		ssize_t s = send(client_socket, ((char*)&resp) + sent, sizeof(PowerResponse) - sent, 0);
    		if (s <= 0) {
        		close(client_socket);
        		return;
    		}
    		sent += s;
		}
		pthread_mutex_unlock(&nonce_lock);
		close(client_socket);
		return;
	}

	// store the new nonce in the history via a rotating buffer
	memcpy(nonce_history[nonce_history_head], req.nonce, 16);
	nonce_history_head = (nonce_history_head + 1) % NONCE_HISTORY_SIZE;

	pthread_mutex_unlock(&nonce_lock);

	// authentication was successful, received and calculated HMAC matched
	// translate bytes from network order
	uint32_t req_amount = ntohl(req.pwr_amount);

	// handle type 1 - power draw request
	if (req.req_type == 1) {
		// prevent negative requests and integer overflows
		if (req_amount == 0 || req_amount > PRODUCTION_RATE + BATTERY_MAX) {
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

	size_t sent = 0;
	while (sent < sizeof(PowerResponse)) {
    	ssize_t s = send(client_socket, ((char*)&resp) + sent, sizeof(PowerResponse) - sent, 0);
    	if (s <= 0) {
        	close(client_socket);
        	return;
    	}
    	sent += s;
	}

	close(client_socket);
}

// used for creating a socket thread
void* request_handler(void* socket) {
	int client_socket = *(int*)socket;

	free(socket);

	handle_power_request(client_socket);

	return NULL;
}

int main(int argc, char *argv[]) {
	// stop service from crashing on a failed send
	signal(SIGPIPE, SIG_IGN);

	// load the secret at startup
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <secret_file>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	// load the secret at startup (requires restart if secret is changed)
	SECRET_FILEPATH = argv[1];
	if (!load_secret(SECRET_FILEPATH)) {
		fprintf(stderr, "Failed to load secret from filepath %s\n", SECRET_FILEPATH);
		exit(1);
	}

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

	// initialize lock for nonce history
	pthread_mutex_init(&nonce_lock, NULL);

	// keep the service listening for connections infinitely
    while (1) {
		// Accept a client connection
		client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
		if (client_socket < 0) {
	    	perror("Client accept failed\n");
			continue;
		}

		printf("Client connected!\n");

		// allocate memory for passing the socket to the thread
		int *new_socket = malloc(sizeof(int));
		if (new_socket == NULL) {
			perror("Failed to allocate memory for new socket\n");
			close(client_socket);
			continue;
		}

		*new_socket = client_socket;

		pthread_t client_thread;

		// create a new thread for the client connection
		if (pthread_create(&client_thread, NULL, request_handler, (void*)new_socket) < 0) {
			perror("Could not create thread");
			free(new_socket);
			close(client_socket);
			continue;
		}

		// client thread will release resources when finished
		pthread_detach(client_thread);
    }

    // Close the server socket
    close(server_socket);

    return 0;
}
