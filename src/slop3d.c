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
    return s3d_vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

static inline S3D_Vec3 v3_sub(S3D_Vec3 a, S3D_Vec3 b) {
    return s3d_vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

static inline S3D_Vec3 v3_mul(S3D_Vec3 v, float s) {
    return s3d_vec3(v.x * s, v.y * s, v.z * s);
}

static inline S3D_Vec3 v3_scale(S3D_Vec3 a, S3D_Vec3 b) {
    return s3d_vec3(a.x * b.x, a.y * b.y, a.z * b.z);
}

static inline S3D_Vec3 v3_negate(S3D_Vec3 v) {
    return s3d_vec3(-v.x, -v.y, -v.z);
}

static inline float v3_dot(S3D_Vec3 a, S3D_Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline S3D_Vec3 v3_cross(S3D_Vec3 a, S3D_Vec3 b) {
    return s3d_vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

static inline float v3_length(S3D_Vec3 v) {
    return sqrtf(v3_dot(v, v));
}

static inline S3D_Vec3 v3_normalize(S3D_Vec3 v) {
    float len = v3_length(v);
    if (len < 1e-8f)
        return s3d_vec3(0, 0, 0);
    float inv = 1.0f / len;
    return s3d_vec3(v.x * inv, v.y * inv, v.z * inv);
}

static inline S3D_Vec3 v3_lerp(S3D_Vec3 a, S3D_Vec3 b, float t) {
    return s3d_vec3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
}

/* ── vec4 ────────────────────────────────────────────────────────────── */

static inline S3D_Vec4 v4_from_v3(S3D_Vec3 v, float w) {
    return s3d_vec4(v.x, v.y, v.z, w);
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
    return s3d_vec4(m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z + m.m[12] * v.w,
                    m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z + m.m[13] * v.w,
                    m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z + m.m[14] * v.w,
                    m.m[3] * v.x + m.m[7] * v.y + m.m[11] * v.z + m.m[15] * v.w);
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

static inline S3D_Mat4 m4_perspective(float fov_deg, float aspect, float near_val,
                                      float far_val) {
    float f = 1.0f / tanf(deg_to_rad(fov_deg) * 0.5f);
    float nf = near_val - far_val;
    S3D_Mat4 r = {{0}};
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (far_val + near_val) / nf;
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * far_val * near_val) / nf;
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

/* ── rasterizer types ────────────────────────────────────────────────── */

typedef struct {
    S3D_Vec4 clip;
    float r, g, b;
    float u, v;
} S3D_ClipVert;

typedef struct {
    float x, y;
    float z;
    float u, v;
    float r, g, b;
} S3D_ScreenVert;

/* ── rasterizer helpers ──────────────────────────────────────────────── */

static inline S3D_ClipVert lerp_clip_vert(S3D_ClipVert *a, S3D_ClipVert *b, float t) {
    S3D_ClipVert out;
    out.clip.x = a->clip.x + (b->clip.x - a->clip.x) * t;
    out.clip.y = a->clip.y + (b->clip.y - a->clip.y) * t;
    out.clip.z = a->clip.z + (b->clip.z - a->clip.z) * t;
    out.clip.w = a->clip.w + (b->clip.w - a->clip.w) * t;
    out.r = a->r + (b->r - a->r) * t;
    out.g = a->g + (b->g - a->g) * t;
    out.b = a->b + (b->b - a->b) * t;
    out.u = a->u + (b->u - a->u) * t;
    out.v = a->v + (b->v - a->v) * t;
    return out;
}

static inline S3D_ScreenVert ndc_to_screen(float ndc_x, float ndc_y, float ndc_z, float r,
                                           float g, float b) {
    S3D_ScreenVert sv;
    sv.x = (ndc_x * 0.5f + 0.5f) * (float)S3D_WIDTH;
    sv.y = (1.0f - (ndc_y * 0.5f + 0.5f)) * (float)S3D_HEIGHT;
    sv.z = ndc_z * 0.5f + 0.5f;
    sv.u = 0.0f;
    sv.v = 0.0f;
    sv.r = r;
    sv.g = g;
    sv.b = b;
    return sv;
}

static inline int is_front_facing(S3D_ScreenVert a, S3D_ScreenVert b, S3D_ScreenVert c) {
    float ex = b.x - a.x, ey = b.y - a.y;
    float fx = c.x - a.x, fy = c.y - a.y;
    return (ex * fy - ey * fx) >= 0.0f;
}

/* ── near-plane clipping (Sutherland-Hodgman) ────────────────────────── */

static int clip_near_plane(S3D_ClipVert in[], int in_count, S3D_ClipVert out[]) {
    int out_count = 0;
    for (int i = 0; i < in_count; i++) {
        S3D_ClipVert *cur = &in[i];
        S3D_ClipVert *prev = &in[(i + in_count - 1) % in_count];
        float d_cur = cur->clip.w + cur->clip.z;
        float d_prev = prev->clip.w + prev->clip.z;
        int cur_inside = (d_cur >= 0.0f);
        int prev_inside = (d_prev >= 0.0f);

        if (prev_inside != cur_inside) {
            float t = d_prev / (d_prev - d_cur);
            out[out_count++] = lerp_clip_vert(prev, cur, t);
        }
        if (cur_inside) {
            out[out_count++] = *cur;
        }
    }
    return out_count;
}

/* ── scanline rasterizer ─────────────────────────────────────────────── */

static inline float clampf(float v, float lo, float hi) {
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static void s3d_rasterize_triangle(S3D_ScreenVert v0, S3D_ScreenVert v1,
                                   S3D_ScreenVert v2) {
    /* sort by y ascending */
    S3D_ScreenVert tmp;
    if (v0.y > v1.y) {
        tmp = v0;
        v0 = v1;
        v1 = tmp;
    }
    if (v0.y > v2.y) {
        tmp = v0;
        v0 = v2;
        v2 = tmp;
    }
    if (v1.y > v2.y) {
        tmp = v1;
        v1 = v2;
        v2 = tmp;
    }

    float total_h = v2.y - v0.y;
    if (total_h < 0.001f)
        return;

    int y_start = (int)ceilf(v0.y);
    int y_end = (int)ceilf(v2.y) - 1;
    if (y_start < 0)
        y_start = 0;
    if (y_end >= S3D_HEIGHT)
        y_end = S3D_HEIGHT - 1;

    uint32_t *fb = (uint32_t *)g_engine.framebuffer;

    for (int y = y_start; y <= y_end; y++) {
        float fy = (float)y + 0.5f;

        /* long edge: v0 → v2 */
        float t_long = (fy - v0.y) / total_h;
        float lx = v0.x + (v2.x - v0.x) * t_long;
        float lz = v0.z + (v2.z - v0.z) * t_long;
        float lr = v0.r + (v2.r - v0.r) * t_long;
        float lg = v0.g + (v2.g - v0.g) * t_long;
        float lb = v0.b + (v2.b - v0.b) * t_long;

        /* short edge */
        float sx, sz, sr, sg, sb;
        if (fy < v1.y) {
            float h = v1.y - v0.y;
            if (h < 0.001f) {
                sx = v1.x;
                sz = v1.z;
                sr = v1.r;
                sg = v1.g;
                sb = v1.b;
            } else {
                float t = (fy - v0.y) / h;
                sx = v0.x + (v1.x - v0.x) * t;
                sz = v0.z + (v1.z - v0.z) * t;
                sr = v0.r + (v1.r - v0.r) * t;
                sg = v0.g + (v1.g - v0.g) * t;
                sb = v0.b + (v1.b - v0.b) * t;
            }
        } else {
            float h = v2.y - v1.y;
            if (h < 0.001f) {
                sx = v1.x;
                sz = v1.z;
                sr = v1.r;
                sg = v1.g;
                sb = v1.b;
            } else {
                float t = (fy - v1.y) / h;
                sx = v1.x + (v2.x - v1.x) * t;
                sz = v1.z + (v2.z - v1.z) * t;
                sr = v1.r + (v2.r - v1.r) * t;
                sg = v1.g + (v2.g - v1.g) * t;
                sb = v1.b + (v2.b - v1.b) * t;
            }
        }

        /* ensure left-to-right */
        float xl = lx, xr = sx;
        float zl = lz, zr = sz;
        float rl = lr, rr = sr;
        float gl = lg, gr = sg;
        float bl = lb, br = sb;
        if (xl > xr) {
            float t;
            t = xl;
            xl = xr;
            xr = t;
            t = zl;
            zl = zr;
            zr = t;
            t = rl;
            rl = rr;
            rr = t;
            t = gl;
            gl = gr;
            gr = t;
            t = bl;
            bl = br;
            br = t;
        }

        int ix_start = (int)ceilf(xl);
        int ix_end = (int)ceilf(xr) - 1;
        if (ix_start < 0)
            ix_start = 0;
        if (ix_end >= S3D_WIDTH)
            ix_end = S3D_WIDTH - 1;

        float span = xr - xl;
        if (span < 0.001f)
            continue;
        float inv_span = 1.0f / span;

        int row_offset = y * S3D_WIDTH;

        for (int x = ix_start; x <= ix_end; x++) {
            float s = ((float)x + 0.5f - xl) * inv_span;
            float z = zl + (zr - zl) * s;
            uint16_t z16 = (uint16_t)(clampf(z, 0.0f, 1.0f) * 65535.0f);

            int idx = row_offset + x;
            if (z16 <= g_engine.zbuffer[idx]) {
                g_engine.zbuffer[idx] = z16;
                uint8_t cr = (uint8_t)(clampf(rl + (rr - rl) * s, 0.0f, 1.0f) * 255.0f);
                uint8_t cg = (uint8_t)(clampf(gl + (gr - gl) * s, 0.0f, 1.0f) * 255.0f);
                uint8_t cb = (uint8_t)(clampf(bl + (br - bl) * s, 0.0f, 1.0f) * 255.0f);
                fb[idx] = (uint32_t)cr | ((uint32_t)cg << 8) | ((uint32_t)cb << 16) |
                          (255u << 24);
            }
        }
    }
}

/* ── camera ──────────────────────────────────────────────────────────── */

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

    g_engine.camera.position = s3d_vec3(0.0f, 0.0f, 5.0f);
    g_engine.camera.target = s3d_vec3(0.0f, 0.0f, 0.0f);
    g_engine.camera.up = s3d_vec3(0.0f, 1.0f, 0.0f);
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
    g_engine.camera.position = s3d_vec3(px, py, pz);
    g_engine.camera.target = s3d_vec3(tx, ty, tz);
    g_engine.camera.up = s3d_vec3(ux, uy, uz);
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

EMSCRIPTEN_KEEPALIVE
void s3d_draw_triangle(float x0, float y0, float z0, float r0, float g0, float b0,
                       float x1, float y1, float z1, float r1, float g1, float b1,
                       float x2, float y2, float z2, float r2, float g2, float b2) {
    S3D_Mat4 vp = g_engine.camera.vp;

    /* transform to clip space */
    S3D_ClipVert tri[3];
    tri[0].clip = m4_mul_vec4(vp, s3d_vec4(x0, y0, z0, 1.0f));
    tri[0].r = r0;
    tri[0].g = g0;
    tri[0].b = b0;
    tri[0].u = 0.0f;
    tri[0].v = 0.0f;
    tri[1].clip = m4_mul_vec4(vp, s3d_vec4(x1, y1, z1, 1.0f));
    tri[1].r = r1;
    tri[1].g = g1;
    tri[1].b = b1;
    tri[1].u = 0.0f;
    tri[1].v = 0.0f;
    tri[2].clip = m4_mul_vec4(vp, s3d_vec4(x2, y2, z2, 1.0f));
    tri[2].r = r2;
    tri[2].g = g2;
    tri[2].b = b2;
    tri[2].u = 0.0f;
    tri[2].v = 0.0f;

    /* near-plane clip */
    S3D_ClipVert clipped[7];
    int n = clip_near_plane(tri, 3, clipped);
    if (n < 3)
        return;

    /* fan clipped polygon into triangles */
    for (int i = 1; i < n - 1; i++) {
        S3D_ClipVert *a = &clipped[0];
        S3D_ClipVert *b = &clipped[i];
        S3D_ClipVert *c = &clipped[i + 1];

        /* perspective divide */
        float inv_w0 = 1.0f / a->clip.w;
        float inv_w1 = 1.0f / b->clip.w;
        float inv_w2 = 1.0f / c->clip.w;

        S3D_ScreenVert sv0 = ndc_to_screen(a->clip.x * inv_w0, a->clip.y * inv_w0,
                                           a->clip.z * inv_w0, a->r, a->g, a->b);
        S3D_ScreenVert sv1 = ndc_to_screen(b->clip.x * inv_w1, b->clip.y * inv_w1,
                                           b->clip.z * inv_w1, b->r, b->g, b->b);
        S3D_ScreenVert sv2 = ndc_to_screen(c->clip.x * inv_w2, c->clip.y * inv_w2,
                                           c->clip.z * inv_w2, c->r, c->g, c->b);

        /* backface cull */
        if (!is_front_facing(sv0, sv1, sv2))
            continue;

        s3d_rasterize_triangle(sv0, sv1, sv2);
    }
}
