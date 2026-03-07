#ifndef SLOP3D_H
#define SLOP3D_H

#include <stdint.h>

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
