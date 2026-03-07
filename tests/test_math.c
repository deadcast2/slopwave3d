#include <stdio.h>
#include <math.h>
#include <string.h>

#include "slop3d.c"

static int g_pass = 0, g_fail = 0;

#define ASSERT_TRUE(expr) do { \
    if (expr) { g_pass++; } \
    else { g_fail++; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); } \
} while(0)

#define ASSERT_NEAR(a, b, eps) ASSERT_TRUE(fabsf((a) - (b)) < (eps))

#define TEST(name) static void test_##name(void)
#define RUN(name) do { printf("  %s\n", #name); test_##name(); } while(0)

static int v3_near(S3D_Vec3 a, S3D_Vec3 b, float eps) {
    return fabsf(a.x - b.x) < eps
        && fabsf(a.y - b.y) < eps
        && fabsf(a.z - b.z) < eps;
}

static int v4_near(S3D_Vec4 a, S3D_Vec4 b, float eps) {
    return fabsf(a.x - b.x) < eps
        && fabsf(a.y - b.y) < eps
        && fabsf(a.z - b.z) < eps
        && fabsf(a.w - b.w) < eps;
}

/* ── vec3 tests ──────────────────────────────────────────────────────── */

TEST(v3_add) {
    S3D_Vec3 r = v3_add((S3D_Vec3){1,2,3}, (S3D_Vec3){4,5,6});
    ASSERT_TRUE(v3_near(r, (S3D_Vec3){5,7,9}, 1e-6f));
}

TEST(v3_sub) {
    S3D_Vec3 r = v3_sub((S3D_Vec3){4,5,6}, (S3D_Vec3){1,2,3});
    ASSERT_TRUE(v3_near(r, (S3D_Vec3){3,3,3}, 1e-6f));
}

TEST(v3_mul) {
    S3D_Vec3 r = v3_mul((S3D_Vec3){1,2,3}, 2.0f);
    ASSERT_TRUE(v3_near(r, (S3D_Vec3){2,4,6}, 1e-6f));
}

TEST(v3_dot) {
    float d = v3_dot((S3D_Vec3){1,2,3}, (S3D_Vec3){4,5,6});
    ASSERT_NEAR(d, 32.0f, 1e-6f);
}

TEST(v3_cross) {
    S3D_Vec3 r = v3_cross((S3D_Vec3){1,0,0}, (S3D_Vec3){0,1,0});
    ASSERT_TRUE(v3_near(r, (S3D_Vec3){0,0,1}, 1e-6f));
}

TEST(v3_length) {
    ASSERT_NEAR(v3_length((S3D_Vec3){3,4,0}), 5.0f, 1e-6f);
}

TEST(v3_normalize) {
    S3D_Vec3 r = v3_normalize((S3D_Vec3){0,0,5});
    ASSERT_TRUE(v3_near(r, (S3D_Vec3){0,0,1}, 1e-6f));
    /* zero vector stays zero */
    S3D_Vec3 z = v3_normalize((S3D_Vec3){0,0,0});
    ASSERT_TRUE(v3_near(z, (S3D_Vec3){0,0,0}, 1e-6f));
}

TEST(v3_negate) {
    S3D_Vec3 r = v3_negate((S3D_Vec3){1,-2,3});
    ASSERT_TRUE(v3_near(r, (S3D_Vec3){-1,2,-3}, 1e-6f));
}

TEST(v3_lerp) {
    S3D_Vec3 r = v3_lerp((S3D_Vec3){0,0,0}, (S3D_Vec3){10,10,10}, 0.5f);
    ASSERT_TRUE(v3_near(r, (S3D_Vec3){5,5,5}, 1e-6f));
}

TEST(v3_scale) {
    S3D_Vec3 r = v3_scale((S3D_Vec3){2,3,4}, (S3D_Vec3){5,6,7});
    ASSERT_TRUE(v3_near(r, (S3D_Vec3){10,18,28}, 1e-6f));
}

/* ── mat4 tests ──────────────────────────────────────────────────────── */

TEST(m4_identity_multiply) {
    S3D_Mat4 id = m4_identity();
    S3D_Mat4 a = m4_translate(1, 2, 3);
    S3D_Mat4 r = m4_multiply(id, a);
    for (int i = 0; i < 16; i++)
        ASSERT_NEAR(r.m[i], a.m[i], 1e-6f);
}

TEST(m4_translate) {
    S3D_Mat4 t = m4_translate(3, 4, 5);
    S3D_Vec4 p = m4_mul_vec4(t, (S3D_Vec4){0,0,0,1});
    ASSERT_TRUE(v4_near(p, (S3D_Vec4){3,4,5,1}, 1e-6f));
}

TEST(m4_scale) {
    S3D_Mat4 s = m4_scale(2, 3, 4);
    S3D_Vec4 p = m4_mul_vec4(s, (S3D_Vec4){1,1,1,1});
    ASSERT_TRUE(v4_near(p, (S3D_Vec4){2,3,4,1}, 1e-6f));
}

