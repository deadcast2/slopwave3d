#ifndef SLOP3D_H
#define SLOP3D_H

#if defined(_MSC_VER) && (_MSC_VER < 1600)
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long  uint32_t;
typedef signed long    int32_t;
#else
#include <stdint.h>
#endif

#define S3D_WIDTH 320
#define S3D_HEIGHT 240

typedef struct {
    float x, y, z;
} S3D_Vec3;
typedef struct {
    float x, y, z, w;
} S3D_Vec4;
typedef struct {
    float m[16];
} S3D_Mat4;

/* Vector constructor helpers (C99 + C++ compatible, replace compound literals) */
static inline S3D_Vec3 s3d_vec3(float x, float y, float z) {
    S3D_Vec3 r; r.x = x; r.y = y; r.z = z; return r;
}
static inline S3D_Vec4 s3d_vec4(float x, float y, float z, float w) {
    S3D_Vec4 r; r.x = x; r.y = y; r.z = z; r.w = w; return r;
}

void s3d_init(void);
void s3d_shutdown(void);
void s3d_clear_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void s3d_frame_begin(void);
uint8_t *s3d_get_framebuffer(void);
int s3d_get_width(void);
int s3d_get_height(void);

void s3d_camera_set(float px, float py, float pz, float tx, float ty, float tz, float ux,
                    float uy, float uz);
void s3d_camera_fov(float fov_degrees);
void s3d_camera_clip(float near_clip, float far_clip);

#endif
