CC = emcc
SRC = src/slop3d.c
OUT = web/slop3d_wasm.js

EXPORTED_FUNCTIONS = \
	_s3d_init, \
	_s3d_shutdown, \
	_s3d_clear_color, \
	_s3d_frame_begin, \
	_s3d_get_framebuffer, \
	_s3d_get_width, \
	_s3d_get_height, \
	_s3d_camera_set, \
	_s3d_camera_fov, \
	_s3d_camera_clip, \
	_s3d_draw_triangle

EXPORTED_RUNTIME = ccall,cwrap,HEAPU8

all: $(OUT)

$(OUT): $(SRC) src/slop3d.h
	$(CC) $(SRC) -o $(OUT) \
		-O2 \
		-I src \
		-s WASM=1 \
		-s MODULARIZE=1 \
		-s EXPORT_NAME="createSlop3D" \
		-s ALLOW_MEMORY_GROWTH=0 \
		-s INITIAL_MEMORY=16777216 \
		-s "EXPORTED_FUNCTIONS=[$(EXPORTED_FUNCTIONS)]" \
		-s "EXPORTED_RUNTIME_METHODS=[$(EXPORTED_RUNTIME)]"

test: tests/test_math
	./tests/test_math

tests/test_math: tests/test_math.c $(SRC) src/slop3d.h
	cc -o $@ $< -I src -lm -Wall -Wextra -Wno-unused-function

clean:
	rm -f web/slop3d_wasm.js web/slop3d_wasm.wasm tests/test_math

serve: all
	python3 -m http.server 8080

fmt:
	clang-format -i src/slop3d.c src/slop3d.h tests/test_math.c
	prettier --write js/slop3d.js web/demo.js

.PHONY: all clean serve test fmt
