/* i2s_examples.c
 *
 * Author: Daniel Collins
 * Date:   2022-02-25
 *
 * Copyright (c) 2022 Daniel Collins
 *
 * This file is part of rp2040_i2s_example.
 *
 * rp2040_i2s_example is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3 as published by the
 * Free Software Foundation.
 *
 * rp2040_i2s_example is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * rp2040_i2s_example. If not, see <https://www.gnu.org/licenses/>.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "i2s.h"
#include "pico/stdlib.h"
#include "tusb.h"


// I2C defines
// This example uses I2C0 on GPIO4 (SDA) and GPIO5 (SCL) running at 100KHz.
// Connect the codec I2C control to this. (Codec-specific customization is
// not part of this example.)
#define I2C_PORT i2c0
#define I2C_SDA  4
#define I2C_SCL  5

#ifndef PICO_DEFAULT_LED_PIN
#warning blink example requires a board with a regular LED
#else
const uint LED_PIN = PICO_DEFAULT_LED_PIN;
#endif

static __attribute__((aligned(8))) pio_i2s i2s;

static void process_audio(const int32_t* input, int32_t* output, size_t num_frames) {
    // Just copy the input to the output
    for (size_t i = 0; i < num_frames * 2; i++) {
        output[i] = (input[i] >> 16) << 16;
    }   
}

static void dma_i2s_in_handler(void) {
    /* We're double buffering using chained TCBs. By checking which buffer the
     * DMA is currently reading from, we can identify which buffer it has just
     * finished reading (the completion of which has triggered this interrupt).
     */
    if (*(int32_t**)dma_hw->ch[i2s.dma_ch_in_ctrl].read_addr == i2s.input_buffer) {
        // It is inputting to the second buffer so we can overwrite the first
        process_audio(i2s.input_buffer, i2s.output_buffer, AUDIO_BUFFER_FRAMES);
    } else {
        // It is currently inputting the first buffer, so we write to the second
        process_audio(&i2s.input_buffer[STEREO_BUFFER_SIZE], &i2s.output_buffer[STEREO_BUFFER_SIZE], AUDIO_BUFFER_FRAMES);
    }
    dma_hw->ints0 = 1u << i2s.dma_ch_in_data;  // clear the IRQ
}

int main() {
    // Set a 132.000 MHz system clock to more evenly divide the audio frequencies
    set_sys_clock_khz(132000, true);
    tusb_init();
    stdio_init_all();

    printf("System Clock: %lu\n", clock_get_hz(clk_sys));

    // Here, do whatever you need to set up your codec for proper operation.
    // Some codecs require register configuration over I2C, for example.

    i2s_config my_config;
    my_config.fs = 48000;
    my_config.sck_mult = 256;
    my_config.bit_depth = 16;
    my_config.sck_pin = 21;
    my_config.dout_pin = 18;
    my_config.din_pin = 22;
    my_config.clock_pin_base = 19;
    my_config.sck_enable = true;

    i2s_program_start_synched(pio0, &my_config, dma_i2s_in_handler, &i2s);
    // Enable the (already configured) codec here.

    puts("i2s_example started.");

    while(1) {
        tud_task(); // tinyusb device task
    }
    return 0;
}
