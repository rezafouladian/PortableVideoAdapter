#include <stdlib.h>
#include <string.h>

#include "common_dvi_pin_configs.h"
#include "dvi.h"
#include "dvi_serialiser.h"
#include "tmds_encode.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/vreg.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#define PORTABLE 1
#define OUTBOUND 2
#define OUTBOUND_CUSTOM 0

/// Hat type detect pin 1
#define DETECT1 9
/// Hat type detect pin 2
#define DETECT2 16

const struct dvi_timing dvi_timing_640x400p_70hz = {
    .h_sync_polarity = false,
    .h_front_porch = 16,
    .h_sync_width = 96,
    .h_back_porch = 48,
    .h_active_pixels = 640,

    .v_sync_polarity = true,
    .v_front_porch = 12,
    .v_sync_width = 2,
    .v_back_porch = 35,
    .v_active_lines = 400,

    .bit_clk_khz = 252000
};

static const struct dvi_serialiser_cfg waveshare_rp2350_pizero = {
    .pio = DVI_DEFAULT_PIO_INST,
    .sm_tmds = {0, 1, 2},
    .pins_tmds = {36, 34, 32},
    .pins_clk = 38,
    .invert_diffpairs = false
};

#define FRAME_WIDTH 640
#define FRAME_HEIGHT 400
#define VREG_VSEL VREG_VOLTAGE_1_10
#define DVI_TIMING dvi_timing_640x400p_70hz

// RGB111 bitplaned framebuffer
#define PLANE_SIZE_BYTES (FRAME_WIDTH * FRAME_HEIGHT / 8)
uint8_t framebuf[3 * PLANE_SIZE_BYTES];
uint8_t framebuf_back[3 * PLANE_SIZE_BYTES]; // Create a second buffer
uint8_t *display_ptr = framebuf; // Pointer to the buffer currently being displayed
uint8_t frame_buffer[32000];

struct dvi_inst dvi0;

uint8_t VIDEO_FRM;
uint8_t VIDEO_CLK;
uint8_t VIDEO_HSYNC;
uint8_t VIDEO_D0;
uint8_t VIDEO_D1;
uint8_t VIDEO_D2;
uint8_t VIDEO_D3;
uint8_t VIDEO_D4;
uint8_t VIDEO_D5;
uint8_t VIDEO_D6;
uint8_t VIDEO_D7;
uint8_t BOARD_TYPE;

static uint32_t VIDEO_M0, VIDEO_M1, VIDEO_M2, VIDEO_M3, VIDEO_M4, VIDEO_M5, VIDEO_M6, VIDEO_M7;

static void video_build_masks(void) {
    VIDEO_M0 = 1u << VIDEO_D0;
    VIDEO_M1 = 1u << VIDEO_D1;
    VIDEO_M2 = 1u << VIDEO_D2;
    VIDEO_M3 = 1u << VIDEO_D3;
    VIDEO_M4 = 1u << VIDEO_D4;
    VIDEO_M5 = 1u << VIDEO_D5;
    VIDEO_M6 = 1u << VIDEO_D6;
    VIDEO_M7 = 1u << VIDEO_D7;
}

void init_setup() {
    gpio_init(DETECT1);
    gpio_init(DETECT2);
    gpio_pull_up(DETECT1);
    gpio_pull_up(DETECT2);
    gpio_set_dir(DETECT1, GPIO_IN);
    gpio_set_dir(DETECT2, GPIO_IN);
    busy_wait_us(1);
    BOARD_TYPE = !gpio_get(DETECT2) << 1 | !gpio_get(DETECT1);
    if (BOARD_TYPE == PORTABLE) {
        VIDEO_FRM = 19;
        VIDEO_CLK = 26;
        VIDEO_D0 = 22;
        VIDEO_D1 = 11;
        VIDEO_D2 = 12;
        VIDEO_D3 = 10;
        VIDEO_D4 = 0;
        VIDEO_D5 = 15;
        VIDEO_D6 = 6;
        VIDEO_D7 = 13;
    }
    if (BOARD_TYPE == OUTBOUND) {
        VIDEO_FRM = 19;
        VIDEO_CLK = 26;
        VIDEO_HSYNC = 20;
        VIDEO_D0 = 11;
        VIDEO_D1 = 10;
        VIDEO_D2 = 15;
        VIDEO_D3 = 13;
        VIDEO_D4 = 22;
        VIDEO_D5 = 12;
        VIDEO_D6 = 0;
        VIDEO_D7 = 6;
    }
    if (BOARD_TYPE == OUTBOUND_CUSTOM) {
        VIDEO_FRM = 10;
        VIDEO_CLK = 4;
        VIDEO_D0 = 6;
        VIDEO_D1 = 15;
        VIDEO_D2 = 12;
        VIDEO_D3 = 19;
        VIDEO_D4 = 26;
        VIDEO_D5 = 21;
        VIDEO_D6 = 0;
        VIDEO_D7 = 2;
    }

    video_build_masks();
}

