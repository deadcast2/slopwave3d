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
        this._textureCreate = this.module.cwrap(
            's3d_texture_create',
            'number',
            ['number', 'number']
        );
        this._textureGetDataPtr = this.module.cwrap(
            's3d_texture_get_data_ptr',
            'number',
            ['number']
        );
        this._meshLoadObj = this.module.cwrap('s3d_mesh_load_obj', 'number', [
            'number',
            'number',
        ]);
        this._objectCreate = this.module.cwrap('s3d_object_create', 'number', [
            'number',
            'number',
        ]);
        this._objectDestroy = this.module.cwrap('s3d_object_destroy', null, [
            'number',
        ]);
        this._objectPosition = this.module.cwrap('s3d_object_position', null, [
            'number',
            'number',
            'number',
            'number',
        ]);
        this._objectRotation = this.module.cwrap('s3d_object_rotation', null, [
            'number',
            'number',
            'number',
            'number',
        ]);
        this._objectScale = this.module.cwrap('s3d_object_scale', null, [
            'number',
            'number',
            'number',
            'number',
        ]);
        this._objectColor = this.module.cwrap('s3d_object_color', null, [
            'number',
            'number',
            'number',
            'number',
        ]);
        this._objectAlpha = this.module.cwrap('s3d_object_alpha', null, [
            'number',
            'number',
        ]);
        this._objectActive = this.module.cwrap('s3d_object_active', null, [
            'number',
            'number',
        ]);
        this._renderScene = this.module.cwrap('s3d_render_scene', null, []);

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

    async loadTexture(url) {
        const img = new Image();
        img.crossOrigin = 'anonymous';
        await new Promise((resolve, reject) => {
            img.onload = resolve;
            img.onerror = reject;
            img.src = url;
        });
        const w = Math.min(img.width, 128);
        const h = Math.min(img.height, 128);
        const canvas = document.createElement('canvas');
        canvas.width = w;
        canvas.height = h;
        const ctx = canvas.getContext('2d');
        ctx.drawImage(img, 0, 0, w, h);
        const rgba = ctx.getImageData(0, 0, w, h).data;

        const texId = this._textureCreate(w, h);
        if (texId < 0) throw new Error('No free texture slots');
        const ptr = this._textureGetDataPtr(texId);
        this.module.HEAPU8.set(rgba, ptr);
        return texId;
    }

    async loadOBJ(url) {
        const resp = await fetch(url);
        const text = await resp.text();
        const encoder = new TextEncoder();
        const bytes = encoder.encode(text);
        const ptr = this.module._malloc(bytes.length + 1);
        this.module.HEAPU8.set(bytes, ptr);
        this.module.HEAPU8[ptr + bytes.length] = 0;
        const meshId = this._meshLoadObj(ptr, bytes.length);
        this.module._free(ptr);
        if (meshId < 0) throw new Error('Failed to parse OBJ');
        return meshId;
    }

    createObject(meshId, textureId = -1) {
        return this._objectCreate(meshId, textureId);
    }

    destroyObject(objectId) {
        this._objectDestroy(objectId);
    }

    setObjectPosition(id, x, y, z) {
        this._objectPosition(id, x, y, z);
    }

    setObjectRotation(id, rx, ry, rz) {
        this._objectRotation(id, rx, ry, rz);
    }

    setObjectScale(id, sx, sy, sz) {
        this._objectScale(id, sx, sy, sz);
    }

    setObjectColor(id, r, g, b) {
        this._objectColor(id, r, g, b);
    }

    setObjectAlpha(id, a) {
        this._objectAlpha(id, a);
    }

    setObjectActive(id, active) {
        this._objectActive(id, active);
    }

    renderScene() {
        this._renderScene();
    }

    onUpdate(callback) {
        this._onUpdate = callback;
    }

    start() {
        this._lastTime = performance.now();
        this._frameCount = 0;
        this._fpsAccum = 0;
        this._fpsDisplay = 0;
        this._ftDisplay = 0;

        const fpsEl = document.getElementById('fps');
        const ftEl = document.getElementById('frametime');
        const frameInterval = 1000 / 30; /* ~30fps */
        this._nextFrame = this._lastTime + frameInterval;

        const render = () => {
            this._animFrameId = requestAnimationFrame(render);

            const now = performance.now();
            if (now < this._nextFrame) return;
            this._nextFrame += frameInterval;
            if (this._nextFrame < now) this._nextFrame = now + frameInterval;
            const dt = now - this._lastTime;
            this._lastTime = now;

            this._frameCount++;
            this._fpsAccum += dt;
            if (this._fpsAccum >= 500) {
                this._fpsDisplay = (this._frameCount / this._fpsAccum) * 1000;
                this._ftDisplay = this._fpsAccum / this._frameCount;
                this._frameCount = 0;
                this._fpsAccum = 0;
                if (fpsEl)
                    fpsEl.textContent = 'FPS: ' + this._fpsDisplay.toFixed(1);
                if (ftEl)
                    ftEl.textContent =
                        'Frame: ' + this._ftDisplay.toFixed(2) + ' ms';
            }

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
