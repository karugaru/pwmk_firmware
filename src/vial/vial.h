#ifndef PWMK_VIAL_H
#define PWMK_VIAL_H

#include <stdint.h>

#define VIAL_PACKET_SIZE 32

void vial_handle_packet(uint8_t packet[VIAL_PACKET_SIZE]);

#endif // PWMK_VIAL_H