void init_video_input_gpios() {
    gpio_init(VIDEO_CLK);
    gpio_set_dir(VIDEO_CLK, GPIO_IN);

    gpio_init(VIDEO_FRM);
    gpio_set_dir(VIDEO_FRM, GPIO_IN);

    gpio_init(VIDEO_D0);
    gpio_set_dir(VIDEO_D0, GPIO_IN);
    gpio_init(VIDEO_D1);
    gpio_set_dir(VIDEO_D1, GPIO_IN);
    gpio_init(VIDEO_D2);
    gpio_set_dir(VIDEO_D2, GPIO_IN);
    gpio_init(VIDEO_D3);
    gpio_set_dir(VIDEO_D3, GPIO_IN);
    gpio_init(VIDEO_D4);
    gpio_set_dir(VIDEO_D4, GPIO_IN);
    gpio_init(VIDEO_D5);
    gpio_set_dir(VIDEO_D5, GPIO_IN);
    gpio_init(VIDEO_D6);
    gpio_set_dir(VIDEO_D6, GPIO_IN);
    gpio_init(VIDEO_D7);
    gpio_set_dir(VIDEO_D7, GPIO_IN);
}

uint8_t read_parallel_byte() {
    const uint32_t in = sio_hw->gpio_in;

    uint8_t byte = 0;
    byte |= (uint8_t) (((in & VIDEO_M0) ? 1u : 0u) << 7);
    byte |= (uint8_t) (((in & VIDEO_M1) ? 1u : 0u) << 6);
    byte |= (uint8_t) (((in & VIDEO_M2) ? 1u : 0u) << 5);
    byte |= (uint8_t) (((in & VIDEO_M3) ? 1u : 0u) << 4);
    byte |= (uint8_t) (((in & VIDEO_M4) ? 1u : 0u) << 3);
    byte |= (uint8_t) (((in & VIDEO_M5) ? 1u : 0u) << 2);
    byte |= (uint8_t) (((in & VIDEO_M6) ? 1u : 0u) << 1);
    byte |= (uint8_t) (((in & VIDEO_M7) ? 1u : 0u) << 0);
    return byte;
}

/// Capture video data sent in a DSTN "dual-scan/dual-panel" 4-bit+4-bit format.
void capture_frame_dstn() {
    while (gpio_get(VIDEO_FRM) == 1) {
    }
    while (gpio_get(VIDEO_FRM) == 0) {
    }

    const uint32_t width = 640;
    const uint32_t height = 400;
    const uint32_t half_h = height / 2; // 200
    const uint32_t bytes_per_row = width / 8; // 80

    // Horizontal phase tweak (4 pixels per sample)
#define DISCARD_SAMPLES_AFTER_FRM 1u
    for (uint32_t i = 0; i < DISCARD_SAMPLES_AFTER_FRM; ++i) {
        while (gpio_get(VIDEO_CLK) == 1) {
        }
        (void) read_parallel_byte();
        while (gpio_get(VIDEO_CLK) == 0) {
        }
    }

    for (uint32_t y = 0; y < half_h; ++y) {
        // Top line missing fix
        const uint32_t dst_y = (y + 1) % half_h;

        for (uint32_t xb = 0; xb < bytes_per_row; ++xb) {
            while (gpio_get(VIDEO_CLK) == 1) {
            }
            const uint8_t s0 = read_parallel_byte();
            while (gpio_get(VIDEO_CLK) == 0) {
            }

            while (gpio_get(VIDEO_CLK) == 1) {
            }
            const uint8_t s1 = read_parallel_byte();
            while (gpio_get(VIDEO_CLK) == 0) {
            }

            const uint8_t top_byte =
                    (uint8_t) ((s1 & 0xF0u) | ((s0 & 0xF0u) >> 4));
            const uint8_t bottom_byte =
                    (uint8_t) (((s1 & 0x0Fu) << 4) | (s0 & 0x0Fu));

            const uint32_t top_index = (dst_y * bytes_per_row) + xb;
            const uint32_t bottom_index = ((dst_y + half_h) * bytes_per_row) + xb;

            frame_buffer[top_index] = top_byte;
            frame_buffer[bottom_index] = bottom_byte;
        }
    }
}

