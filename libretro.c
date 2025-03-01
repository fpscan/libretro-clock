#include "libretro.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>

retro_video_refresh_t video_cb;
retro_input_poll_t input_poll_cb;
retro_input_state_t input_state_cb;
retro_environment_t environ_cb;
retro_audio_sample_t audio_cb;
retro_audio_sample_batch_t audio_batch_cb;

#define WIDTH 320
#define HEIGHT 240
#define MAX_FORMATS 4 // 0: Digital, 1: Analog, 2: Time Circuits, 3: Analog Sun Clock

static uint32_t frame_buffer[WIDTH * HEIGHT];
static int current_format = 0;
static int input_cooldown = 0;

static const unsigned char font_7seg[11][12] = {
    {0x7E, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x7E, 0x00}, // 0
    {0x30, 0x50, 0x50, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x7C, 0x00}, // 1
    {0x7E, 0x01, 0x01, 0x01, 0x7E, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7E, 0x00}, // 2
    {0x7E, 0x01, 0x01, 0x01, 0x7E, 0x01, 0x01, 0x01, 0x01, 0x01, 0x7E, 0x00}, // 3
    {0x42, 0x42, 0x42, 0x42, 0x7E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x00}, // 4
    {0x7E, 0x80, 0x80, 0x80, 0x7E, 0x01, 0x01, 0x01, 0x01, 0x01, 0x7E, 0x00}, // 5
    {0x7E, 0x80, 0x80, 0x80, 0x7E, 0x81, 0x81, 0x81, 0x81, 0x81, 0x7E, 0x00}, // 6
    {0x7E, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00}, // 7
    {0x7E, 0x81, 0x81, 0x81, 0x7E, 0x81, 0x81, 0x81, 0x81, 0x81, 0x7E, 0x00}, // 8
    {0x7E, 0x81, 0x81, 0x81, 0x7E, 0x01, 0x01, 0x01, 0x01, 0x01, 0x7E, 0x00}, // 9
    {0x00, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x00, 0x00}  // :
};

static const unsigned char font_7seg_alpha[26][12] = {
    {0x7E, 0x81, 0x81, 0x81, 0x7E, 0x81, 0x81, 0x81, 0x81, 0x81, 0x7E, 0x00}, // A
    {0x7C, 0x82, 0x82, 0x82, 0x7C, 0x82, 0x82, 0x82, 0x82, 0x7C, 0x00, 0x00}, // B
    {0x3C, 0x42, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x42, 0x3C, 0x00, 0x00}, // C
    {0x78, 0x44, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x44, 0x78, 0x00, 0x00}, // D
    {0x7E, 0x40, 0x40, 0x40, 0x7C, 0x40, 0x40, 0x40, 0x40, 0x7E, 0x00, 0x00}, // E
    {0x7E, 0x40, 0x40, 0x40, 0x7C, 0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0x00}, // F
    {0x3C, 0x42, 0x40, 0x40, 0x4E, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00, 0x00}, // G
    {0x42, 0x42, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00}, // H
    {0x3C, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x3C, 0x00, 0x00}, // I
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x44, 0x44, 0x38, 0x00, 0x00}, // J
    {0x42, 0x44, 0x48, 0x50, 0x60, 0x60, 0x50, 0x48, 0x44, 0x42, 0x00, 0x00}, // K
    {0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7E, 0x00, 0x00}, // L
    {0x42, 0x66, 0x5A, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00}, // M
    {0x42, 0x62, 0x52, 0x4A, 0x46, 0x42, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00}, // N
    {0x3C, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00, 0x00}, // O
    {0x7C, 0x42, 0x42, 0x42, 0x7C, 0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0x00}, // P
    {0x3C, 0x42, 0x42, 0x42, 0x42, 0x42, 0x4A, 0x44, 0x44, 0x3A, 0x00, 0x00}, // Q
    {0x7C, 0x42, 0x42, 0x42, 0x7C, 0x48, 0x44, 0x44, 0x42, 0x42, 0x00, 0x00}, // R
    {0x3C, 0x42, 0x40, 0x40, 0x3C, 0x02, 0x02, 0x02, 0x42, 0x3C, 0x00, 0x00}, // S
    {0x7E, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00}, // T
    {0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00, 0x00}, // U
    {0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x24, 0x24, 0x18, 0x18, 0x00, 0x00}, // V
    {0x42, 0x42, 0x42, 0x42, 0x42, 0x5A, 0x66, 0x42, 0x42, 0x42, 0x00, 0x00}, // W
    {0x42, 0x42, 0x24, 0x24, 0x18, 0x18, 0x24, 0x24, 0x42, 0x42, 0x00, 0x00}, // X
    {0x42, 0x42, 0x24, 0x24, 0x18, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00}, // Y
    {0x7E, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x40, 0x80, 0x7E, 0x00, 0x00}  // Z
};

