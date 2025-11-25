// energy grid service rewritten in c
// built out of Tristan's basic chat server code

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

// definitions for socket
#define PORT 8080
#define BUFFER_SIZE 1024

// definitions for power generator
#define PRODUCTION_RATE 100
#define CYCLE_DURATION 5
#define BATTERY_MAX 500

// global variables
pthread_mutex_t lock;
int cycle_power;
int battery_power;


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

// attempts to draw <amount> power from the grid and returns a status code
int draw_power(int amount) {
	// use the lock to prevent race conditions
	pthread_mutex_lock(&lock);

	// enough power available from just this cycle's production
	if (amount <= cycle_power) {
		cycle_power -= amount;
		printf("[SERVICE] Drew %d power from current cycle. Remaining in cycle: %d power.\n", amount, cycle_power);
		pthread_mutex_unlock(&lock);
		return '1';
	}

	// not fully enough power from just this cycle's production, use some battery power
	else if (amount <= cycle_power + battery_power) {
		battery_power -= amount - cycle_power;
		cycle_power = 0;
		printf("[SERVICE] Falling back to backup battery power. Current cycle exhausted. Remaining in battery: %d power.\n", battery_power);
		pthread_mutex_unlock(&lock);
		return '1';
	}

	// not enough power available from this cycle and backup battery, fail the request
	else {
		printf("[SERVICE] Failed to draw power. Insufficient power available.\n");
		pthread_mutex_unlock(&lock);
		return '0';
	}
}

/** original client handler function, commented out but left for now for reference
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1)) > 0) {
        buffer[bytes_read] = '\0';  // Null-terminate the message
        printf("Client: %s\n", buffer);  // Print the message received from the client

        // Echo the message back to the client
        write(client_socket, buffer, strlen(buffer));

        // If the client sends "exit", break the loop and close the connection
        if (strncmp(buffer, "exit", 4) == 0) {
            printf("Client disconnected.\n");
            break;
        }
    }

    close(client_socket);  // Close the client socket when done
}
**/

// handles requests for drawing power from the grid
void handle_power_request(int client_socket) {
	char buffer[BUFFER_SIZE];
	int bytes_read;

	// expects a power amount in string form, ex: "15"
	if ((bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1)) > 0) {
		buffer[bytes_read] = '\0';
		int requested_amount = atoi(buffer);
		printf("[SERVICE] Requested %d power.\n", requested_amount);

		// try to draw the requested amount of power
		char result = draw_power(requested_amount);

		// send back '1' for success or '0' for failure
		write(client_socket, &result, 1);
		close(client_socket);
	}
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
    server_addr.sin_addr.s_addr = INADDR_ANY;
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
    while (true) {
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