TEST(m4_rotate_z_90) {
    S3D_Mat4 r = m4_rotate_z(90.0f);
    S3D_Vec4 p = m4_mul_vec4(r, (S3D_Vec4){1,0,0,1});
    ASSERT_TRUE(v4_near(p, (S3D_Vec4){0,1,0,1}, 1e-5f));
}

TEST(m4_rotate_x_90) {
    S3D_Mat4 r = m4_rotate_x(90.0f);
    S3D_Vec4 p = m4_mul_vec4(r, (S3D_Vec4){0,1,0,1});
    ASSERT_TRUE(v4_near(p, (S3D_Vec4){0,0,1,1}, 1e-5f));
}

TEST(m4_rotate_y_90) {
    S3D_Mat4 r = m4_rotate_y(90.0f);
    S3D_Vec4 p = m4_mul_vec4(r, (S3D_Vec4){0,0,1,1});
    ASSERT_TRUE(v4_near(p, (S3D_Vec4){1,0,0,1}, 1e-5f));
}

TEST(m4_inverse_affine) {
    S3D_Mat4 t = m4_translate(3, 4, 5);
    S3D_Mat4 inv = m4_inverse_affine(t);
    S3D_Mat4 result = m4_multiply(t, inv);
    S3D_Mat4 id = m4_identity();
    for (int i = 0; i < 16; i++)
        ASSERT_NEAR(result.m[i], id.m[i], 1e-5f);
}

TEST(m4_inverse_affine_rotation) {
    S3D_Mat4 r = m4_rotate_y(45.0f);
    S3D_Mat4 t = m4_translate(1, 2, 3);
    S3D_Mat4 rt = m4_multiply(t, r);
    S3D_Mat4 inv = m4_inverse_affine(rt);
    S3D_Mat4 result = m4_multiply(rt, inv);
    S3D_Mat4 id = m4_identity();
    for (int i = 0; i < 16; i++)
        ASSERT_NEAR(result.m[i], id.m[i], 1e-4f);
}

/* ── camera tests ────────────────────────────────────────────────────── */

TEST(camera_defaults) {
    s3d_init();
    S3D_Camera *cam = &g_engine.camera;
    ASSERT_TRUE(v3_near(cam->position, (S3D_Vec3){0,0,5}, 1e-6f));
    ASSERT_TRUE(v3_near(cam->target, (S3D_Vec3){0,0,0}, 1e-6f));
    ASSERT_NEAR(cam->fov, 60.0f, 1e-6f);
    ASSERT_NEAR(cam->near_clip, 0.1f, 1e-6f);
    ASSERT_NEAR(cam->far_clip, 100.0f, 1e-6f);
}

TEST(camera_origin_projects_to_center) {
    s3d_init();
    /* transform origin through VP, then perspective divide + viewport */
    S3D_Vec4 clip = m4_mul_vec4(g_engine.camera.vp, (S3D_Vec4){0,0,0,1});
    float ndc_x = clip.x / clip.w;
    float ndc_y = clip.y / clip.w;
    /* NDC (0,0) = screen center */
    ASSERT_NEAR(ndc_x, 0.0f, 1e-5f);
    ASSERT_NEAR(ndc_y, 0.0f, 1e-5f);
}

TEST(camera_right_projects_right) {
    s3d_init();
    S3D_Vec4 clip = m4_mul_vec4(g_engine.camera.vp, (S3D_Vec4){1,0,0,1});
    float ndc_x = clip.x / clip.w;
    /* point to the right in world = positive ndc_x */
    ASSERT_TRUE(ndc_x > 0.0f);
}

TEST(camera_set) {
    s3d_init();
    s3d_camera_set(10, 20, 30, 1, 2, 3, 0, 1, 0);
    ASSERT_TRUE(v3_near(g_engine.camera.position, (S3D_Vec3){10,20,30}, 1e-6f));
    ASSERT_TRUE(v3_near(g_engine.camera.target, (S3D_Vec3){1,2,3}, 1e-6f));
}

TEST(camera_fov) {
    s3d_init();
    s3d_camera_fov(90.0f);
    ASSERT_NEAR(g_engine.camera.fov, 90.0f, 1e-6f);
}

TEST(camera_clip) {
    s3d_init();
    s3d_camera_clip(1.0f, 500.0f);
    ASSERT_NEAR(g_engine.camera.near_clip, 1.0f, 1e-6f);
    ASSERT_NEAR(g_engine.camera.far_clip, 500.0f, 1e-6f);
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
    RUN(camera_defaults);
    RUN(camera_origin_projects_to_center);
    RUN(camera_right_projects_right);
    RUN(camera_set);
    RUN(camera_fov);
    RUN(camera_clip);

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