// Matrix rain
#define MAX_DROPS 85
static struct {
    int x;
    int y;
    int speed;
} drops[MAX_DROPS];

void retro_init(void) {
    for (int i = 0; i < MAX_DROPS; i++) {
        drops[i].x = rand() % WIDTH;
        drops[i].y = rand() % HEIGHT;
        drops[i].speed = 1 + (rand() % 3);
    }
    input_cooldown = 0;
}

void retro_deinit(void) {}

unsigned retro_api_version(void) {
    return RETRO_API_VERSION;
}

void retro_set_environment(retro_environment_t cb) {
    environ_cb = cb;
    bool no_content = true;
    cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);
}

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { audio_cb = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

void retro_get_system_info(struct retro_system_info *info) {
    memset(info, 0, sizeof(*info));
    info->library_name = "Clock Core";
    info->library_version = "0.1";
    info->need_fullpath = false;
    info->valid_extensions = "";
}

void retro_get_system_av_info(struct retro_system_av_info *info) {
    info->timing.fps = 60.0;
    info->timing.sample_rate = 0.0;
    info->geometry.base_width = WIDTH;
    info->geometry.base_height = HEIGHT;
    info->geometry.max_width = WIDTH;
    info->geometry.max_height = HEIGHT;
    info->geometry.aspect_ratio = (float)WIDTH / HEIGHT;
}

void retro_set_controller_port_device(unsigned port, unsigned device) {
    (void)port;
    (void)device;
}

void retro_reset(void) {
    current_format = 0;
    input_cooldown = 0;
}

void draw_text(uint32_t *buffer, int x, int y, const char *text, uint32_t color) {
    for (int i = 0; text[i]; i++) {
        int char_idx = (text[i] == ':') ? 10 : (text[i] - '0');
        if (char_idx >= 0 && char_idx <= 10) {
            for (int dy = 0; dy < 12; dy++) {
                for (int dx = 0; dx < 8; dx++) {
                    if (font_7seg[char_idx][dy] & (1 << (7 - dx))) {
                        int px = x + i * 16 + dx;
                        int py = y + dy;
                        if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
                            buffer[py * WIDTH + px] = color;
                        }
                    }
                }
            }
        }
    }
}

void draw_alpha_text(uint32_t *buffer, int x, int y, const char *text, uint32_t color) {
    for (int i = 0; text[i]; i++) {
        int char_idx = text[i] - 'A';
        if (char_idx >= 0 && char_idx < 26) {
            for (int dy = 0; dy < 12; dy++) {
                for (int dx = 0; dx < 8; dx++) {
                    if (font_7seg_alpha[char_idx][dy] & (1 << (7 - dx))) {
                        int px = x + i * 16 + dx;
                        int py = y + dy;
                        if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
                            buffer[py * WIDTH + px] = color;
                        }
                    }
                }
            }
        }
    }
}

void draw_line(uint32_t *buffer, int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (1) {
        if (x0 >= 0 && x0 < WIDTH && y0 >= 0 && y0 < HEIGHT) {
            buffer[y0 * WIDTH + x0] = color; // 1px thick line
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void draw_filled_circle(uint32_t *buffer, int cx, int cy, int radius, uint32_t color) {
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x * x + y * y <= radius * radius) {
                int px = cx + x, py = cy + y;
                if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
                    buffer[py * WIDTH + px] = color;
                }
            }
        }
    }
}