/// Capture video data sent in a TFT 8-bit format.
void capture_frame_tft() {
    while (gpio_get(VIDEO_FRM) == 0) {
    }

    uint32_t byte_index = 0;
    const uint32_t total_bytes = (640 * 400) / 8;

    while (byte_index < total_bytes) {
        while (gpio_get(VIDEO_CLK) == 1) {
        }

        frame_buffer[byte_index++] = read_parallel_byte();

        while (gpio_get(VIDEO_CLK) == 0);
    }
}

void convert_frame_buffer_to_framebuf() {
    // Convert into the back buffer, not the active display buffer
    uint8_t *target_buf = (display_ptr == framebuf) ? framebuf_back : framebuf;
    memset(target_buf, 0, 3 * PLANE_SIZE_BYTES);


    for (uint y = 0; y < 400; ++y) {
        const uint src_row_offset = y * (640 / 8);
        const uint dst_row_offset = y * (640 / 8);

        for (uint x_byte = 0; x_byte < (640 / 8); ++x_byte) {
            const uint8_t src_byte = frame_buffer[src_row_offset + x_byte];

            target_buf[dst_row_offset + x_byte + 0 * PLANE_SIZE_BYTES] = src_byte;
            target_buf[dst_row_offset + x_byte + 1 * PLANE_SIZE_BYTES] = src_byte;
            target_buf[dst_row_offset + x_byte + 2 * PLANE_SIZE_BYTES] = src_byte;
        }
    }
    // Swap the pointers after conversion is done
    display_ptr = target_buf;
}

void core1_main() {
    dvi_register_irqs_this_core(&dvi0, DMA_IRQ_0);
    dvi_start(&dvi0);
    while (true) {
        for (uint y = 0; y < FRAME_HEIGHT; ++y) {
            uint32_t *tmdsbuf = 0;
            queue_remove_blocking_u32(&dvi0.q_tmds_free, &tmdsbuf);
            for (uint component = 0; component < 3; ++component) {
                // Always read from the current display_ptr
                tmds_encode_1bpp(
                    (const uint32_t *) &display_ptr[y * FRAME_WIDTH / 8 + component * PLANE_SIZE_BYTES],
                    tmdsbuf + component * FRAME_WIDTH / DVI_SYMBOLS_PER_WORD,
                    FRAME_WIDTH);
            }
            queue_add_blocking_u32(&dvi0.q_tmds_valid, &tmdsbuf);
        }
    }
}


int main() {
    vreg_set_voltage(VREG_VSEL);
    sleep_ms(10);
    set_sys_clock_khz(DVI_TIMING.bit_clk_khz, true);

    setup_default_uart();

    init_setup();

    pio_set_gpio_base(DVI_DEFAULT_SERIAL_CONFIG.pio, 16);

    dvi0.timing = &DVI_TIMING;
    dvi0.ser_cfg = DVI_DEFAULT_SERIAL_CONFIG;
    dvi_init(&dvi0, next_striped_spin_lock_num(), next_striped_spin_lock_num());
    multicore_launch_core1(core1_main);

    init_video_input_gpios();

    if ((BOARD_TYPE == OUTBOUND) | (BOARD_TYPE == OUTBOUND_CUSTOM)) {
        while (1) {
            capture_frame_dstn();
            convert_frame_buffer_to_framebuf();
        }
    }
    if (BOARD_TYPE == PORTABLE) {
        while (1) {
            capture_frame_tft();
            convert_frame_buffer_to_framebuf();
        }
    }
}
