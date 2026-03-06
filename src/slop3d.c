#include "slop3d.h"
#include <string.h>
#include <emscripten/emscripten.h>

typedef struct {
    uint8_t  framebuffer[S3D_WIDTH * S3D_HEIGHT * 4];
    uint16_t zbuffer[S3D_WIDTH * S3D_HEIGHT];
    uint8_t  clear_r, clear_g, clear_b, clear_a;
} S3D_Engine;

static S3D_Engine g_engine;

EMSCRIPTEN_KEEPALIVE
void s3d_init(void) {
    memset(&g_engine, 0, sizeof(g_engine));
    g_engine.clear_a = 255;
}

EMSCRIPTEN_KEEPALIVE
void s3d_shutdown(void) {
}

EMSCRIPTEN_KEEPALIVE
void s3d_clear_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    g_engine.clear_r = r;
    g_engine.clear_g = g;
    g_engine.clear_b = b;
    g_engine.clear_a = a;
}

EMSCRIPTEN_KEEPALIVE
void s3d_frame_begin(void) {
    uint32_t color = (uint32_t)g_engine.clear_r
                   | ((uint32_t)g_engine.clear_g << 8)
                   | ((uint32_t)g_engine.clear_b << 16)
                   | ((uint32_t)g_engine.clear_a << 24);

    uint32_t *fb = (uint32_t *)g_engine.framebuffer;
    int count = S3D_WIDTH * S3D_HEIGHT;
    for (int i = 0; i < count; i++) {
        fb[i] = color;
    }

    memset(g_engine.zbuffer, 0xFF, sizeof(g_engine.zbuffer));
}

EMSCRIPTEN_KEEPALIVE
uint8_t* s3d_get_framebuffer(void) {
    return g_engine.framebuffer;
}

EMSCRIPTEN_KEEPALIVE
int s3d_get_width(void) {
    return S3D_WIDTH;
}

EMSCRIPTEN_KEEPALIVE
int s3d_get_height(void) {
    return S3D_HEIGHT;
}
