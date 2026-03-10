#ifndef SLOP3D_H
#define SLOP3D_H

#if defined(_MSC_VER) && (_MSC_VER < 1600)
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef signed long int32_t;
#else
#include <stdint.h>
#endif

#define S3D_WIDTH 320
#define S3D_HEIGHT 240

#define S3D_MAX_TEXTURES  64
#define S3D_MAX_MESHES    128
#define S3D_MAX_VERTICES  32768
#define S3D_MAX_TRIANGLES 65536
#define S3D_MAX_TEX_SIZE  128

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
    S3D_Vec3 r;
    r.x = x;
    r.y = y;
    r.z = z;
    return r;
}
static inline S3D_Vec4 s3d_vec4(float x, float y, float z, float w) {
    S3D_Vec4 r;
    r.x = x;
    r.y = y;
    r.z = z;
    r.w = w;
    return r;
}

typedef struct {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
} S3D_Vertex;

typedef struct {
    uint16_t i0, i1, i2;
} S3D_Triangle;

typedef struct {
    int active;
    int vertex_offset;
    int vertex_count;
    int triangle_offset;
    int triangle_count;
} S3D_Mesh;

typedef struct {
    int active;
    int width, height;
    uint8_t pixels[S3D_MAX_TEX_SIZE * S3D_MAX_TEX_SIZE * 4];
} S3D_Texture;

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

void s3d_draw_triangle(float x0, float y0, float z0, float r0, float g0, float b0,
                       float x1, float y1, float z1, float r1, float g1, float b1,
                       float x2, float y2, float z2, float r2, float g2, float b2);

int s3d_texture_create(int width, int height);
uint8_t *s3d_texture_get_data_ptr(int texture_id);

int s3d_mesh_create(int vertex_count, int triangle_count);
float *s3d_mesh_get_vertex_ptr(int mesh_id);
uint16_t *s3d_mesh_get_index_ptr(int mesh_id);
int s3d_mesh_load_obj(const char *obj_text, int len);

void s3d_draw_mesh(int mesh_id, int texture_id);

#endif