void draw_analog_clock(uint32_t *buffer, int cx, int cy, int radius, time_t t) {
    struct tm *timeinfo = localtime(&t);
    int hour = timeinfo->tm_hour % 12;
    int min = timeinfo->tm_min;

    memset(buffer, 0, WIDTH * HEIGHT * sizeof(uint32_t));
    for (int i = 0; i < 360; i++) {
        int x = cx + radius * cos(i * 3.14159 / 180);
        int y = cy + radius * sin(i * 3.14159 / 180);
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
            buffer[y * WIDTH + x] = 0xFFFFFFFF;
        }
    }
    int hx = cx + (radius * 0.5) * cos((hour * 30 + min / 2.0 - 90) * 3.14159 / 180);
    int hy = cy + (radius * 0.5) * sin((hour * 30 + min / 2.0 - 90) * 3.14159 / 180);
    draw_line(buffer, cx, cy, hx, hy, 0xFF0000FF);
    int mx = cx + radius * cos((min * 6 - 90) * 3.14159 / 180);
    int my = cy + radius * sin((min * 6 - 90) * 3.14159 / 180);
    draw_line(buffer, cx, cy, mx, my, 0xFF00FF00);
}

void draw_time_circuits(uint32_t *buffer, time_t t) {
    struct tm *timeinfo = localtime(&t);
    char current_time[32];
    strftime(current_time, sizeof(current_time), "%I:%M", timeinfo); // 12 hour 

    // Metallic gray background
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            buffer[y * WIDTH + x] = 0xFF808080;
        }
    }

    uint32_t red = 0xFFFF0000, yellow = 0xFFFFFF00, green = 0xFF00FF00, white = 0xFFFFFFFF, gray = 0xFF808080;

    // BTT
    int box_y = 20, box_h = 20, gap = 10;
    for (int x = 20; x < 300; x++) buffer[box_y * WIDTH + x] = gray;
    draw_alpha_text(buffer, 25, box_y + 2, "OCT", red);
    draw_text(buffer, 75, box_y + 2, "26", red);
    draw_text(buffer, 125, box_y + 2, "1985", red);
    draw_text(buffer, 195, box_y + 2, "01", red);
    draw_text(buffer, 235, box_y + 2, ":", yellow);
    draw_text(buffer, 255, box_y + 2, "21", red);
    draw_alpha_text(buffer, 25, box_y + box_h + 10, "DESTINATION TIME", white);
    draw_line(buffer, 15, 15, 305, 15, red);
    draw_line(buffer, 15, 15, 15, 75, red);
    draw_line(buffer, 305, 15, 305, 75, red);
    draw_line(buffer, 15, 75, 305, 75, red);

    box_y = 90;
    for (int x = 20; x < 300; x++) buffer[box_y * WIDTH + x] = gray;
    draw_alpha_text(buffer, 25, box_y + 2, "OCT", green); // Current month (simplified)
    draw_text(buffer, 75, box_y + 2, "26", green);
    draw_text(buffer, 125, box_y + 2, "2025", green);
    draw_text(buffer, 195, box_y + 2, current_time, green);
    draw_alpha_text(buffer, 25, box_y + box_h + 10, "PRESENT TIME", white);
    draw_line(buffer, 15, 85, 305, 85, red);
    draw_line(buffer, 15, 85, 15, 145, red);
    draw_line(buffer, 305, 85, 305, 145, red);
    draw_line(buffer, 15, 145, 305, 145, red);

    box_y = 160;
    for (int x = 20; x < 300; x++) buffer[box_y * WIDTH + x] = gray;
    draw_alpha_text(buffer, 25, box_y + 2, "OCT", yellow);
    draw_text(buffer, 75, box_y + 2, "26", yellow);
    draw_text(buffer, 125, box_y + 2, "1985", yellow);
    draw_text(buffer, 195, box_y + 2, "08", yellow);
    draw_text(buffer, 235, box_y + 2, ":", yellow);
    draw_text(buffer, 255, box_y + 2, "20", yellow);
    draw_alpha_text(buffer, 25, box_y + box_h + 10, "LAST TIME DEPARTED", white);
    draw_line(buffer, 15, 155, 305, 155, red);
    draw_line(buffer, 15, 155, 15, 215, red);
    draw_line(buffer, 305, 155, 305, 215, red);
    draw_line(buffer, 15, 215, 305, 215, red);
}

