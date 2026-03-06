#ifndef SLOP3D_H
#define SLOP3D_H

#include <stdint.h>

#define S3D_WIDTH  320
#define S3D_HEIGHT 240

void     s3d_init(void);
void     s3d_shutdown(void);
void     s3d_clear_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void     s3d_frame_begin(void);
uint8_t* s3d_get_framebuffer(void);
int      s3d_get_width(void);
int      s3d_get_height(void);

#endif
