#include "slop3d.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#else
#ifndef EMSCRIPTEN_KEEPALIVE
#define EMSCRIPTEN_KEEPALIVE
#endif
#endif

/* ── constants ───────────────────────────────────────────────────────── */

#define S3D_PI 3.14159265358979323846f
#define S3D_EPSILON 1e-8f
#define S3D_DEGENERATE_HEIGHT 0.001f
#define S3D_MIN_SPAN 1e-6f
#define S3D_MAX_FACE_VERTS 32
#define S3D_PACK_RGBA(r, g, b, a) ((uint32_t)(r) | ((uint32_t)(g) << 8) | ((uint32_t)(b) << 16) | ((uint32_t)(a) << 24))
#define S3D_PERSP_SUBDIV 16

/* ── helpers ─────────────────────────────────────────────────────────── */

static inline float deg_to_rad(float deg) { return deg * S3D_PI / 180.0f; }

/* ── vec3 ────────────────────────────────────────────────────────────── */

static inline S3D_Vec3 v3_add(S3D_Vec3 a, S3D_Vec3 b) { return s3d_vec3(a.x + b.x, a.y + b.y, a.z + b.z); }

static inline S3D_Vec3 v3_sub(S3D_Vec3 a, S3D_Vec3 b) { return s3d_vec3(a.x - b.x, a.y - b.y, a.z - b.z); }

static inline S3D_Vec3 v3_mul(S3D_Vec3 v, float s) { return s3d_vec3(v.x * s, v.y * s, v.z * s); }

static inline S3D_Vec3 v3_scale(S3D_Vec3 a, S3D_Vec3 b) { return s3d_vec3(a.x * b.x, a.y * b.y, a.z * b.z); }

static inline S3D_Vec3 v3_negate(S3D_Vec3 v) { return s3d_vec3(-v.x, -v.y, -v.z); }

