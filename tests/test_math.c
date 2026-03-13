#include <stdio.h>
#include <math.h>
#include <string.h>

#include "slop3d.c"

static int g_pass = 0, g_fail = 0;

#define ASSERT_TRUE(expr)                                                                                              \
    do {                                                                                                               \
        if (expr) {                                                                                                    \
            g_pass++;                                                                                                  \
        } else {                                                                                                       \
            g_fail++;                                                                                                  \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);                                                   \
        }                                                                                                              \
    } while (0)

#define ASSERT_NEAR(a, b, eps) ASSERT_TRUE(fabsf((a) - (b)) < (eps))

#define TEST(name) static void test_##name(void)
#define RUN(name)                                                                                                      \
    do {                                                                                                               \
        printf("  %s\n", #name);                                                                                       \
        test_##name();                                                                                                 \
    } while (0)

static int v3_near(S3D_Vec3 a, S3D_Vec3 b, float eps) {
    return fabsf(a.x - b.x) < eps && fabsf(a.y - b.y) < eps && fabsf(a.z - b.z) < eps;
}

static int v4_near(S3D_Vec4 a, S3D_Vec4 b, float eps) {
    return fabsf(a.x - b.x) < eps && fabsf(a.y - b.y) < eps && fabsf(a.z - b.z) < eps && fabsf(a.w - b.w) < eps;
}

/* ── vec3 tests ──────────────────────────────────────────────────────── */

TEST(v3_add) {
    S3D_Vec3 r = v3_add((S3D_Vec3){1, 2, 3}, (S3D_Vec3){4, 5, 6});
    ASSERT_TRUE(v3_near(r, (S3D_Vec3){5, 7, 9}, 1e-6f));
}

TEST(v3_sub) {
    S3D_Vec3 r = v3_sub((S3D_Vec3){4, 5, 6}, (S3D_Vec3){1, 2, 3});
    ASSERT_TRUE(v3_near(r, (S3D_Vec3){3, 3, 3}, 1e-6f));
}

TEST(v3_mul) {
    S3D_Vec3 r = v3_mul((S3D_Vec3){1, 2, 3}, 2.0f);
    ASSERT_TRUE(v3_near(r, (S3D_Vec3){2, 4, 6}, 1e-6f));
}

TEST(v3_dot) {
    float d = v3_dot((S3D_Vec3){1, 2, 3}, (S3D_Vec3){4, 5, 6});
    ASSERT_NEAR(d, 32.0f, 1e-6f);
}

TEST(v3_cross) {
    S3D_Vec3 r = v3_cross((S3D_Vec3){1, 0, 0}, (S3D_Vec3){0, 1, 0});
    ASSERT_TRUE(v3_near(r, (S3D_Vec3){0, 0, 1}, 1e-6f));
}

TEST(v3_length) { ASSERT_NEAR(v3_length((S3D_Vec3){3, 4, 0}), 5.0f, 1e-6f); }

TEST(v3_normalize) {
    S3D_Vec3 r = v3_normalize((S3D_Vec3){0, 0, 5});
    ASSERT_TRUE(v3_near(r, (S3D_Vec3){0, 0, 1}, 1e-6f));
    /* zero vector stays zero */
    S3D_Vec3 z = v3_normalize((S3D_Vec3){0, 0, 0});
    ASSERT_TRUE(v3_near(z, (S3D_Vec3){0, 0, 0}, 1e-6f));
}

TEST(v3_negate) {
    S3D_Vec3 r = v3_negate((S3D_Vec3){1, -2, 3});
    ASSERT_TRUE(v3_near(r, (S3D_Vec3){-1, 2, -3}, 1e-6f));
}

TEST(v3_lerp) {
    S3D_Vec3 r = v3_lerp((S3D_Vec3){0, 0, 0}, (S3D_Vec3){10, 10, 10}, 0.5f);
    ASSERT_TRUE(v3_near(r, (S3D_Vec3){5, 5, 5}, 1e-6f));
}

TEST(v3_scale) {
    S3D_Vec3 r = v3_scale((S3D_Vec3){2, 3, 4}, (S3D_Vec3){5, 6, 7});
    ASSERT_TRUE(v3_near(r, (S3D_Vec3){10, 18, 28}, 1e-6f));
}

/* ── mat4 tests ──────────────────────────────────────────────────────── */

TEST(m4_identity_multiply) {
    S3D_Mat4 id = m4_identity();
    S3D_Mat4 a = m4_translate(1, 2, 3);
    S3D_Mat4 r = m4_multiply(id, a);
    for (int i = 0; i < 16; i++) ASSERT_NEAR(r.m[i], a.m[i], 1e-6f);
}

TEST(m4_translate) {
    S3D_Mat4 t = m4_translate(3, 4, 5);
    S3D_Vec4 p = m4_mul_vec4(t, (S3D_Vec4){0, 0, 0, 1});
    ASSERT_TRUE(v4_near(p, (S3D_Vec4){3, 4, 5, 1}, 1e-6f));
}

TEST(m4_scale) {
    S3D_Mat4 s = m4_scale(2, 3, 4);
    S3D_Vec4 p = m4_mul_vec4(s, (S3D_Vec4){1, 1, 1, 1});
    ASSERT_TRUE(v4_near(p, (S3D_Vec4){2, 3, 4, 1}, 1e-6f));
}

TEST(m4_rotate_z_90) {
    S3D_Mat4 r = m4_rotate_z(90.0f);
    S3D_Vec4 p = m4_mul_vec4(r, (S3D_Vec4){1, 0, 0, 1});
    ASSERT_TRUE(v4_near(p, (S3D_Vec4){0, 1, 0, 1}, 1e-5f));
}

TEST(m4_rotate_x_90) {
    S3D_Mat4 r = m4_rotate_x(90.0f);
    S3D_Vec4 p = m4_mul_vec4(r, (S3D_Vec4){0, 1, 0, 1});
    ASSERT_TRUE(v4_near(p, (S3D_Vec4){0, 0, 1, 1}, 1e-5f));
}

TEST(m4_rotate_y_90) {
    S3D_Mat4 r = m4_rotate_y(90.0f);
    S3D_Vec4 p = m4_mul_vec4(r, (S3D_Vec4){0, 0, 1, 1});
    ASSERT_TRUE(v4_near(p, (S3D_Vec4){1, 0, 0, 1}, 1e-5f));
}

TEST(m4_inverse_affine) {
    S3D_Mat4 t = m4_translate(3, 4, 5);
    S3D_Mat4 inv = m4_inverse_affine(t);
    S3D_Mat4 result = m4_multiply(t, inv);
    S3D_Mat4 id = m4_identity();
    for (int i = 0; i < 16; i++) ASSERT_NEAR(result.m[i], id.m[i], 1e-5f);
}

TEST(m4_inverse_affine_rotation) {
    S3D_Mat4 r = m4_rotate_y(45.0f);
    S3D_Mat4 t = m4_translate(1, 2, 3);
    S3D_Mat4 rt = m4_multiply(t, r);
    S3D_Mat4 inv = m4_inverse_affine(rt);
    S3D_Mat4 result = m4_multiply(rt, inv);
    S3D_Mat4 id = m4_identity();
    for (int i = 0; i < 16; i++) ASSERT_NEAR(result.m[i], id.m[i], 1e-4f);
}

/* ── camera tests ────────────────────────────────────────────────────── */

TEST(camera_no_default) {
    s3d_init();
    ASSERT_TRUE(g_engine.active_camera == -1);
    for (int i = 0; i < S3D_MAX_CAMERAS; i++) { ASSERT_TRUE(!g_engine.cameras[i].active); }
}

TEST(camera_create) {
    s3d_init();
    int id = s3d_camera_create(0, 0, 5, 0, 0, 0);
    ASSERT_TRUE(id >= 0);
    S3D_Camera *cam = &g_engine.cameras[id];
    ASSERT_TRUE(cam->active);
    ASSERT_TRUE(v3_near(cam->position, (S3D_Vec3){0, 0, 5}, 1e-6f));
    ASSERT_TRUE(v3_near(cam->target, (S3D_Vec3){0, 0, 0}, 1e-6f));
    ASSERT_NEAR(cam->fov, 60.0f, 1e-6f);
    ASSERT_NEAR(cam->near_clip, 0.1f, 1e-6f);
    ASSERT_NEAR(cam->far_clip, 100.0f, 1e-6f);
}

TEST(camera_origin_projects_to_center) {
    s3d_init();
    int id = s3d_camera_create(0, 0, 5, 0, 0, 0);
    S3D_Vec4 clip = m4_mul_vec4(g_engine.cameras[id].vp, (S3D_Vec4){0, 0, 0, 1});
    float ndc_x = clip.x / clip.w;
    float ndc_y = clip.y / clip.w;
    ASSERT_NEAR(ndc_x, 0.0f, 1e-5f);
    ASSERT_NEAR(ndc_y, 0.0f, 1e-5f);
}

TEST(camera_right_projects_right) {
    s3d_init();
    int id = s3d_camera_create(0, 0, 5, 0, 0, 0);
    S3D_Vec4 clip = m4_mul_vec4(g_engine.cameras[id].vp, (S3D_Vec4){1, 0, 0, 1});
    float ndc_x = clip.x / clip.w;
    ASSERT_TRUE(ndc_x > 0.0f);
}

TEST(camera_activate) {
    s3d_init();
    int c0 = s3d_camera_create(0, 0, 5, 0, 0, 0);
    int c1 = s3d_camera_create(10, 0, 0, 0, 0, 0);
    s3d_camera_activate(c0);
    ASSERT_TRUE(g_engine.active_camera == c0);
    s3d_camera_activate(c1);
    ASSERT_TRUE(g_engine.active_camera == c1);
}

TEST(camera_destroy) {
    s3d_init();
    int id = s3d_camera_create(0, 0, 5, 0, 0, 0);
    s3d_camera_activate(id);
    ASSERT_TRUE(g_engine.active_camera == id);
    s3d_camera_destroy(id);
    ASSERT_TRUE(!g_engine.cameras[id].active);
    ASSERT_TRUE(g_engine.active_camera == -1);
}

TEST(camera_fov) {
    s3d_init();
    int id = s3d_camera_create(0, 0, 5, 0, 0, 0);
    s3d_camera_set_fov(id, 90.0f);
    ASSERT_NEAR(g_engine.cameras[id].fov, 90.0f, 1e-6f);
}

TEST(camera_clip) {
    s3d_init();
    int id = s3d_camera_create(0, 0, 5, 0, 0, 0);
    s3d_camera_set_clip(id, 1.0f, 500.0f);
    ASSERT_NEAR(g_engine.cameras[id].near_clip, 1.0f, 1e-6f);
    ASSERT_NEAR(g_engine.cameras[id].far_clip, 500.0f, 1e-6f);
}

/* ── viewport transform tests ────────────────────────────────────────── */

TEST(ndc_origin_to_screen_center) {
    S3D_ScreenVert sv = ndc_to_screen(0.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.0f, 0.0f, 0.0f);
    ASSERT_NEAR(sv.x, 160.0f, 0.01f);
    ASSERT_NEAR(sv.y, 120.0f, 0.01f);
    ASSERT_NEAR(sv.z, 0.5f, 1e-6f);
    ASSERT_NEAR(sv.r, 1.0f, 1e-6f);
    ASSERT_NEAR(sv.g, 0.5f, 1e-6f);
    ASSERT_NEAR(sv.b, 0.0f, 1e-6f);
}

TEST(ndc_corners) {
    /* NDC (-1, -1) = bottom-left in NDC → screen bottom-left = (0, 240) */
    S3D_ScreenVert bl = ndc_to_screen(-1.0f, -1.0f, -1.0f, 0, 0, 0, 0, 0);
    ASSERT_NEAR(bl.x, 0.0f, 0.01f);
    ASSERT_NEAR(bl.y, 240.0f, 0.01f);
    ASSERT_NEAR(bl.z, 0.0f, 1e-6f);

    /* NDC (1, 1) = top-right in NDC → screen top-right = (320, 0) */
    S3D_ScreenVert tr = ndc_to_screen(1.0f, 1.0f, 1.0f, 0, 0, 0, 0, 0);
    ASSERT_NEAR(tr.x, 320.0f, 0.01f);
    ASSERT_NEAR(tr.y, 0.0f, 0.01f);
    ASSERT_NEAR(tr.z, 1.0f, 1e-6f);
}

/* ── backface cull tests ─────────────────────────────────────────────── */

TEST(backface_cw_is_front) {
    /* CW in screen space (Y-down): top, bottom-right, bottom-left */
    S3D_ScreenVert a = {160, 10, 0.5f, 0, 0, 1, 0, 0};
    S3D_ScreenVert b = {200, 200, 0.5f, 0, 0, 0, 1, 0};
    S3D_ScreenVert c = {120, 200, 0.5f, 0, 0, 0, 0, 1};
    ASSERT_TRUE(is_front_facing(a, b, c));
}

TEST(backface_ccw_is_back) {
    /* CCW in screen space: top, bottom-left, bottom-right */
    S3D_ScreenVert a = {160, 10, 0.5f, 0, 0, 1, 0, 0};
    S3D_ScreenVert b = {120, 200, 0.5f, 0, 0, 0, 0, 1};
    S3D_ScreenVert c = {200, 200, 0.5f, 0, 0, 0, 1, 0};
    ASSERT_TRUE(!is_front_facing(a, b, c));
}

/* ── near-plane clipping tests ───────────────────────────────────────── */

TEST(clip_all_inside) {
    S3D_ClipVert in[3] = {
        {{0, 0, 0, 1}, 1, 0, 0, 0, 0},
        {{1, 0, 0, 1}, 0, 1, 0, 0, 0},
        {{0, 1, 0, 1}, 0, 0, 1, 0, 0},
    };
    S3D_ClipVert out[7];
    int n = clip_near_plane(in, 3, out);
    ASSERT_TRUE(n == 3);
}

TEST(clip_all_behind) {
    /* w + z < 0 for all: z = -2, w = 1 → -2 + 1 = -1 < 0 */
    S3D_ClipVert in[3] = {
        {{0, 0, -2, 1}, 1, 0, 0, 0, 0},
        {{1, 0, -2, 1}, 0, 1, 0, 0, 0},
        {{0, 1, -2, 1}, 0, 0, 1, 0, 0},
    };
    S3D_ClipVert out[7];
    int n = clip_near_plane(in, 3, out);
    ASSERT_TRUE(n == 0);
}

TEST(clip_one_behind) {
    /* Two inside (w+z >= 0), one behind → produces 4 verts (quad) */
    S3D_ClipVert in[3] = {
        {{0, 0, 0, 1}, 1, 0, 0, 0, 0},  /* w+z = 1, inside */
        {{1, 0, 0, 1}, 0, 1, 0, 0, 0},  /* w+z = 1, inside */
        {{0, 0, -2, 1}, 0, 0, 1, 0, 0}, /* w+z = -1, behind */
    };
    S3D_ClipVert out[7];
    int n = clip_near_plane(in, 3, out);
    ASSERT_TRUE(n == 4);
}

TEST(clip_two_behind) {
    /* One inside, two behind → produces 3 verts (triangle) */
    S3D_ClipVert in[3] = {
        {{0, 0, 0, 1}, 1, 0, 0, 0, 0},  /* w+z = 1, inside */
        {{1, 0, -2, 1}, 0, 1, 0, 0, 0}, /* w+z = -1, behind */
        {{0, 0, -2, 1}, 0, 0, 1, 0, 0}, /* w+z = -1, behind */
    };
    S3D_ClipVert out[7];
    int n = clip_near_plane(in, 3, out);
    ASSERT_TRUE(n == 3);
}

/* ── rasterizer integration tests ────────────────────────────────────── */

/* helper: create a simple 1-triangle mesh for tests */
static int make_test_tri_mesh(float x0, float y0, float z0, float x1, float y1, float z1, float x2, float y2,
                              float z2) {
    int mid = s3d_mesh_create(3, 1);
    if (mid < 0) return -1;
    float *vp = s3d_mesh_get_vertex_ptr(mid);
    /* vertex 0 */
    vp[0] = x0;
    vp[1] = y0;
    vp[2] = z0;
    vp[3] = 0;
    vp[4] = 0;
    vp[5] = 1; /* normal */
    vp[6] = 0;
    vp[7] = 0; /* uv */
    /* vertex 1 */
    vp[8] = x1;
    vp[9] = y1;
    vp[10] = z1;
    vp[11] = 0;
    vp[12] = 0;
    vp[13] = 1;
    vp[14] = 0;
    vp[15] = 0;
    /* vertex 2 */
    vp[16] = x2;
    vp[17] = y2;
    vp[18] = z2;
    vp[19] = 0;
    vp[20] = 0;
    vp[21] = 1;
    vp[22] = 0;
    vp[23] = 0;
    uint16_t *ip = s3d_mesh_get_index_ptr(mid);
    ip[0] = 0;
    ip[1] = 1;
    ip[2] = 2;
    return mid;
}

TEST(rasterize_fills_pixels) {
    s3d_init();
    int cid = s3d_camera_create(0, 0, 5, 0, 0, 0);
    s3d_camera_activate(cid);
    s3d_frame_begin();

    int mid = make_test_tri_mesh(0, 1, 0, 1, -1, 0, -1, -1, 0);
    int oid = s3d_object_create(mid, -1);
    s3d_object_color(oid, 1, 0, 0);
    s3d_render_scene();

    uint32_t *fb = (uint32_t *)g_engine.framebuffer;
    uint32_t clear = (uint32_t)g_engine.clear_r | ((uint32_t)g_engine.clear_g << 8) |
                     ((uint32_t)g_engine.clear_b << 16) | ((uint32_t)g_engine.clear_a << 24);
    uint32_t center = fb[120 * S3D_WIDTH + 160];
    ASSERT_TRUE(center != clear);
}

TEST(rasterize_zbuffer_occlusion) {
    s3d_init();
    int cid = s3d_camera_create(0, 0, 5, 0, 0, 0);
    s3d_camera_activate(cid);
    s3d_frame_begin();

    /* front object: red, at z=0 */
    int m1 = make_test_tri_mesh(0, 1, 0, 1, -1, 0, -1, -1, 0);
    int o1 = s3d_object_create(m1, -1);
    s3d_object_color(o1, 1, 0, 0);

    /* back object: blue, at z=-2 */
    int m2 = make_test_tri_mesh(0, 1, -2, 1, -1, -2, -1, -1, -2);
    int o2 = s3d_object_create(m2, -1);
    s3d_object_color(o2, 0, 0, 1);

    s3d_render_scene();

    uint32_t *fb = (uint32_t *)g_engine.framebuffer;
    uint32_t center = fb[120 * S3D_WIDTH + 160];
    uint8_t r = center & 0xFF;
    uint8_t b = (center >> 16) & 0xFF;
    ASSERT_TRUE(r > b);
}

/* ── main ────────────────────────────────────────────────────────────── */

int main(void) {
    printf("vec3:\n");
    RUN(v3_add);
    RUN(v3_sub);
    RUN(v3_mul);
    RUN(v3_dot);
    RUN(v3_cross);
    RUN(v3_length);
    RUN(v3_normalize);
    RUN(v3_negate);
    RUN(v3_lerp);
    RUN(v3_scale);

    printf("mat4:\n");
    RUN(m4_identity_multiply);
    RUN(m4_translate);
    RUN(m4_scale);
    RUN(m4_rotate_z_90);
    RUN(m4_rotate_x_90);
    RUN(m4_rotate_y_90);
    RUN(m4_inverse_affine);
    RUN(m4_inverse_affine_rotation);

    printf("camera:\n");
    RUN(camera_no_default);
    RUN(camera_create);
    RUN(camera_origin_projects_to_center);
    RUN(camera_right_projects_right);
    RUN(camera_activate);
    RUN(camera_destroy);
    RUN(camera_fov);
    RUN(camera_clip);

    printf("viewport:\n");
    RUN(ndc_origin_to_screen_center);
    RUN(ndc_corners);

    printf("backface:\n");
    RUN(backface_cw_is_front);
    RUN(backface_ccw_is_back);

    printf("clipping:\n");
    RUN(clip_all_inside);
    RUN(clip_all_behind);
    RUN(clip_one_behind);
    RUN(clip_two_behind);

    printf("rasterizer:\n");
    RUN(rasterize_fills_pixels);
    RUN(rasterize_zbuffer_occlusion);

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
