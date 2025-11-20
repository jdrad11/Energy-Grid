// energy grid service rewritten in c
// built out of Tristan's basic chat server code

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024

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

int main() {
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
	int cycle_power = 0;
	int battery_power = 0;
	
	// initialize lock for preventing race conditions
	pthread_mutex_t lock;
	pthread_mutex_init(&lock, NULL);

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
		handle_client(client_socket);
    }

    // Close the server socket
    close(server_socket);

    return 0;
}