static inline float v3_dot(S3D_Vec3 a, S3D_Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

static inline S3D_Vec3 v3_cross(S3D_Vec3 a, S3D_Vec3 b) {
    return s3d_vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

static inline float v3_length(S3D_Vec3 v) { return sqrtf(v3_dot(v, v)); }

static inline S3D_Vec3 v3_normalize(S3D_Vec3 v) {
    float len = v3_length(v);
    if (len < S3D_EPSILON) return s3d_vec3(0, 0, 0);
    float inv = 1.0f / len;
    return s3d_vec3(v.x * inv, v.y * inv, v.z * inv);
}

static inline S3D_Vec3 v3_lerp(S3D_Vec3 a, S3D_Vec3 b, float t) {
    return s3d_vec3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
}

/* ── vec4 ────────────────────────────────────────────────────────────── */

static inline S3D_Vec4 v4_from_v3(S3D_Vec3 v, float w) { return s3d_vec4(v.x, v.y, v.z, w); }

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
            for (int k = 0; k < 4; k++) { sum += a.m[k * 4 + row] * b.m[col * 4 + k]; }
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

static inline S3D_Mat4 m4_perspective(float fov_deg, float aspect, float near_val, float far_val) {
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

/* ── bounds-check macros ─────────────────────────────────────────────── */

#define S3D_CHECK_CAMERA(id)                                                                                           \
    if ((id) < 0 || (id) >= S3D_MAX_CAMERAS) return
#define S3D_CHECK_CAMERA_ACTIVE(id)                                                                                    \
    if ((id) < 0 || (id) >= S3D_MAX_CAMERAS || !g_engine.cameras[(id)].active) return
#define S3D_CHECK_OBJECT(id)                                                                                           \
    if ((id) < 0 || (id) >= S3D_MAX_OBJECTS) return
#define S3D_CHECK_OBJECT_ACTIVE(id)                                                                                    \
    if ((id) < 0 || (id) >= S3D_MAX_OBJECTS || !g_engine.objects[(id)].active) return
#define S3D_CHECK_LIGHT(id)                                                                                            \
    if ((id) < 0 || (id) >= S3D_MAX_LIGHTS) return
#define S3D_CHECK_MESH(id)                                                                                             \
    if ((id) < 0 || (id) >= S3D_MAX_MESHES || !g_engine.meshes[(id)].active) return
#define S3D_CHECK_TEXTURE(id)                                                                                          \
    if ((id) < 0 || (id) >= S3D_MAX_TEXTURES || !g_engine.textures[(id)].active) return

/* ── engine state ────────────────────────────────────────────────────── */

typedef struct {
    int active;
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
    S3D_Camera cameras[S3D_MAX_CAMERAS];
    int active_camera;

    S3D_Texture textures[S3D_MAX_TEXTURES];
    S3D_Mesh meshes[S3D_MAX_MESHES];
    S3D_Vertex vertex_pool[S3D_MAX_VERTICES];
    int vertex_pool_used;
    S3D_Triangle triangle_pool[S3D_MAX_TRIANGLES];
    int triangle_pool_used;
    S3D_Object objects[S3D_MAX_OBJECTS];
    S3D_Light lights[S3D_MAX_LIGHTS];
    S3D_Fog fog;
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
    float eye_z;
    float uw, vw, invw;
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

static inline S3D_ScreenVert ndc_to_screen(float ndc_x, float ndc_y, float ndc_z, float r, float g, float b, float u,
                                           float v, float eye_z) {
    S3D_ScreenVert sv;
    sv.x = (ndc_x * 0.5f + 0.5f) * (float)S3D_WIDTH;
    sv.y = (1.0f - (ndc_y * 0.5f + 0.5f)) * (float)S3D_HEIGHT;
    sv.z = ndc_z * 0.5f + 0.5f;
    sv.u = u;
    sv.v = v;
    sv.r = r;
    sv.g = g;
    sv.b = b;
    sv.eye_z = eye_z;
    sv.invw = 1.0f / eye_z;
    sv.uw = u * sv.invw;
    sv.vw = v * sv.invw;
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
        if (cur_inside) { out[out_count++] = *cur; }
    }
    return out_count;
}

/* ── scanline rasterizer ─────────────────────────────────────────────── */

static inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

typedef struct {
    float x, z, r, g, b, u, v, ez;
    float uw, vw, invw;
} S3D_EdgeVals;

static inline S3D_EdgeVals interpolate_edge(S3D_ScreenVert *a, S3D_ScreenVert *b, float fy) {
    S3D_EdgeVals e;
    float h = b->y - a->y;
    if (h < S3D_DEGENERATE_HEIGHT) {
        e.x = b->x;
        e.z = b->z;
        e.r = b->r;
        e.g = b->g;
        e.b = b->b;
        e.u = b->u;
        e.v = b->v;
        e.ez = b->eye_z;
        e.uw = b->uw;
        e.vw = b->vw;
        e.invw = b->invw;
    } else {
        float t = clampf((fy - a->y) / h, 0.0f, 1.0f);
        e.x = a->x + (b->x - a->x) * t;
        e.z = a->z + (b->z - a->z) * t;
        e.r = a->r + (b->r - a->r) * t;
        e.g = a->g + (b->g - a->g) * t;
        e.b = a->b + (b->b - a->b) * t;
        e.u = a->u + (b->u - a->u) * t;
        e.v = a->v + (b->v - a->v) * t;
        e.ez = a->eye_z + (b->eye_z - a->eye_z) * t;
        e.uw = a->uw + (b->uw - a->uw) * t;
        e.vw = a->vw + (b->vw - a->vw) * t;
        e.invw = a->invw + (b->invw - a->invw) * t;
    }
    return e;
}

static inline void swap_edges(S3D_EdgeVals *a, S3D_EdgeVals *b) {
    S3D_EdgeVals t = *a;
    *a = *b;
    *b = t;
}

/* pixel body macro — shared by affine and perspective scanline loops */
#define S3D_RASTER_PIXEL(x, z, cu, cv, cr_f, cg_f, cb_f, ez, row_offset, fb, tex, obj_alpha)                           \
    do {                                                                                                               \
        uint16_t z16 = (uint16_t)(clampf(z, 0.0f, 1.0f) * 65535.0f);                                                   \
        int idx = (row_offset) + (x);                                                                                  \
        if (z16 <= g_engine.zbuffer[idx]) {                                                                            \
            float fr = clampf(cr_f, 0.0f, 1.0f);                                                                       \
            float fg = clampf(cg_f, 0.0f, 1.0f);                                                                       \
            float fb_c = clampf(cb_f, 0.0f, 1.0f);                                                                     \
            if (tex) {                                                                                                 \
                int wmask = (tex)->width - 1;                                                                          \
                int hmask = (tex)->height - 1;                                                                         \
                int iu = (int)((cu) * (float)(tex)->width) & wmask;                                                    \
                int iv = (int)((cv) * (float)(tex)->height) & hmask;                                                   \
                int tidx = (iv * (tex)->width + iu) * 4;                                                               \
                fr *= (tex)->pixels[tidx] * (1.0f / 255.0f);                                                           \
                fg *= (tex)->pixels[tidx + 1] * (1.0f / 255.0f);                                                       \
                fb_c *= (tex)->pixels[tidx + 2] * (1.0f / 255.0f);                                                     \
            }                                                                                                          \
            if (g_engine.fog.enabled) {                                                                                \
                float fog_range = g_engine.fog.end - g_engine.fog.start;                                               \
                float fog_t =                                                                                          \
                    (fog_range > S3D_MIN_SPAN) ? clampf(((ez) - g_engine.fog.start) / fog_range, 0.0f, 1.0f) : 0.0f;   \
                float inv_fog = 1.0f - fog_t;                                                                          \
                fr = fr * inv_fog + g_engine.fog.r * fog_t;                                                            \
                fg = fg * inv_fog + g_engine.fog.g * fog_t;                                                            \
                fb_c = fb_c * inv_fog + g_engine.fog.b * fog_t;                                                        \
            }                                                                                                          \
            if ((obj_alpha) >= 1.0f) {                                                                                 \
                g_engine.zbuffer[idx] = z16;                                                                           \
                uint8_t cr = (uint8_t)(fr * 255.0f);                                                                   \
                uint8_t cg = (uint8_t)(fg * 255.0f);                                                                   \
                uint8_t cb = (uint8_t)(fb_c * 255.0f);                                                                 \
                (fb)[idx] = S3D_PACK_RGBA(cr, cg, cb, 255u);                                                           \
            } else {                                                                                                   \
                uint32_t dst = (fb)[idx];                                                                              \
                float pdr = (float)(dst & 0xFF) * (1.0f / 255.0f);                                                     \
                float pdg = (float)((dst >> 8) & 0xFF) * (1.0f / 255.0f);                                              \
                float pdb = (float)((dst >> 16) & 0xFF) * (1.0f / 255.0f);                                             \
                float inv_a = 1.0f - (obj_alpha);                                                                      \
                fr = fr * (obj_alpha) + pdr * inv_a;                                                                   \
                fg = fg * (obj_alpha) + pdg * inv_a;                                                                   \
                fb_c = fb_c * (obj_alpha) + pdb * inv_a;                                                               \
                uint8_t cr = (uint8_t)(clampf(fr, 0.0f, 1.0f) * 255.0f);                                               \
                uint8_t cg = (uint8_t)(clampf(fg, 0.0f, 1.0f) * 255.0f);                                               \
                uint8_t cb = (uint8_t)(clampf(fb_c, 0.0f, 1.0f) * 255.0f);                                             \
                (fb)[idx] = S3D_PACK_RGBA(cr, cg, cb, 255u);                                                           \
            }                                                                                                          \
        }                                                                                                              \
    } while (0)

static void s3d_rasterize_triangle(S3D_ScreenVert v0, S3D_ScreenVert v1, S3D_ScreenVert v2, int texture_id,
                                   float obj_alpha, int texmap) {
    S3D_Texture *tex = NULL;
    if (texture_id >= 0 && texture_id < S3D_MAX_TEXTURES && g_engine.textures[texture_id].active) {
        tex = &g_engine.textures[texture_id];
    }

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
    if (total_h < S3D_DEGENERATE_HEIGHT) return;

    int y_start = (int)ceilf(v0.y);
    int y_end = (int)ceilf(v2.y) - 1;
    if (y_start < 0) y_start = 0;
    if (y_end >= S3D_HEIGHT) y_end = S3D_HEIGHT - 1;

    uint32_t *fb = (uint32_t *)g_engine.framebuffer;

    for (int y = y_start; y <= y_end; y++) {
        float fy = (float)y;

        /* long edge: v0 → v2 */
        S3D_EdgeVals le = interpolate_edge(&v0, &v2, fy);

        /* short edge */
        S3D_EdgeVals se = (fy < v1.y) ? interpolate_edge(&v0, &v1, fy) : interpolate_edge(&v1, &v2, fy);

        /* ensure left-to-right */
        if (le.x > se.x) swap_edges(&le, &se);

        int ix_start = (int)ceilf(le.x);
        int ix_end = (int)ceilf(se.x) - 1;
        if (ix_start < 0) ix_start = 0;
        if (ix_end >= S3D_WIDTH) ix_end = S3D_WIDTH - 1;

        float span = se.x - le.x;
        if (span < S3D_MIN_SPAN) continue;
        float inv_span = 1.0f / span;

        /* precompute per-scanline step values */
        float dz = (se.z - le.z) * inv_span;
        float dr = (se.r - le.r) * inv_span;
        float dg = (se.g - le.g) * inv_span;
        float db = (se.b - le.b) * inv_span;
        float du = (se.u - le.u) * inv_span;
        float dv = (se.v - le.v) * inv_span;
        float dez = (se.ez - le.ez) * inv_span;

        /* initial values at ix_start */
        float s0 = ((float)ix_start + 0.5f - le.x) * inv_span;
        float z = le.z + (se.z - le.z) * s0;
        float cr_f = le.r + (se.r - le.r) * s0;
        float cg_f = le.g + (se.g - le.g) * s0;
        float cb_f = le.b + (se.b - le.b) * s0;
        float cu = le.u + (se.u - le.u) * s0;
        float cv = le.v + (se.v - le.v) * s0;
        float ez = le.ez + (se.ez - le.ez) * s0;

        int row_offset = y * S3D_WIDTH;

        if (texmap == S3D_TEXMAP_PERSPECTIVE && tex) {
            /* perspective-correct path (Abrash subdivision) */
            float cur_uw = le.uw + (se.uw - le.uw) * s0;
            float cur_vw = le.vw + (se.vw - le.vw) * s0;
            float cur_invw = le.invw + (se.invw - le.invw) * s0;
            float d_uw = (se.uw - le.uw) * inv_span;
            float d_vw = (se.vw - le.vw) * inv_span;
            float d_invw = (se.invw - le.invw) * inv_span;

            int x = ix_start;
            while (x <= ix_end) {
                int remain = ix_end - x + 1;
                int step = (remain < S3D_PERSP_SUBDIV) ? remain : S3D_PERSP_SUBDIV;
                /* true UV at start of sub-span */
                float rw0 = 1.0f / cur_invw;
                float u0 = cur_uw * rw0;
                float v0 = cur_vw * rw0;
                /* true UV at end of sub-span */
                float end_uw = cur_uw + d_uw * step;
                float end_vw = cur_vw + d_vw * step;
                float end_invw = cur_invw + d_invw * step;
                float rw1 = 1.0f / end_invw;
                float u1 = end_uw * rw1;
                float v1 = end_vw * rw1;
                /* affine sub-steps within this chunk */
                float inv_step = 1.0f / (float)step;
                float du_sub = (u1 - u0) * inv_step;
                float dv_sub = (v1 - v0) * inv_step;
                cu = u0;
                cv = v0;
                for (int i = 0; i < step; i++, x++) {
                    S3D_RASTER_PIXEL(x, z, cu, cv, cr_f, cg_f, cb_f, ez, row_offset, fb, tex, obj_alpha);
                    z += dz;
                    cr_f += dr;
                    cg_f += dg;
                    cb_f += db;
                    cu += du_sub;
                    cv += dv_sub;
                    ez += dez;
                }
                cur_uw = end_uw;
                cur_vw = end_vw;
                cur_invw = end_invw;
            }
        } else {
            /* affine path (default) */
            for (int x = ix_start; x <= ix_end; x++) {
                S3D_RASTER_PIXEL(x, z, cu, cv, cr_f, cg_f, cb_f, ez, row_offset, fb, tex, obj_alpha);
                z += dz;
                cr_f += dr;
                cg_f += dg;
                cb_f += db;
                cu += du;
                cv += dv;
                ez += dez;
            }
        }
    }
}

/* ── camera ──────────────────────────────────────────────────────────── */

static void update_camera(int id) {
    S3D_Camera *cam = &g_engine.cameras[id];
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
    g_engine.active_camera = -1;
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
    uint32_t color = S3D_PACK_RGBA(g_engine.clear_r, g_engine.clear_g, g_engine.clear_b, g_engine.clear_a);

    uint32_t *fb = (uint32_t *)g_engine.framebuffer;
    int count = S3D_WIDTH * S3D_HEIGHT;
    for (int i = 0; i < count; i++) { fb[i] = color; }

    memset(g_engine.zbuffer, 0xFF, sizeof(g_engine.zbuffer));
}

EMSCRIPTEN_KEEPALIVE
uint8_t *s3d_get_framebuffer(void) { return g_engine.framebuffer; }

EMSCRIPTEN_KEEPALIVE
int s3d_get_width(void) { return S3D_WIDTH; }

EMSCRIPTEN_KEEPALIVE
int s3d_get_height(void) { return S3D_HEIGHT; }

EMSCRIPTEN_KEEPALIVE
int s3d_camera_create(float px, float py, float pz, float tx, float ty, float tz) {
    for (int i = 0; i < S3D_MAX_CAMERAS; i++) {
        if (!g_engine.cameras[i].active) {
            S3D_Camera *cam = &g_engine.cameras[i];
            cam->active = 1;
            cam->position = s3d_vec3(px, py, pz);
            cam->target = s3d_vec3(tx, ty, tz);
            cam->up = s3d_vec3(0.0f, 1.0f, 0.0f);
            cam->fov = 60.0f;
            cam->near_clip = 0.1f;
            cam->far_clip = 100.0f;
            update_camera(i);
            return i;
        }
    }
    return -1;
}

EMSCRIPTEN_KEEPALIVE
void s3d_camera_destroy(int camera_id) {
    S3D_CHECK_CAMERA(camera_id);
    g_engine.cameras[camera_id].active = 0;
    if (g_engine.active_camera == camera_id) g_engine.active_camera = -1;
}

EMSCRIPTEN_KEEPALIVE
void s3d_camera_pos(int camera_id, float x, float y, float z) {
    S3D_CHECK_CAMERA_ACTIVE(camera_id);
    g_engine.cameras[camera_id].position = s3d_vec3(x, y, z);
    update_camera(camera_id);
}

EMSCRIPTEN_KEEPALIVE
void s3d_camera_target(int camera_id, float x, float y, float z) {
    S3D_CHECK_CAMERA_ACTIVE(camera_id);
    g_engine.cameras[camera_id].target = s3d_vec3(x, y, z);
    update_camera(camera_id);
}

EMSCRIPTEN_KEEPALIVE
void s3d_camera_set_fov(int camera_id, float fov_degrees) {
    S3D_CHECK_CAMERA_ACTIVE(camera_id);
    g_engine.cameras[camera_id].fov = fov_degrees;
    update_camera(camera_id);
}

EMSCRIPTEN_KEEPALIVE
void s3d_camera_set_clip(int camera_id, float near_clip, float far_clip) {
    S3D_CHECK_CAMERA_ACTIVE(camera_id);
    g_engine.cameras[camera_id].near_clip = near_clip;
    g_engine.cameras[camera_id].far_clip = far_clip;
    update_camera(camera_id);
}

EMSCRIPTEN_KEEPALIVE
void s3d_camera_activate(int camera_id) {
    S3D_CHECK_CAMERA_ACTIVE(camera_id);
    g_engine.active_camera = camera_id;
}

EMSCRIPTEN_KEEPALIVE
void s3d_camera_off(int camera_id) {
    S3D_CHECK_CAMERA(camera_id);
    if (g_engine.active_camera == camera_id) g_engine.active_camera = -1;
}

EMSCRIPTEN_KEEPALIVE
int s3d_camera_get_active(void) { return g_engine.active_camera; }

/* ── texture API ─────────────────────────────────────────────────────── */

EMSCRIPTEN_KEEPALIVE
int s3d_texture_create(int width, int height) {
    if (width <= 0 || width > S3D_MAX_TEX_SIZE || height <= 0 || height > S3D_MAX_TEX_SIZE) return -1;
    for (int i = 0; i < S3D_MAX_TEXTURES; i++) {
        if (!g_engine.textures[i].active) {
            g_engine.textures[i].active = 1;
            g_engine.textures[i].width = width;
            g_engine.textures[i].height = height;
            return i;
        }
    }
    return -1;
}

EMSCRIPTEN_KEEPALIVE
uint8_t *s3d_texture_get_data_ptr(int texture_id) {
    if (texture_id < 0 || texture_id >= S3D_MAX_TEXTURES || !g_engine.textures[texture_id].active) return 0;
    return g_engine.textures[texture_id].pixels;
}

/* ── mesh API ────────────────────────────────────────────────────────── */

EMSCRIPTEN_KEEPALIVE
int s3d_mesh_create(int vertex_count, int triangle_count) {
    if (vertex_count <= 0 || triangle_count <= 0) return -1;
    if (g_engine.vertex_pool_used + vertex_count > S3D_MAX_VERTICES) return -1;
    if (g_engine.triangle_pool_used + triangle_count > S3D_MAX_TRIANGLES) return -1;
    for (int i = 0; i < S3D_MAX_MESHES; i++) {
        if (!g_engine.meshes[i].active) {
            g_engine.meshes[i].active = 1;
            g_engine.meshes[i].vertex_offset = g_engine.vertex_pool_used;
            g_engine.meshes[i].vertex_count = vertex_count;
            g_engine.meshes[i].triangle_offset = g_engine.triangle_pool_used;
            g_engine.meshes[i].triangle_count = triangle_count;
            g_engine.vertex_pool_used += vertex_count;
            g_engine.triangle_pool_used += triangle_count;
            return i;
        }
    }
    return -1;
}

EMSCRIPTEN_KEEPALIVE
float *s3d_mesh_get_vertex_ptr(int mesh_id) {
    if (mesh_id < 0 || mesh_id >= S3D_MAX_MESHES || !g_engine.meshes[mesh_id].active) return 0;
    return (float *)&g_engine.vertex_pool[g_engine.meshes[mesh_id].vertex_offset];
}

EMSCRIPTEN_KEEPALIVE
uint16_t *s3d_mesh_get_index_ptr(int mesh_id) {
    if (mesh_id < 0 || mesh_id >= S3D_MAX_MESHES || !g_engine.meshes[mesh_id].active) return 0;
    return (uint16_t *)&g_engine.triangle_pool[g_engine.meshes[mesh_id].triangle_offset];
}

/* ── OBJ parser ──────────────────────────────────────────────────────── */

#define S3D_OBJ_MAX 4096

EMSCRIPTEN_KEEPALIVE
int s3d_mesh_load_obj(const char *obj_text, int len) {
    static float positions[S3D_OBJ_MAX * 3];
    static float texcoords[S3D_OBJ_MAX * 2];
    static float normals[S3D_OBJ_MAX * 3];
    int pos_count = 0, tex_count = 0, norm_count = 0;

    /* temporary vertex/triangle output */
    static S3D_Vertex tmp_verts[S3D_OBJ_MAX * 3];
    static S3D_Triangle tmp_tris[S3D_OBJ_MAX * 2];
    int vert_count = 0, tri_count = 0;

    const char *p = obj_text;
    const char *end = obj_text + len;

    while (p < end) {
        /* skip whitespace */
        while (p < end && (*p == ' ' || *p == '\t')) p++;

        if (p >= end) break;

        if (*p == 'v' && p + 1 < end && p[1] == ' ') {
            /* vertex position: v x y z */
            float x = 0, y = 0, z = 0;
            p += 2;
            x = (float)strtod(p, (char **)&p);
            y = (float)strtod(p, (char **)&p);
            z = (float)strtod(p, (char **)&p);
            if (pos_count < S3D_OBJ_MAX) {
                positions[pos_count * 3] = x;
                positions[pos_count * 3 + 1] = y;
                positions[pos_count * 3 + 2] = z;
                pos_count++;
            } else {
                fprintf(stderr, "s3d: OBJ vertex positions exceeded limit (%d)\n", S3D_OBJ_MAX);
            }
        } else if (*p == 'v' && p + 1 < end && p[1] == 't' && p + 2 < end && p[2] == ' ') {
            /* texcoord: vt u v */
            float u = 0, v = 0;
            p += 3;
            u = (float)strtod(p, (char **)&p);
            v = (float)strtod(p, (char **)&p);
            if (tex_count < S3D_OBJ_MAX) {
                texcoords[tex_count * 2] = u;
                texcoords[tex_count * 2 + 1] = v;
                tex_count++;
            } else {
                fprintf(stderr, "s3d: OBJ texcoords exceeded limit (%d)\n", S3D_OBJ_MAX);
            }
        } else if (*p == 'v' && p + 1 < end && p[1] == 'n' && p + 2 < end && p[2] == ' ') {
            /* normal: vn x y z */
            float x = 0, y = 0, z = 0;
            p += 3;
            x = (float)strtod(p, (char **)&p);
            y = (float)strtod(p, (char **)&p);
            z = (float)strtod(p, (char **)&p);
            if (norm_count < S3D_OBJ_MAX) {
                normals[norm_count * 3] = x;
                normals[norm_count * 3 + 1] = y;
                normals[norm_count * 3 + 2] = z;
                norm_count++;
            } else {
                fprintf(stderr, "s3d: OBJ normals exceeded limit (%d)\n", S3D_OBJ_MAX);
            }
        } else if (*p == 'f' && p + 1 < end && p[1] == ' ') {
            /* face: f v/vt/vn ... (fan-triangulate) */
            p += 2;
            int face_verts[S3D_MAX_FACE_VERTS];
            int face_count = 0;

            while (p < end && *p != '\n' && *p != '\r') {
                while (p < end && (*p == ' ' || *p == '\t')) p++;
                if (p >= end || *p == '\n' || *p == '\r') break;

                int vi = 0, ti = 0, ni = 0;
                vi = (int)strtol(p, (char **)&p, 10);
                if (p < end && *p == '/') {
                    p++;
                    if (p < end && *p != '/') { ti = (int)strtol(p, (char **)&p, 10); }
                    if (p < end && *p == '/') {
                        p++;
                        ni = (int)strtol(p, (char **)&p, 10);
                    }
                }

                /* resolve 1-based (and negative) indices */
                if (vi < 0) vi = pos_count + vi + 1;
                if (ti < 0) ti = tex_count + ti + 1;
                if (ni < 0) ni = norm_count + ni + 1;
                vi--;
                ti--;
                ni--;

                if (face_count < S3D_MAX_FACE_VERTS && vert_count < S3D_OBJ_MAX * 3) {
                    S3D_Vertex vert;
                    memset(&vert, 0, sizeof(vert));
                    if (vi >= 0 && vi < pos_count) {
                        vert.x = positions[vi * 3];
                        vert.y = positions[vi * 3 + 1];
                        vert.z = positions[vi * 3 + 2];
                    }
                    if (ti >= 0 && ti < tex_count) {
                        vert.u = texcoords[ti * 2];
                        vert.v = texcoords[ti * 2 + 1];
                    }
                    if (ni >= 0 && ni < norm_count) {
                        vert.nx = normals[ni * 3];
                        vert.ny = normals[ni * 3 + 1];
                        vert.nz = normals[ni * 3 + 2];
                    }
                    face_verts[face_count] = vert_count;
                    tmp_verts[vert_count++] = vert;
                    face_count++;
                }
            }

            /* fan-triangulate (swap i1/i2 to convert OBJ CCW → engine CW) */
            for (int i = 1; i < face_count - 1 && tri_count < S3D_OBJ_MAX * 2; i++) {
                tmp_tris[tri_count].i0 = (uint16_t)face_verts[0];
                tmp_tris[tri_count].i1 = (uint16_t)face_verts[i + 1];
                tmp_tris[tri_count].i2 = (uint16_t)face_verts[i];
                tri_count++;
            }
        }

        /* skip to end of line */
        while (p < end && *p != '\n') p++;
        if (p < end) p++; /* skip newline */
    }

    if (vert_count == 0 || tri_count == 0) return -1;

    int mesh_id = s3d_mesh_create(vert_count, tri_count);
    if (mesh_id < 0) return -1;

    /* copy vertex data — indices in tmp_tris are absolute so make them relative */
    int voff = g_engine.meshes[mesh_id].vertex_offset;
    int toff = g_engine.meshes[mesh_id].triangle_offset;
    memcpy(&g_engine.vertex_pool[voff], tmp_verts, vert_count * sizeof(S3D_Vertex));
    for (int i = 0; i < tri_count; i++) {
        g_engine.triangle_pool[toff + i].i0 = tmp_tris[i].i0;
        g_engine.triangle_pool[toff + i].i1 = tmp_tris[i].i1;
        g_engine.triangle_pool[toff + i].i2 = tmp_tris[i].i2;
    }

    return mesh_id;
}

/* ── lighting ────────────────────────────────────────────────────────── */

static inline float smoothstepf(float edge0, float edge1, float x) {
    float t = clampf((x - edge0) / (edge1 - edge0 + S3D_EPSILON), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static S3D_Vec3 compute_vertex_light(S3D_Vec3 world_pos, S3D_Vec3 world_normal, S3D_Vec3 base_color) {
    S3D_Vec3 result = s3d_vec3(0.0f, 0.0f, 0.0f);
    int any_light = 0;

    for (int i = 0; i < S3D_MAX_LIGHTS; i++) {
        S3D_Light *l = &g_engine.lights[i];
        if (l->type == S3D_LIGHT_OFF) continue;
        any_light = 1;

        if (l->type == S3D_LIGHT_AMBIENT) {
            result = v3_add(result, v3_scale(l->color, base_color));
        } else if (l->type == S3D_LIGHT_DIRECTIONAL) {
            float ndotl = v3_dot(world_normal, v3_negate(l->direction));
            if (ndotl < 0.0f) ndotl = 0.0f;
            result = v3_add(result, v3_mul(v3_scale(l->color, base_color), ndotl));
        } else if (l->type == S3D_LIGHT_POINT || l->type == S3D_LIGHT_SPOT) {
            S3D_Vec3 to_light = v3_sub(l->position, world_pos);
            float dist = v3_length(to_light);
            float att = clampf(1.0f - dist / l->range, 0.0f, 1.0f);
            if (att > 0.0f) {
                S3D_Vec3 light_dir = v3_mul(to_light, 1.0f / (dist + S3D_EPSILON));
                float ndotl = v3_dot(world_normal, light_dir);
                if (ndotl < 0.0f) ndotl = 0.0f;
                float intensity = ndotl * att;
                if (l->type == S3D_LIGHT_SPOT) {
                    float cos_angle = v3_dot(v3_negate(l->direction), light_dir);
                    float cos_outer = cosf(deg_to_rad(l->outer_angle));
                    float cos_inner = cosf(deg_to_rad(l->inner_angle));
                    intensity *= smoothstepf(cos_outer, cos_inner, cos_angle);
                }
                result = v3_add(result, v3_mul(v3_scale(l->color, base_color), intensity));
            }
        }
    }

    /* no lights active: pass through base color (backwards compatible) */
    if (!any_light) return base_color;

    result.x = clampf(result.x, 0.0f, 1.0f);
    result.y = clampf(result.y, 0.0f, 1.0f);
    result.z = clampf(result.z, 0.0f, 1.0f);
    return result;
}

/* ── internal object drawing ─────────────────────────────────────────── */

static void draw_object_internal(int mesh_id, int texture_id, S3D_Mat4 mvp, S3D_Mat4 model, S3D_Vec3 color, float alpha,
                                 int texmap) {
    S3D_CHECK_MESH(mesh_id);

    S3D_Mesh *mesh = &g_engine.meshes[mesh_id];

    for (int t = 0; t < mesh->triangle_count; t++) {
        S3D_Triangle *tri = &g_engine.triangle_pool[mesh->triangle_offset + t];
        S3D_Vertex *v0 = &g_engine.vertex_pool[mesh->vertex_offset + tri->i0];
        S3D_Vertex *v1 = &g_engine.vertex_pool[mesh->vertex_offset + tri->i1];
        S3D_Vertex *v2 = &g_engine.vertex_pool[mesh->vertex_offset + tri->i2];

        S3D_ClipVert cv[3];
        S3D_Vertex *verts[3] = {v0, v1, v2};
        for (int i = 0; i < 3; i++) {
            S3D_Vertex *v = verts[i];
            cv[i].clip = m4_mul_vec4(mvp, s3d_vec4(v->x, v->y, v->z, 1.0f));
            cv[i].u = v->u;
            cv[i].v = v->v;

            S3D_Vec4 wp = m4_mul_vec4(model, s3d_vec4(v->x, v->y, v->z, 1.0f));
            S3D_Vec4 wn = m4_mul_vec4(model, s3d_vec4(v->nx, v->ny, v->nz, 0.0f));
            S3D_Vec3 world_pos = s3d_vec3(wp.x, wp.y, wp.z);
            S3D_Vec3 world_normal = v3_normalize(s3d_vec3(wn.x, wn.y, wn.z));

            S3D_Vec3 lit = compute_vertex_light(world_pos, world_normal, color);
            cv[i].r = lit.x;
            cv[i].g = lit.y;
            cv[i].b = lit.z;
        }

        S3D_ClipVert clipped[7];
        int n = clip_near_plane(cv, 3, clipped);
        if (n < 3) continue;

        for (int i = 1; i < n - 1; i++) {
            S3D_ClipVert *a = &clipped[0];
            S3D_ClipVert *b = &clipped[i];
            S3D_ClipVert *c = &clipped[i + 1];

            float inv_w0 = 1.0f / a->clip.w;
            float inv_w1 = 1.0f / b->clip.w;
            float inv_w2 = 1.0f / c->clip.w;

            S3D_ScreenVert sv0 = ndc_to_screen(a->clip.x * inv_w0, a->clip.y * inv_w0, a->clip.z * inv_w0, a->r, a->g,
                                               a->b, a->u, a->v, a->clip.w);
            S3D_ScreenVert sv1 = ndc_to_screen(b->clip.x * inv_w1, b->clip.y * inv_w1, b->clip.z * inv_w1, b->r, b->g,
                                               b->b, b->u, b->v, b->clip.w);
            S3D_ScreenVert sv2 = ndc_to_screen(c->clip.x * inv_w2, c->clip.y * inv_w2, c->clip.z * inv_w2, c->r, c->g,
                                               c->b, c->u, c->v, c->clip.w);

            if (!is_front_facing(sv0, sv1, sv2)) continue;

            s3d_rasterize_triangle(sv0, sv1, sv2, texture_id, alpha, texmap);
        }
    }
}

/* ── object API ─────────────────────────────────────────────────────── */

static void rebuild_model_matrix(S3D_Object *obj) {
    S3D_Mat4 t = m4_translate(obj->position.x, obj->position.y, obj->position.z);
    S3D_Mat4 ry = m4_rotate_y(obj->rotation.y);
    S3D_Mat4 rx = m4_rotate_x(obj->rotation.x);
    S3D_Mat4 rz = m4_rotate_z(obj->rotation.z);
    S3D_Mat4 s = m4_scale(obj->scale.x, obj->scale.y, obj->scale.z);
    obj->model = m4_multiply(t, m4_multiply(ry, m4_multiply(rx, m4_multiply(rz, s))));
}

EMSCRIPTEN_KEEPALIVE
int s3d_object_create(int mesh_id, int texture_id) {
    for (int i = 0; i < S3D_MAX_OBJECTS; i++) {
        if (!g_engine.objects[i].active) {
            S3D_Object *obj = &g_engine.objects[i];
            obj->active = 1;
            obj->mesh_id = mesh_id;
            obj->texture_id = texture_id;
            obj->position = s3d_vec3(0.0f, 0.0f, 0.0f);
            obj->rotation = s3d_vec3(0.0f, 0.0f, 0.0f);
            obj->scale = s3d_vec3(1.0f, 1.0f, 1.0f);
            obj->color = s3d_vec3(1.0f, 1.0f, 1.0f);
            obj->alpha = 1.0f;
            obj->texmap = S3D_TEXMAP_AFFINE;
            obj->parent_id = -1;
            obj->world_frame = 0;
            rebuild_model_matrix(obj);
            obj->world = obj->model;
            return i;
        }
    }
    return -1;
}

EMSCRIPTEN_KEEPALIVE
void s3d_object_destroy(int object_id) {
    S3D_CHECK_OBJECT(object_id);
    for (int i = 0; i < S3D_MAX_OBJECTS; i++) {
        if (g_engine.objects[i].active && g_engine.objects[i].parent_id == object_id) {
            g_engine.objects[i].parent_id = -1;
        }
    }
    g_engine.objects[object_id].active = 0;
}

EMSCRIPTEN_KEEPALIVE
void s3d_object_position(int object_id, float x, float y, float z) {
    S3D_CHECK_OBJECT_ACTIVE(object_id);
    g_engine.objects[object_id].position = s3d_vec3(x, y, z);
    rebuild_model_matrix(&g_engine.objects[object_id]);
}

EMSCRIPTEN_KEEPALIVE
void s3d_object_rotation(int object_id, float rx, float ry, float rz) {
    S3D_CHECK_OBJECT_ACTIVE(object_id);
    g_engine.objects[object_id].rotation = s3d_vec3(rx, ry, rz);
    rebuild_model_matrix(&g_engine.objects[object_id]);
}

EMSCRIPTEN_KEEPALIVE
void s3d_object_scale(int object_id, float sx, float sy, float sz) {
    S3D_CHECK_OBJECT_ACTIVE(object_id);
    g_engine.objects[object_id].scale = s3d_vec3(sx, sy, sz);
    rebuild_model_matrix(&g_engine.objects[object_id]);
}

EMSCRIPTEN_KEEPALIVE
void s3d_object_color(int object_id, float r, float g, float b) {
    S3D_CHECK_OBJECT_ACTIVE(object_id);
    g_engine.objects[object_id].color = s3d_vec3(r, g, b);
}

EMSCRIPTEN_KEEPALIVE
void s3d_object_alpha(int object_id, float a) {
    S3D_CHECK_OBJECT_ACTIVE(object_id);
    g_engine.objects[object_id].alpha = a;
}

EMSCRIPTEN_KEEPALIVE
void s3d_object_active(int object_id, int active) {
    S3D_CHECK_OBJECT(object_id);
    g_engine.objects[object_id].active = active;
}

EMSCRIPTEN_KEEPALIVE
void s3d_object_texmap(int object_id, int mode) {
    S3D_CHECK_OBJECT_ACTIVE(object_id);
    g_engine.objects[object_id].texmap = mode;
}

EMSCRIPTEN_KEEPALIVE
void s3d_object_parent(int object_id, int parent_id) {
    S3D_CHECK_OBJECT_ACTIVE(object_id);
    if (parent_id >= 0) {
        S3D_CHECK_OBJECT_ACTIVE(parent_id);
        if (parent_id == object_id) return;
        int p = parent_id;
        while (p >= 0) {
            if (p == object_id) return;
            p = g_engine.objects[p].parent_id;
        }
    }
    g_engine.objects[object_id].parent_id = parent_id;
}

/* ── light API ───────────────────────────────────────────────────────── */

EMSCRIPTEN_KEEPALIVE
void s3d_light_ambient(int light_id, float r, float g, float b) {
    S3D_CHECK_LIGHT(light_id);
    S3D_Light *l = &g_engine.lights[light_id];
    l->type = S3D_LIGHT_AMBIENT;
    l->color = s3d_vec3(r, g, b);
}

EMSCRIPTEN_KEEPALIVE
void s3d_light_directional(int light_id, float r, float g, float b, float dx, float dy, float dz) {
    S3D_CHECK_LIGHT(light_id);
    S3D_Light *l = &g_engine.lights[light_id];
    l->type = S3D_LIGHT_DIRECTIONAL;
    l->color = s3d_vec3(r, g, b);
    l->direction = v3_normalize(s3d_vec3(dx, dy, dz));
}

EMSCRIPTEN_KEEPALIVE
void s3d_light_point(int light_id, float r, float g, float b, float x, float y, float z, float range) {
    S3D_CHECK_LIGHT(light_id);
    S3D_Light *l = &g_engine.lights[light_id];
    l->type = S3D_LIGHT_POINT;
    l->color = s3d_vec3(r, g, b);
    l->position = s3d_vec3(x, y, z);
    l->range = range;
}

EMSCRIPTEN_KEEPALIVE
void s3d_light_spot(int light_id, float r, float g, float b, float x, float y, float z, float dx, float dy, float dz,
                    float range, float inner_deg, float outer_deg) {
    S3D_CHECK_LIGHT(light_id);
    S3D_Light *l = &g_engine.lights[light_id];
    l->type = S3D_LIGHT_SPOT;
    l->color = s3d_vec3(r, g, b);
    l->position = s3d_vec3(x, y, z);
    l->direction = v3_normalize(s3d_vec3(dx, dy, dz));
    l->range = range;
    l->inner_angle = inner_deg;
    l->outer_angle = outer_deg;
}

EMSCRIPTEN_KEEPALIVE
void s3d_light_off(int light_id) {
    S3D_CHECK_LIGHT(light_id);
    g_engine.lights[light_id].type = S3D_LIGHT_OFF;
}

EMSCRIPTEN_KEEPALIVE
void s3d_fog_set(int enabled, float r, float g, float b, float start, float end) {
    g_engine.fog.enabled = enabled;
    g_engine.fog.r = r;
    g_engine.fog.g = g;
    g_engine.fog.b = b;
    g_engine.fog.start = start;
    g_engine.fog.end = end;
}

/* ── world matrix computation ───────────────────────────────────────── */

static uint32_t g_frame_counter = 0;

static S3D_Mat4 get_world_matrix(int object_id) {
    S3D_Object *obj = &g_engine.objects[object_id];
    if (obj->world_frame == g_frame_counter) return obj->world;
    if (obj->parent_id < 0 || !g_engine.objects[obj->parent_id].active) {
        if (obj->parent_id >= 0) obj->parent_id = -1;
        obj->world = obj->model;
    } else {
        obj->world = m4_multiply(get_world_matrix(obj->parent_id), obj->model);
    }
    obj->world_frame = g_frame_counter;
    return obj->world;
}

static void rebuild_world_matrices(void) {
    g_frame_counter++;
    for (int i = 0; i < S3D_MAX_OBJECTS; i++) {
        if (!g_engine.objects[i].active) continue;
        get_world_matrix(i);
    }
}

/* ── scene rendering ────────────────────────────────────────────────── */

EMSCRIPTEN_KEEPALIVE
void s3d_render_scene(void) {
    if (g_engine.active_camera < 0) return;
    S3D_Camera *active_cam = &g_engine.cameras[g_engine.active_camera];
    if (!active_cam->active) return;
    S3D_Mat4 vp = active_cam->vp;
    S3D_Vec3 cam_pos = active_cam->position;

    rebuild_world_matrices();

    int opaque[S3D_MAX_OBJECTS];
    int opaque_count = 0;
    int transparent[S3D_MAX_OBJECTS];
    float transparent_dist[S3D_MAX_OBJECTS];
    int transparent_count = 0;

    for (int i = 0; i < S3D_MAX_OBJECTS; i++) {
        S3D_Object *obj = &g_engine.objects[i];
        if (!obj->active) continue;
        if (obj->alpha >= 1.0f) {
            opaque[opaque_count++] = i;
        } else {
            float dx = obj->world.m[12] - cam_pos.x;
            float dy = obj->world.m[13] - cam_pos.y;
            float dz = obj->world.m[14] - cam_pos.z;
            transparent_dist[transparent_count] = dx * dx + dy * dy + dz * dz;
            transparent[transparent_count++] = i;
        }
    }

    /* render opaque objects */
    for (int i = 0; i < opaque_count; i++) {
        S3D_Object *obj = &g_engine.objects[opaque[i]];
        S3D_Mat4 mvp = m4_multiply(vp, obj->world);
        draw_object_internal(obj->mesh_id, obj->texture_id, mvp, obj->world, obj->color, obj->alpha, obj->texmap);
    }

    /* sort transparent objects back-to-front (insertion sort) */
    for (int i = 1; i < transparent_count; i++) {
        int key_idx = transparent[i];
        float key_dist = transparent_dist[i];
        int j = i - 1;
        while (j >= 0 && transparent_dist[j] < key_dist) {
            transparent[j + 1] = transparent[j];
            transparent_dist[j + 1] = transparent_dist[j];
            j--;
        }
        transparent[j + 1] = key_idx;
        transparent_dist[j + 1] = key_dist;
    }

    /* render transparent objects far-to-near */
    for (int i = 0; i < transparent_count; i++) {
        S3D_Object *obj = &g_engine.objects[transparent[i]];
        S3D_Mat4 mvp = m4_multiply(vp, obj->world);
        draw_object_internal(obj->mesh_id, obj->texture_id, mvp, obj->world, obj->color, obj->alpha, obj->texmap);
    }
}
