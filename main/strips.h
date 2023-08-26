#include "led_strip_encoder.h"


#ifndef STRIPS_H
#define STRIPS_H

void leds_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b);

/** Init the leds and the RMT behind the encoder. Also init the task to update the leds when the datastructure is modified.
 **/
void leds_init();

void leds_setled(uint32_t led_idx, uint8_t r, uint8_t g, uint8_t b);

/** Run the routine to transfer the leds information to the actual ledstrips.
 **/
void leds_show();


#endif