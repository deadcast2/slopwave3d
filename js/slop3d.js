class Slop3D {
    constructor(canvasId) {
        this.canvas = document.getElementById(canvasId);
        this.ctx = this.canvas.getContext('2d');
        this.module = null;
        this._animFrameId = null;
        this._onUpdate = null;
    }

    async init() {
        this.module = await createSlop3D();

        this._init = this.module.cwrap('s3d_init', null, []);
        this._shutdown = this.module.cwrap('s3d_shutdown', null, []);
        this._clearColor = this.module.cwrap('s3d_clear_color', null, [
            'number',
            'number',
            'number',
            'number',
        ]);
        this._frameBegin = this.module.cwrap('s3d_frame_begin', null, []);
        this._getFramebuffer = this.module.cwrap(
            's3d_get_framebuffer',
            'number',
            []
        );
        this._getWidth = this.module.cwrap('s3d_get_width', 'number', []);
        this._getHeight = this.module.cwrap('s3d_get_height', 'number', []);
        this._cameraSet = this.module.cwrap('s3d_camera_set', null, [
            'number',
            'number',
            'number',
            'number',
            'number',
            'number',
            'number',
            'number',
            'number',
        ]);
        this._cameraFov = this.module.cwrap('s3d_camera_fov', null, ['number']);
        this._cameraClip = this.module.cwrap('s3d_camera_clip', null, [
            'number',
            'number',
        ]);
        this._drawTriangle = this.module.cwrap('s3d_draw_triangle', null, [
            'number',
            'number',
            'number',
            'number',
            'number',
            'number',
            'number',
            'number',
            'number',
            'number',
            'number',
            'number',
            'number',
            'number',
            'number',
            'number',
            'number',
            'number',
        ]);

        this._init();

        this.width = this._getWidth();
        this.height = this._getHeight();
        this.canvas.width = this.width;
        this.canvas.height = this.height;
        this.imageData = new ImageData(this.width, this.height);
    }

    setClearColor(r, g, b, a = 255) {
        this._clearColor(r, g, b, a);
    }

    setCamera(pos, target, up = { x: 0, y: 1, z: 0 }) {
        this._cameraSet(
            pos.x,
            pos.y,
            pos.z,
            target.x,
            target.y,
            target.z,
            up.x,
            up.y,
            up.z
        );
    }

    setCameraFov(degrees) {
        this._cameraFov(degrees);
    }

    setCameraClip(near, far) {
        this._cameraClip(near, far);
    }

    drawTriangle(v0, v1, v2) {
        this._drawTriangle(
            v0.x,
            v0.y,
            v0.z,
            v0.r,
            v0.g,
            v0.b,
            v1.x,
            v1.y,
            v1.z,
            v1.r,
            v1.g,
            v1.b,
            v2.x,
            v2.y,
            v2.z,
            v2.r,
            v2.g,
            v2.b
        );
    }

    onUpdate(callback) {
        this._onUpdate = callback;
    }

    start() {
        const render = () => {
            this._frameBegin();

            if (this._onUpdate) {
                this._onUpdate();
            }

            const fbPtr = this._getFramebuffer();
            const pixels = this.module.HEAPU8.subarray(
                fbPtr,
                fbPtr + this.width * this.height * 4
            );
            this.imageData.data.set(pixels);
            this.ctx.putImageData(this.imageData, 0, 0);

            this._animFrameId = requestAnimationFrame(render);
        };
        this._animFrameId = requestAnimationFrame(render);
    }

    stop() {
        if (this._animFrameId !== null) {
            cancelAnimationFrame(this._animFrameId);
            this._animFrameId = null;
        }
    }
}
