#ifndef __PROTOCOL_PARSE_H
#define __PROTOCOL_PARSE_H
#include <stdint.h>
void protocol_parse_ptu_cmd(uint8_t *ring, uint16_t ring_size, uint16_t *p_rd, uint16_t *p_wr);
#endif
