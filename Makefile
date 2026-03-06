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
	_s3d_get_height

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

clean:
	rm -f web/slop3d_wasm.js web/slop3d_wasm.wasm

serve: all
	python3 -m http.server 8080

.PHONY: all clean serve
