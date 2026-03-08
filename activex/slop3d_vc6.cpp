/*
 * slop3d_vc6.cpp — VC6-compatible port of src/slop3d.c
 *
 * Mechanical changes from the original:
 *   - No stdint.h (typedefs in slop3d_vc6.h)
 *   - static inline -> static __forceinline
 *   - Compound literals (S3D_Vec3){x,y,z} -> s3d_vec3(x,y,z) helpers
 *   - For-loop declarations hoisted to block scope
 *   - No EMSCRIPTEN_KEEPALIVE
 */

#include "slop3d_vc6.h"
#include <math.h>
#include <string.h>

/* ── helpers ─────────────────────────────────────────────────────────── */

static __forceinline float deg_to_rad(float deg) {
    return deg * 3.14159265358979323846f / 180.0f;
}

/* ── vec3 ────────────────────────────────────────────────────────────── */

static __forceinline S3D_Vec3 v3_add(S3D_Vec3 a, S3D_Vec3 b) {
    return s3d_vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

static __forceinline S3D_Vec3 v3_sub(S3D_Vec3 a, S3D_Vec3 b) {
    return s3d_vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

static __forceinline S3D_Vec3 v3_mul(S3D_Vec3 v, float s) {
    return s3d_vec3(v.x * s, v.y * s, v.z * s);
}

static __forceinline S3D_Vec3 v3_scale(S3D_Vec3 a, S3D_Vec3 b) {
    return s3d_vec3(a.x * b.x, a.y * b.y, a.z * b.z);
}

static __forceinline S3D_Vec3 v3_negate(S3D_Vec3 v) {
    return s3d_vec3(-v.x, -v.y, -v.z);
}

static __forceinline float v3_dot(S3D_Vec3 a, S3D_Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static __forceinline S3D_Vec3 v3_cross(S3D_Vec3 a, S3D_Vec3 b) {
    return s3d_vec3(a.y * b.z - a.z * b.y,
                    a.z * b.x - a.x * b.z,
                    a.x * b.y - a.y * b.x);
}

static __forceinline float v3_length(S3D_Vec3 v) {
    return (float)sqrt(v3_dot(v, v));
}

static __forceinline S3D_Vec3 v3_normalize(S3D_Vec3 v) {
    float len = v3_length(v);
    float inv;
    if (len < 1e-8f)
        return s3d_vec3(0, 0, 0);
    inv = 1.0f / len;
    return s3d_vec3(v.x * inv, v.y * inv, v.z * inv);
}

static __forceinline S3D_Vec3 v3_lerp(S3D_Vec3 a, S3D_Vec3 b, float t) {
    return s3d_vec3(a.x + (b.x - a.x) * t,
                    a.y + (b.y - a.y) * t,
                    a.z + (b.z - a.z) * t);
}

/* ── vec4 ────────────────────────────────────────────────────────────── */

static __forceinline S3D_Vec4 v4_from_v3(S3D_Vec3 v, float w) {
    return s3d_vec4(v.x, v.y, v.z, w);
}

/* ── mat4 (column-major: m[col*4 + row]) ─────────────────────────────── */

static __forceinline S3D_Mat4 m4_identity(void) {
    S3D_Mat4 r;
    memset(&r, 0, sizeof(r));
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

static __forceinline S3D_Mat4 m4_multiply(S3D_Mat4 a, S3D_Mat4 b) {
    S3D_Mat4 r;
    int col, row, k;
    memset(&r, 0, sizeof(r));
    for (col = 0; col < 4; col++) {
        for (row = 0; row < 4; row++) {
            float sum = 0.0f;
            for (k = 0; k < 4; k++) {
                sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
            r.m[col * 4 + row] = sum;
        }
    }
    return r;
}

static __forceinline S3D_Vec4 m4_mul_vec4(S3D_Mat4 m, S3D_Vec4 v) {
    return s3d_vec4(
        m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z + m.m[12] * v.w,
        m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z + m.m[13] * v.w,
        m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z + m.m[14] * v.w,
        m.m[3] * v.x + m.m[7] * v.y + m.m[11] * v.z + m.m[15] * v.w);
}

static __forceinline S3D_Mat4 m4_translate(float x, float y, float z) {
    S3D_Mat4 r = m4_identity();
    r.m[12] = x;
    r.m[13] = y;
    r.m[14] = z;
    return r;
}

static __forceinline S3D_Mat4 m4_scale(float x, float y, float z) {
    S3D_Mat4 r;
    memset(&r, 0, sizeof(r));
    r.m[0] = x;
    r.m[5] = y;
    r.m[10] = z;
    r.m[15] = 1.0f;
    return r;
}

static __forceinline S3D_Mat4 m4_rotate_x(float angle_deg) {
    float rad = deg_to_rad(angle_deg);
    float c = (float)cos(rad), s = (float)sin(rad);
    S3D_Mat4 r = m4_identity();
    r.m[5] = c;
    r.m[9] = -s;
    r.m[6] = s;
    r.m[10] = c;
    return r;
}

static __forceinline S3D_Mat4 m4_rotate_y(float angle_deg) {
    float rad = deg_to_rad(angle_deg);
    float c = (float)cos(rad), s = (float)sin(rad);
    S3D_Mat4 r = m4_identity();
    r.m[0] = c;
    r.m[8] = s;
    r.m[2] = -s;
    r.m[10] = c;
    return r;
}

static __forceinline S3D_Mat4 m4_rotate_z(float angle_deg) {
    float rad = deg_to_rad(angle_deg);
    float c = (float)cos(rad), s = (float)sin(rad);
    S3D_Mat4 r = m4_identity();
    r.m[0] = c;
    r.m[4] = -s;
    r.m[1] = s;
    r.m[5] = c;
    return r;
}

static __forceinline S3D_Mat4 m4_perspective(float fov_deg, float aspect,
                                              float near_val, float far_val) {
    float f = 1.0f / (float)tan(deg_to_rad(fov_deg) * 0.5f);
    float nf = near_val - far_val;
    S3D_Mat4 r;
    memset(&r, 0, sizeof(r));
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (far_val + near_val) / nf;
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * far_val * near_val) / nf;
    return r;
}

static __forceinline S3D_Mat4 m4_lookat(S3D_Vec3 eye, S3D_Vec3 target, S3D_Vec3 up) {
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

static __forceinline S3D_Mat4 m4_inverse_affine(S3D_Mat4 m) {
    S3D_Mat4 r;
    float tx, ty, tz;
    memset(&r, 0, sizeof(r));
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
    tx = m.m[12]; ty = m.m[13]; tz = m.m[14];
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

void s3d_init(void) {
    memset(&g_engine, 0, sizeof(g_engine));
    g_engine.clear_a = 255;

    g_engine.camera.position = s3d_vec3(0.0f, 0.0f, 5.0f);
    g_engine.camera.target = s3d_vec3(0.0f, 0.0f, 0.0f);
    g_engine.camera.up = s3d_vec3(0.0f, 1.0f, 0.0f);
    g_engine.camera.fov = 60.0f;
    g_engine.camera.near_clip = 0.1f;
    g_engine.camera.far_clip = 100.0f;
    update_camera();
}

void s3d_shutdown(void) {}

void s3d_clear_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    g_engine.clear_r = r;
    g_engine.clear_g = g;
    g_engine.clear_b = b;
    g_engine.clear_a = a;
}

void s3d_frame_begin(void) {
    uint32_t color;
    uint32_t *fb;
    int i, count;

    color = (uint32_t)g_engine.clear_r | ((uint32_t)g_engine.clear_g << 8) |
            ((uint32_t)g_engine.clear_b << 16) | ((uint32_t)g_engine.clear_a << 24);

    fb = (uint32_t *)g_engine.framebuffer;
    count = S3D_WIDTH * S3D_HEIGHT;
    for (i = 0; i < count; i++) {
        fb[i] = color;
    }

    memset(g_engine.zbuffer, 0xFF, sizeof(g_engine.zbuffer));
}

uint8_t *s3d_get_framebuffer(void) {
    return g_engine.framebuffer;
}

int s3d_get_width(void) {
    return S3D_WIDTH;
}

int s3d_get_height(void) {
    return S3D_HEIGHT;
}

void s3d_camera_set(float px, float py, float pz, float tx, float ty, float tz,
                    float ux, float uy, float uz) {
    g_engine.camera.position = s3d_vec3(px, py, pz);
    g_engine.camera.target = s3d_vec3(tx, ty, tz);
    g_engine.camera.up = s3d_vec3(ux, uy, uz);
    update_camera();
}

void s3d_camera_fov(float fov_degrees) {
    g_engine.camera.fov = fov_degrees;
    update_camera();
}

void s3d_camera_clip(float near_clip, float far_clip) {
    g_engine.camera.near_clip = near_clip;
    g_engine.camera.far_clip = far_clip;
    update_camera();
}