// Analog Sun
void draw_analog_sun_clock(uint32_t *buffer, time_t t) {
    struct tm *timeinfo = localtime(&t);
    int hour = timeinfo->tm_hour;
    int min = timeinfo->tm_min;

    // Sky blue gradient background
    for (int y = 0; y < HEIGHT; y++) {
        uint32_t color = 0xFF87CEEB + ((255 - 87) * (HEIGHT - y) / HEIGHT) * 0x10000;
        for (int x = 0; x < WIDTH; x++) {
            buffer[y * WIDTH + x] = color;
        }
    }

    int cx = WIDTH / 2;
    int cy = HEIGHT / 2;
    int radius = 80;

    float time_normalized = (hour % 12 + min / 60.0) / 12.0 * 180.0;
    if (hour < 6 || hour >= 18) time_normalized = 0;
    else if (hour >= 6 && hour < 12) time_normalized = (time_normalized - 90) * -1;
    else time_normalized = time_normalized - 90;

    int sun_x = cx + radius * cos(time_normalized * 3.14159 / 180.0);
    int sun_y = cy + radius * sin(time_normalized * 3.14159 / 180.0);

    uint32_t sun_color = (hour < 12) ? 0xFFFFFF00 : 0xFFFFA500;

    draw_filled_circle(buffer, sun_x, sun_y, 10, sun_color);

    draw_line(buffer, cx - radius, cy, cx - radius + 10, cy, 0xFFFFFF00); // Short yellow line
    draw_line(buffer, cx, cy - radius, cx, cy - radius + 10, 0xFFFFFF00); // Short yellow line
    draw_line(buffer, cx + radius, cy, cx + radius - 10, cy, 0xFFFFFF00); // Short yellow line

    draw_filled_circle(buffer, sun_x, sun_y, 2, 0xFFFFFFFF); // White dot on sun center
}

void retro_run(void) {
    input_poll_cb();

    memset(frame_buffer, 0, WIDTH * HEIGHT * sizeof(uint32_t));

    if (input_cooldown > 0) {
        input_cooldown--;
    } else {
        if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT)) {
            current_format = (current_format + 1) % MAX_FORMATS;
            input_cooldown = 10;
        }
        if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT)) {
            current_format = (current_format - 1 + MAX_FORMATS) % MAX_FORMATS;
            input_cooldown = 10;
        }
    }

    time_t rawtime;
    time(&rawtime);
    struct tm *timeinfo = localtime(&rawtime);
    char time_str[32];

    switch (current_format) {
        case 0:
            strftime(time_str, sizeof(time_str), "%H:%M:%S", timeinfo);
            draw_text(frame_buffer, 100, 100, time_str, 0xFFFFFFFF);
            break;
        case 1:
            draw_analog_clock(frame_buffer, WIDTH / 2, HEIGHT / 2, 80, rawtime);
            break;
        case 2: 
            draw_time_circuits(frame_buffer, rawtime);
            break;
        case 3: 
            draw_analog_sun_clock(frame_buffer, rawtime);
            break;
    }

    video_cb(frame_buffer, WIDTH, HEIGHT, WIDTH * sizeof(uint32_t));
}

bool retro_load_game(const struct retro_game_info *game) {
    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
    if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt)) {
        return false;
    }
    return true;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info) {
    (void)game_type;
    (void)info;
    (void)num_info;
    return false;
}

void retro_unload_game(void) {}
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
size_t retro_serialize_size(void) { return 0; }
bool retro_serialize(void *data, size_t size) { return false; }
bool retro_unserialize(const void *data, size_t size) { return false; }
void *retro_get_memory_data(unsigned id) { return NULL; }
size_t retro_get_memory_size(unsigned id) { return 0; }
void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned index, bool enabled, const char *code) {}