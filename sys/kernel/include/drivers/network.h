#pragma once

#include <stdint.h>

void network_init(void);
void network_poll(void);
int network_available(void);