#include "slop3d.h"
#include <math.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#else
#ifndef EMSCRIPTEN_KEEPALIVE
#define EMSCRIPTEN_KEEPALIVE
#endif
#endif

/* ── helpers ─────────────────────────────────────────────────────────── */

static inline float deg_to_rad(float deg) {
    return deg * 3.14159265358979323846f / 180.0f;
}

/* ── vec3 ────────────────────────────────────────────────────────────── */

static inline S3D_Vec3 v3_add(S3D_Vec3 a, S3D_Vec3 b) {
    return (S3D_Vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static inline S3D_Vec3 v3_sub(S3D_Vec3 a, S3D_Vec3 b) {
    return (S3D_Vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static inline S3D_Vec3 v3_mul(S3D_Vec3 v, float s) {
    return (S3D_Vec3){v.x * s, v.y * s, v.z * s};
}

static inline S3D_Vec3 v3_scale(S3D_Vec3 a, S3D_Vec3 b) {
    return (S3D_Vec3){a.x * b.x, a.y * b.y, a.z * b.z};
}

static inline S3D_Vec3 v3_negate(S3D_Vec3 v) {
    return (S3D_Vec3){-v.x, -v.y, -v.z};
}

static inline float v3_dot(S3D_Vec3 a, S3D_Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline S3D_Vec3 v3_cross(S3D_Vec3 a, S3D_Vec3 b) {
    return (S3D_Vec3){a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                      a.x * b.y - a.y * b.x};
}

static inline float v3_length(S3D_Vec3 v) {
    return sqrtf(v3_dot(v, v));
}

static inline S3D_Vec3 v3_normalize(S3D_Vec3 v) {
    float len = v3_length(v);
    if (len < 1e-8f)
        return (S3D_Vec3){0, 0, 0};
    float inv = 1.0f / len;
    return (S3D_Vec3){v.x * inv, v.y * inv, v.z * inv};
}

static inline S3D_Vec3 v3_lerp(S3D_Vec3 a, S3D_Vec3 b, float t) {
    return (S3D_Vec3){a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                      a.z + (b.z - a.z) * t};
}

/* ── vec4 ────────────────────────────────────────────────────────────── */

static inline S3D_Vec4 v4_from_v3(S3D_Vec3 v, float w) {
    return (S3D_Vec4){v.x, v.y, v.z, w};
}

/* ── mat4 (column-major: m[col*4 + row]) ─────────────────────────────── */

static inline S3D_Mat4 m4_identity(void) {
    S3D_Mat4 r = {{0}};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

static inline S3D_Mat4 m4_multiply(S3D_Mat4 a, S3D_Mat4 b) {
    S3D_Mat4 r = {{0}};
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++) {
                sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
            r.m[col * 4 + row] = sum;
        }
    }
    return r;
}

static inline S3D_Vec4 m4_mul_vec4(S3D_Mat4 m, S3D_Vec4 v) {
    return (S3D_Vec4){m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z + m.m[12] * v.w,
                      m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z + m.m[13] * v.w,
                      m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z + m.m[14] * v.w,
                      m.m[3] * v.x + m.m[7] * v.y + m.m[11] * v.z + m.m[15] * v.w};
}

static inline S3D_Mat4 m4_translate(float x, float y, float z) {
    S3D_Mat4 r = m4_identity();
    r.m[12] = x;
    r.m[13] = y;
    r.m[14] = z;
    return r;
}

static inline S3D_Mat4 m4_scale(float x, float y, float z) {
    S3D_Mat4 r = {{0}};
    r.m[0] = x;
    r.m[5] = y;
    r.m[10] = z;
    r.m[15] = 1.0f;
    return r;
}

static inline S3D_Mat4 m4_rotate_x(float angle_deg) {
    float rad = deg_to_rad(angle_deg);
    float c = cosf(rad), s = sinf(rad);
    S3D_Mat4 r = m4_identity();
    r.m[5] = c;
    r.m[9] = -s;
    r.m[6] = s;
    r.m[10] = c;
    return r;
}

static inline S3D_Mat4 m4_rotate_y(float angle_deg) {
    float rad = deg_to_rad(angle_deg);
    float c = cosf(rad), s = sinf(rad);
    S3D_Mat4 r = m4_identity();
    r.m[0] = c;
    r.m[8] = s;
    r.m[2] = -s;
    r.m[10] = c;
    return r;
}

static inline S3D_Mat4 m4_rotate_z(float angle_deg) {
    float rad = deg_to_rad(angle_deg);
    float c = cosf(rad), s = sinf(rad);
    S3D_Mat4 r = m4_identity();
    r.m[0] = c;
    r.m[4] = -s;
    r.m[1] = s;
    r.m[5] = c;
    return r;
}

static inline S3D_Mat4 m4_perspective(float fov_deg, float aspect, float near,
                                      float far) {
    float f = 1.0f / tanf(deg_to_rad(fov_deg) * 0.5f);
    float nf = near - far;
    S3D_Mat4 r = {{0}};
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (far + near) / nf;
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * far * near) / nf;
    return r;
}

static inline S3D_Mat4 m4_lookat(S3D_Vec3 eye, S3D_Vec3 target, S3D_Vec3 up) {
    S3D_Vec3 f = v3_normalize(v3_sub(target, eye));
    S3D_Vec3 r = v3_normalize(v3_cross(f, up));
    S3D_Vec3 u = v3_cross(r, f);

    S3D_Mat4 m = m4_identity();
    m.m[0] = r.x;
    m.m[4] = r.y;
    m.m[8] = r.z;
    m.m[1] = u.x;
    m.m[5] = u.y;
    m.m[9] = u.z;
    m.m[2] = -f.x;
    m.m[6] = -f.y;
    m.m[10] = -f.z;
    m.m[12] = -v3_dot(r, eye);
    m.m[13] = -v3_dot(u, eye);
    m.m[14] = v3_dot(f, eye);
    return m;
}

static inline S3D_Mat4 m4_inverse_affine(S3D_Mat4 m) {
    S3D_Mat4 r = {{0}};
    /* transpose upper-left 3x3 */
    r.m[0] = m.m[0];
    r.m[4] = m.m[1];
    r.m[8] = m.m[2];
    r.m[1] = m.m[4];
    r.m[5] = m.m[5];
    r.m[9] = m.m[6];
    r.m[2] = m.m[8];
    r.m[6] = m.m[9];
    r.m[10] = m.m[10];
    /* inverse translation: -(R^T * t) */
    float tx = m.m[12], ty = m.m[13], tz = m.m[14];
    r.m[12] = -(r.m[0] * tx + r.m[4] * ty + r.m[8] * tz);
    r.m[13] = -(r.m[1] * tx + r.m[5] * ty + r.m[9] * tz);
    r.m[14] = -(r.m[2] * tx + r.m[6] * ty + r.m[10] * tz);
    r.m[15] = 1.0f;
    return r;
}

/* ── engine state ────────────────────────────────────────────────────── */

typedef struct {
    S3D_Vec3 position;
    S3D_Vec3 target;
    S3D_Vec3 up;
    float fov;
    float near_clip;
    float far_clip;
    S3D_Mat4 view;
    S3D_Mat4 projection;
    S3D_Mat4 vp;
} S3D_Camera;

typedef struct {
    uint8_t framebuffer[S3D_WIDTH * S3D_HEIGHT * 4];
    uint16_t zbuffer[S3D_WIDTH * S3D_HEIGHT];
    uint8_t clear_r, clear_g, clear_b, clear_a;
    S3D_Camera camera;
} S3D_Engine;

static S3D_Engine g_engine;

static void update_camera(void) {
    S3D_Camera *cam = &g_engine.camera;
    float aspect = (float)S3D_WIDTH / (float)S3D_HEIGHT;
    cam->view = m4_lookat(cam->position, cam->target, cam->up);
    cam->projection = m4_perspective(cam->fov, aspect, cam->near_clip, cam->far_clip);
    cam->vp = m4_multiply(cam->projection, cam->view);
}

/* ── exported functions ──────────────────────────────────────────────── */

EMSCRIPTEN_KEEPALIVE
void s3d_init(void) {
    memset(&g_engine, 0, sizeof(g_engine));
    g_engine.clear_a = 255;

    g_engine.camera.position = (S3D_Vec3){0.0f, 0.0f, 5.0f};
    g_engine.camera.target = (S3D_Vec3){0.0f, 0.0f, 0.0f};
    g_engine.camera.up = (S3D_Vec3){0.0f, 1.0f, 0.0f};
    g_engine.camera.fov = 60.0f;
    g_engine.camera.near_clip = 0.1f;
    g_engine.camera.far_clip = 100.0f;
    update_camera();
}

EMSCRIPTEN_KEEPALIVE
void s3d_shutdown(void) {}

EMSCRIPTEN_KEEPALIVE
void s3d_clear_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    g_engine.clear_r = r;
    g_engine.clear_g = g;
    g_engine.clear_b = b;
    g_engine.clear_a = a;
}

EMSCRIPTEN_KEEPALIVE
void s3d_frame_begin(void) {
    uint32_t color = (uint32_t)g_engine.clear_r | ((uint32_t)g_engine.clear_g << 8) |
                     ((uint32_t)g_engine.clear_b << 16) |
                     ((uint32_t)g_engine.clear_a << 24);

    uint32_t *fb = (uint32_t *)g_engine.framebuffer;
    int count = S3D_WIDTH * S3D_HEIGHT;
    for (int i = 0; i < count; i++) {
        fb[i] = color;
    }

    memset(g_engine.zbuffer, 0xFF, sizeof(g_engine.zbuffer));
}

EMSCRIPTEN_KEEPALIVE
uint8_t *s3d_get_framebuffer(void) {
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

EMSCRIPTEN_KEEPALIVE
void s3d_camera_set(float px, float py, float pz, float tx, float ty, float tz, float ux,
                    float uy, float uz) {
    g_engine.camera.position = (S3D_Vec3){px, py, pz};
    g_engine.camera.target = (S3D_Vec3){tx, ty, tz};
    g_engine.camera.up = (S3D_Vec3){ux, uy, uz};
    update_camera();
}

EMSCRIPTEN_KEEPALIVE
void s3d_camera_fov(float fov_degrees) {
    g_engine.camera.fov = fov_degrees;
    update_camera();
}

EMSCRIPTEN_KEEPALIVE
void s3d_camera_clip(float near_clip, float far_clip) {
    g_engine.camera.near_clip = near_clip;
    g_engine.camera.far_clip = far_clip;
    update_camera();
}
