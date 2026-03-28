#pragma once
#include <stdint.h>

// returns:
//  1 = success
//  0 = insufficient power
//  2 = malformed / illegal request
//  3 = auth failure
// -1 = client-side error

int request_power(const char *server_ip, uint16_t port, const char *secret_file, uint32_t amount);

