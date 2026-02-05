#include <cstdio>
#include <cstdlib>
#include "grid-client.hpp"

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s <secret_file> <amount>\n", argv[0]);
        return 1;
    }

    int amount = atoi(argv[2]);

    int result = request_power("127.0.0.1", 502, argv[1], amount);

    printf("Result code: %d\n", result);

    return 0;
}

