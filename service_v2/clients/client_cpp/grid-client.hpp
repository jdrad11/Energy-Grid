#pragma once
#include <cstdint>

int request_power(const char *server_ip, uint16_t port, const char *secret_file, uint32_t amount);

