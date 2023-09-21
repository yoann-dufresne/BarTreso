#include <stdint.h>

#ifndef SOCKET_H
#define SOCKET_H

void sock_init();
void sock_send(uint8_t * msg, uint8_t size);

#endif