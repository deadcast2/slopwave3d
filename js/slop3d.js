// ============================================================
// SlopScript — DSL transpiler + runtime for slop3d
// ============================================================

// --- Constants ---

const MAX_TEXTURE_SIZE = 128;
const TARGET_FPS = 30;
const FPS_SAMPLE_MS = 500;

// --- Runtime Helpers ---

const _DEG = Math.PI / 180;
function _sin(d) {
    return Math.sin(d * _DEG);
}
function _cos(d) {
    return Math.cos(d * _DEG);
}
function _tan(d) {
    return Math.tan(d * _DEG);
}
function _lerp(a, b, t) {
    return a + (b - a) * t;
}
function _clamp(v, lo, hi) {
    return Math.min(Math.max(v, lo), hi);
}
function _random() {
    return Math.random();
}
function _abs(v) {
    return Math.abs(v);
}
function _min() {
    return Math.min.apply(null, arguments);
}
function _max() {
    return Math.max.apply(null, arguments);
}
function _range(n) {
    const a = [];
    for (let i = 0; i < n; i++) a.push(i);
    return a;
}

const _KEY_MAP = {
    left: 37,
    right: 39,
    up: 38,
    down: 40,
    space: 32,
    enter: 13,
    escape: 27,
    tab: 9,
    shift: 16,
    ctrl: 17,
    alt: 18,
    a: 65,
    b: 66,
    c: 67,
    d: 68,
    e: 69,
    f: 70,
    g: 71,
    h: 72,
    i: 73,
    j: 74,
    k: 75,
    l: 76,
    m: 77,
    n: 78,
    o: 79,
    p: 80,
    q: 81,
    r: 82,
    s: 83,
    t: 84,
    u: 85,
    v: 86,
    w: 87,
    x: 88,
    y: 89,
    z: 90,
    0: 48,
    1: 49,
    2: 50,
    3: 51,
    4: 52,
    5: 53,
    6: 54,
    7: 55,
    8: 56,
    9: 57,
};
let _key_down_rt = null;
function _key_down(name) {
    if (!_key_down_rt) return false;
    const code = _KEY_MAP[name];
    return code !== undefined && _key_down_rt.isKeyDown(code);
}

class SlopVec3 {
    constructor(onChange, x, y, z) {
        this._x = x || 0;
        this._y = y || 0;
        this._z = z || 0;
        this._cb = onChange;
    }
    get x() {
        return this._x;
    }
    set x(v) {
        this._x = v;
        this._cb(this);
    }
    get y() {
        return this._y;
    }
    set y(v) {
        this._y = v;
        this._cb(this);
    }
    get z() {
        return this._z;
    }
    set z(v) {
        this._z = v;
        this._cb(this);
    }
    setAll(x, y, z) {
        this._x = x;
        this._y = y;
        this._z = z;
        this._cb(this);
    }
}

class SlopObject {
    constructor(rt, id) {
        this._rt = rt;
        this._id = id;
        this._alpha = 1;
        this._active = true;
        this._style = 0;
        this._parent = null;
        this.position = new SlopVec3(v => rt._objectPosition(id, v._x, v._y, v._z));
        this.rotation = new SlopVec3(v => rt._objectRotation(id, v._x, v._y, v._z));
        this.scale = new SlopVec3(v => rt._objectScale(id, v._x, v._y, v._z), 1, 1, 1);
        this.color = new SlopVec3(v => rt._objectColor(id, v._x, v._y, v._z), 1, 1, 1);
    }
    get id() {
        return this._id;
    }
    get alpha() {
        return this._alpha;
    }
    set alpha(v) {
        this._alpha = v;
        this._rt._objectAlpha(this._id, v);
    }
    get active() {
        return this._active;
    }
    set active(v) {
        this._active = v;
        this._rt._objectActive(this._id, v ? 1 : 0);
    }
    get style() {
        return this._style;
    }
    set style(v) {
        this._style = v;
        this._rt._objectTexmap(this._id, v);
    }
    get dad() {
        return this._parent;
    }
    set dad(v) {
        this._parent = v;
        this._rt._objectParent(this._id, v ? v._id : -1);
    }
    get mom() {
        return this._parent;
    }
    set mom(v) {
        this.dad = v;
    }
}

class SlopTileGrid {
    constructor(rt, obj, meshId, cols, rows) {
        this._rt = rt;
        this._obj = obj;
        this._meshId = meshId;
        this.cols = cols;
        this.rows = rows;
        this._heights = new Float32Array((cols + 1) * (rows + 1));
        this.hills = new SlopVec3(v => this._applyHills(v._x, v._y), 0, 0, 0);
    }
    get id() {
        return this._obj.id;
    }
    get _id() {
        return this._obj._id;
    }
    get position() {
        return this._obj.position;
    }
    get rotation() {
        return this._obj.rotation;
    }
    get scale() {
        return this._obj.scale;
    }
    get active() {
        return this._obj.active;
    }
    set active(v) {
        this._obj.active = v;
    }
    get style() {
        return this._obj.style;
    }
    set style(v) {
        this._obj.style = v;
    }
    get color() {
        return this._obj.color;
    }
    get alpha() {
        return this._obj.alpha;
    }
    set alpha(v) {
        this._obj.alpha = v;
    }
    _hash(x, z, seed) {
        let h = (seed * 374761393 + x * 668265263 + z * 1274126177) | 0;
        h = ((h ^ (h >> 13)) * 1103515245) | 0;
        return ((h ^ (h >> 16)) & 0x7fffffff) / 0x7fffffff;
    }
    _noise(fx, fz, seed) {
        const x0 = Math.floor(fx),
            z0 = Math.floor(fz);
        const x1 = x0 + 1,
            z1 = z0 + 1;
        const sx = fx - x0,
            sz = fz - z0;
        const n00 = this._hash(x0, z0, seed);
        const n10 = this._hash(x1, z0, seed);
        const n01 = this._hash(x0, z1, seed);
        const n11 = this._hash(x1, z1, seed);
        const nx0 = n00 + (n10 - n00) * sx;
        const nx1 = n01 + (n11 - n01) * sx;
        return nx0 + (nx1 - nx0) * sz;
    }
    _applyHills(seed, height) {
        if (!height) return;
        const vCols = this.cols + 1,
            vRows = this.rows + 1;
        const halfW = this.cols / 2,
            halfD = this.rows / 2;
        for (let r = 0; r < vRows; r++) {
            for (let c = 0; c < vCols; c++) {
                this._heights[r * vCols + c] = this._noise(c * 0.3, r * 0.3, seed) * height;
            }
        }
        this._writeVertices();
    }
    _writeVertices() {
        const vCols = this.cols + 1,
            vRows = this.rows + 1;
        const halfW = this.cols / 2,
            halfD = this.rows / 2;
        const ptr = this._rt._meshGetVertexPtr(this._meshId);
        const floats = new Float32Array(this._rt.module.HEAPU8.buffer, ptr, vCols * vRows * 8);
        for (let r = 0; r < vRows; r++) {
            for (let c = 0; c < vCols; c++) {
                const vi = r * vCols + c;
                const off = vi * 8;
                const y = this._heights[vi];
                floats[off] = c - halfW;
                floats[off + 1] = y;
                floats[off + 2] = r - halfD;
                // Compute normal from neighbors
                const yL = c > 0 ? this._heights[vi - 1] : y;
                const yR = c < this.cols ? this._heights[vi + 1] : y;
                const yD = r > 0 ? this._heights[vi - vCols] : y;
                const yU = r < this.rows ? this._heights[vi + vCols] : y;
                const nx = yL - yR;
                const nz = yD - yU;
                const ny = 2.0;
                const len = Math.sqrt(nx * nx + ny * ny + nz * nz);
                floats[off + 3] = nx / len;
                floats[off + 4] = ny / len;
                floats[off + 5] = nz / len;
                // UVs stay the same (set once in tile())
            }
        }
    }
}

class SlopLight {
    constructor(rt, id, type) {
        this._rt = rt;
        this._id = id;
        this._type = type;
        this._range = 10;
        this._innerAngle = 30;
        this._outerAngle = 45;
        this.color = new SlopVec3(() => this._flush(), 1, 1, 1);
        this.position = new SlopVec3(() => this._flush());
        this.direction = new SlopVec3(() => this._flush(), 0, -1, 0);
    }
    get id() {
        return this._id;
    }
    get range() {
        return this._range;
    }
    set range(v) {
        this._range = v;
        this._flush();
    }
    get inner_angle() {
        return this._innerAngle;
    }
    set inner_angle(v) {
        this._innerAngle = v;
        this._flush();
    }
    get outer_angle() {
        return this._outerAngle;
    }
    set outer_angle(v) {
        this._outerAngle = v;
        this._flush();
    }
    _flush() {
        const c = this.color,
            p = this.position,
            d = this.direction;
        switch (this._type) {
            case 'ambient':
                this._rt._lightAmbient(this._id, c._x, c._y, c._z);
                break;
            case 'directional':
                this._rt._lightDirectional(this._id, c._x, c._y, c._z, d._x, d._y, d._z);
                break;
            case 'point':
                this._rt._lightPoint(this._id, c._x, c._y, c._z, p._x, p._y, p._z, this._range);
                break;
            case 'spot':
                this._rt._lightSpot(
                    this._id,
                    c._x,
                    c._y,
                    c._z,
                    p._x,
                    p._y,
                    p._z,
                    d._x,
                    d._y,
                    d._z,
                    this._range,
                    this._innerAngle,
                    this._outerAngle
                );
                break;
        }
    }
}

class SlopCamera {
    constructor(rt, id) {
        this._rt = rt;
        this._id = id;
        this._fov = 60;
        this._near = 0.1;
        this._far = 100;
        this._behavior = null;
        this._yaw = 0;
        this._pitch = 0;
        this._speed = 5;
        this._sensitivity = 0.15;
        this.position = new SlopVec3(v => rt._cameraPos(id, v._x, v._y, v._z));
        this.target = new SlopVec3(v => rt._cameraTarget(id, v._x, v._y, v._z));
    }
    get id() {
        return this._id;
    }
    get fov() {
        return this._fov;
    }
    set fov(v) {
        this._fov = v;
        this._rt._cameraSetFov(this._id, v);
    }
    get near() {
        return this._near;
    }
    set near(v) {
        this._near = v;
        this._rt._cameraSetClip(this._id, this._near, this._far);
    }
    get far() {
        return this._far;
    }
    set far(v) {
        this._far = v;
        this._rt._cameraSetClip(this._id, this._near, this._far);
    }
    get speed() {
        return this._speed;
    }
    set speed(v) {
        this._speed = v;
    }
    get sensitivity() {
        return this._sensitivity;
    }
    set sensitivity(v) {
        this._sensitivity = v;
    }
    get act_as() {
        return this._behavior;
    }
    set act_as(v) {
        this._rt._attachBehavior(this, v);
    }
}

class SlopRuntime {
    constructor(canvasId) {
        this.canvas = document.getElementById(canvasId);
        this.ctx = this.canvas.getContext('2d');
        this.module = null;
        this._animFrameId = null;
        this.assets = { meshes: {}, textures: {} };
        this.scenes = {};
        this._activeScene = null;
        this._sceneObjects = [];
        this._sceneLights = [];
        this._sceneCameras = [];
        this._behaviorCameras = [];
        this._scope = null;
        this.t = 0;
        this.dt = 0;
        this._startTime = 0;
        this._lastTime = 0;
    }

    async init() {
        this.module = await createSlop3D();

        this._init = this.module.cwrap('s3d_init', null, []);
        this._shutdown = this.module.cwrap('s3d_shutdown', null, []);
        this._clearColor = this.module.cwrap('s3d_clear_color', null, ['number', 'number', 'number', 'number']);
        this._frameBegin = this.module.cwrap('s3d_frame_begin', null, []);
        this._getFramebuffer = this.module.cwrap('s3d_get_framebuffer', 'number', []);
        this._getWidth = this.module.cwrap('s3d_get_width', 'number', []);
        this._getHeight = this.module.cwrap('s3d_get_height', 'number', []);
        this._cameraCreate = this.module.cwrap('s3d_camera_create', 'number', [
            'number',
            'number',
            'number',
            'number',
            'number',
            'number',
        ]);
        this._cameraDestroy = this.module.cwrap('s3d_camera_destroy', null, ['number']);
        this._cameraPos = this.module.cwrap('s3d_camera_pos', null, ['number', 'number', 'number', 'number']);
        this._cameraTarget = this.module.cwrap('s3d_camera_target', null, ['number', 'number', 'number', 'number']);
        this._cameraSetFov = this.module.cwrap('s3d_camera_set_fov', null, ['number', 'number']);
        this._cameraSetClip = this.module.cwrap('s3d_camera_set_clip', null, ['number', 'number', 'number']);
        this._cameraActivate = this.module.cwrap('s3d_camera_activate', null, ['number']);
        this._cameraOff = this.module.cwrap('s3d_camera_off', null, ['number']);
        this._textureCreate = this.module.cwrap('s3d_texture_create', 'number', ['number', 'number']);
        this._textureGetDataPtr = this.module.cwrap('s3d_texture_get_data_ptr', 'number', ['number']);
        this._meshLoadObj = this.module.cwrap('s3d_mesh_load_obj', 'number', ['number', 'number']);
        this._meshCreate = this.module.cwrap('s3d_mesh_create', 'number', ['number', 'number']);
        this._meshGetVertexPtr = this.module.cwrap('s3d_mesh_get_vertex_ptr', 'number', ['number']);
        this._meshGetIndexPtr = this.module.cwrap('s3d_mesh_get_index_ptr', 'number', ['number']);
        this._objectCreate = this.module.cwrap('s3d_object_create', 'number', ['number', 'number']);
        this._objectDestroy = this.module.cwrap('s3d_object_destroy', null, ['number']);
        this._objectPosition = this.module.cwrap('s3d_object_position', null, ['number', 'number', 'number', 'number']);
        this._objectRotation = this.module.cwrap('s3d_object_rotation', null, ['number', 'number', 'number', 'number']);
        this._objectScale = this.module.cwrap('s3d_object_scale', null, ['number', 'number', 'number', 'number']);
        this._objectColor = this.module.cwrap('s3d_object_color', null, ['number', 'number', 'number', 'number']);
        this._objectAlpha = this.module.cwrap('s3d_object_alpha', null, ['number', 'number']);
        this._objectActive = this.module.cwrap('s3d_object_active', null, ['number', 'number']);
        this._objectTexmap = this.module.cwrap('s3d_object_texmap', null, ['number', 'number']);
        this._objectParent = this.module.cwrap('s3d_object_parent', null, ['number', 'number']);
        this._renderScene = this.module.cwrap('s3d_render_scene', null, []);
        this._lightAmbient = this.module.cwrap('s3d_light_ambient', null, ['number', 'number', 'number', 'number']);
        this._lightDirectional = this.module.cwrap('s3d_light_directional', null, [
            'number',
            'number',
            'number',
            'number',
            'number',
            'number',
            'number',
        ]);
        this._lightPoint = this.module.cwrap('s3d_light_point', null, [
            'number',
            'number',
            'number',
            'number',
            'number',
            'number',
            'number',
            'number',
        ]);
        this._lightSpot = this.module.cwrap('s3d_light_spot', null, [
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
        this._lightOff = this.module.cwrap('s3d_light_off', null, ['number']);
        this._fogSet = this.module.cwrap('s3d_fog_set', null, [
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

        this._keys = new Uint8Array(256);
        this._mouseX = 0;
        this._mouseY = 0;
        this._mouseButtons = 0;
        this._mouseDeltaX = 0;
        this._mouseDeltaY = 0;
        this._pointerLockSetup = false;
        document.addEventListener('keydown', e => {
            if (e.keyCode < 256) this._keys[e.keyCode] = 1;
            if (document.pointerLockElement === this.canvas) e.preventDefault();
        });
        document.addEventListener('keyup', e => {
            if (e.keyCode < 256) this._keys[e.keyCode] = 0;
            if (document.pointerLockElement === this.canvas) e.preventDefault();
        });
        this.canvas.addEventListener('mousemove', e => {
            if (document.pointerLockElement === this.canvas) {
                this._mouseDeltaX += e.movementX;
                this._mouseDeltaY += e.movementY;
            }
            const rect = this.canvas.getBoundingClientRect();
            this._mouseX = Math.round(((e.clientX - rect.left) / rect.width) * this.width);
            this._mouseY = Math.round(((e.clientY - rect.top) / rect.height) * this.height);
        });
        this.canvas.addEventListener('mousedown', e => {
            this._mouseButtons |= 1 << e.button;
        });
        this.canvas.addEventListener('mouseup', e => {
            this._mouseButtons &= ~(1 << e.button);
        });
    }

    isKeyDown(keyCode) {
        return keyCode >= 0 && keyCode < 256 && this._keys[keyCode] === 1;
    }

    async loadTexture(url) {
        const img = new Image();
        img.crossOrigin = 'anonymous';
        await new Promise((resolve, reject) => {
            img.onload = resolve;
            img.onerror = reject;
            img.src = url;
        });
        const w = Math.min(img.width, MAX_TEXTURE_SIZE);
        const h = Math.min(img.height, MAX_TEXTURE_SIZE);
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

    spawn(meshName, texName) {
        const mid = this.assets.meshes[meshName];
        const tid = texName ? this.assets.textures[texName] : -1;
        const oid = this._objectCreate(mid, tid);
        const obj = new SlopObject(this, oid);
        this._sceneObjects.push(obj);
        return obj;
    }
    terrain(texName, cols, rows, cx, cy, cz) {
        cx = cx || 0;
        cy = cy || 0;
        cz = cz || 0;
        const tid = texName ? this.assets.textures[texName] : -1;
        const vCols = cols + 1,
            vRows = rows + 1;
        const vertCount = vCols * vRows;
        const triCount = cols * rows * 2;
        const meshId = this._meshCreate(vertCount, triCount);
        if (meshId < 0) throw new Error('No free mesh slots for terrain');
        const halfW = cols / 2,
            halfD = rows / 2;
        // Write vertices: position + normal + UV (8 floats per vertex)
        const vPtr = this._meshGetVertexPtr(meshId);
        const vFloats = new Float32Array(this.module.HEAPU8.buffer, vPtr, vertCount * 8);
        for (let r = 0; r < vRows; r++) {
            for (let c = 0; c < vCols; c++) {
                const off = (r * vCols + c) * 8;
                vFloats[off] = c - halfW; // x
                vFloats[off + 1] = 0; // y
                vFloats[off + 2] = r - halfD; // z
                vFloats[off + 3] = 0; // nx
                vFloats[off + 4] = 1; // ny
                vFloats[off + 5] = 0; // nz
                vFloats[off + 6] = c % 2 === 0 ? 0 : 1; // u — tile UV per cell
                vFloats[off + 7] = r % 2 === 0 ? 0 : 1; // v — tile UV per cell
            }
        }
        // Write triangle indices (2 tris per cell, CW winding)
        const iPtr = this._meshGetIndexPtr(meshId);
        const indices = new Uint16Array(this.module.HEAPU8.buffer, iPtr, triCount * 3);
        let idx = 0;
        for (let r = 0; r < rows; r++) {
            for (let c = 0; c < cols; c++) {
                const tl = r * vCols + c;
                const tr = tl + 1;
                const bl = tl + vCols;
                const br = bl + 1;
                // Triangle 1: tl, tr, bl (CW from above)
                indices[idx++] = tl;
                indices[idx++] = tr;
                indices[idx++] = bl;
                // Triangle 2: tr, br, bl (CW from above)
                indices[idx++] = tr;
                indices[idx++] = br;
                indices[idx++] = bl;
            }
        }
        const oid = this._objectCreate(meshId, tid);
        const obj = new SlopObject(this, oid);
        obj.position.setAll(cx, cy, cz);
        this._sceneObjects.push(obj);
        return new SlopTileGrid(this, obj, meshId, cols, rows);
    }
    kill(target) {
        if (target instanceof SlopCamera) {
            this._cameraDestroy(target.id);
            this._sceneCameras[target.id] = null;
        } else if (target instanceof SlopTileGrid) {
            this._objectDestroy(target._obj._id);
            const i = this._sceneObjects.indexOf(target._obj);
            if (i >= 0) this._sceneObjects.splice(i, 1);
        } else {
            for (const obj of this._sceneObjects) {
                if (obj._parent === target) obj._parent = null;
            }
            this._objectDestroy(target.id);
            const i = this._sceneObjects.indexOf(target);
            if (i >= 0) this._sceneObjects.splice(i, 1);
        }
    }
    light(type, ...args) {
        let id = -1;
        for (let i = 0; i < 8; i++) {
            if (!this._sceneLights[i]) {
                id = i;
                break;
            }
        }
        if (id < 0) throw new Error('No free light slots');
        const l = new SlopLight(this, id, type);
        if (type === 'ambient') {
            if (args.length >= 3) l.color.setAll(args[0], args[1], args[2]);
        } else if (type === 'directional') {
            if (args.length >= 3) l.color.setAll(args[0], args[1], args[2]);
            if (args.length >= 6) l.direction.setAll(args[3], args[4], args[5]);
        } else if (type === 'point') {
            if (args.length >= 3) l.color.setAll(args[0], args[1], args[2]);
            if (args.length >= 6) l.position.setAll(args[3], args[4], args[5]);
            if (args.length >= 7) l._range = args[6];
            l._flush();
        } else if (type === 'spot') {
            if (args.length >= 3) l.color.setAll(args[0], args[1], args[2]);
            if (args.length >= 6) l.position.setAll(args[3], args[4], args[5]);
            if (args.length >= 9) l.direction.setAll(args[6], args[7], args[8]);
            if (args.length >= 10) l._range = args[9];
            if (args.length >= 11) l._innerAngle = args[10];
            if (args.length >= 12) l._outerAngle = args[11];
            l._flush();
        }
        this._sceneLights[id] = l;
        return l;
    }
    camera(...args) {
        const px = args[0],
            py = args[1],
            pz = args[2];
        const tx = args.length >= 6 ? args[3] : 0,
            ty = args.length >= 6 ? args[4] : 0,
            tz = args.length >= 6 ? args[5] : 0;
        const id = this._cameraCreate(px, py, pz, tx, ty, tz);
        if (id < 0) throw new Error('No free camera slots');
        const cam = new SlopCamera(this, id);
        cam.position._x = px;
        cam.position._y = py;
        cam.position._z = pz;
        cam.target._x = tx;
        cam.target._y = ty;
        cam.target._z = tz;
        this._sceneCameras[id] = cam;
        if (this._sceneCameras.filter(Boolean).length === 1) {
            this._cameraActivate(id);
        }
        return cam;
    }
    use(target) {
        if (target instanceof SlopCamera) {
            this._cameraActivate(target.id);
        }
    }
    off(target) {
        if (target instanceof SlopCamera) {
            this._cameraOff(target.id);
        } else if (target instanceof SlopLight) {
            this._lightOff(target.id);
            this._sceneLights[target.id] = null;
        } else if (target instanceof SlopObject) {
            target.active = false;
        }
    }
    on(target) {
        if (target instanceof SlopCamera) {
            this._cameraActivate(target.id);
        } else if (target instanceof SlopLight) {
            this._sceneLights[target.id] = target;
            target._flush();
        } else if (target instanceof SlopObject) {
            target.active = true;
        }
    }
    sky(r, g, b) {
        this._clearColor(Math.round(r * 255), Math.round(g * 255), Math.round(b * 255), 255);
    }
    fog(r, g, b, start, end) {
        this._fogSet(1, r, g, b, start, end);
    }
    _setupPointerLock() {
        if (this._pointerLockSetup) return;
        this._pointerLockSetup = true;
        this.canvas.setAttribute('tabindex', '0');
        this.canvas.addEventListener('click', () => {
            this.canvas.requestPointerLock();
        });
        document.addEventListener('pointerlockchange', () => {
            if (document.pointerLockElement === this.canvas) {
                this.canvas.focus();
            }
        });
    }
    _attachBehavior(cam, type) {
        cam._behavior = type;
        if (type === 'fps') {
            const dx = cam.target._x - cam.position._x;
            const dy = cam.target._y - cam.position._y;
            const dz = cam.target._z - cam.position._z;
            cam._yaw = Math.atan2(dx, dz) * (180 / Math.PI);
            cam._pitch = Math.atan2(dy, Math.sqrt(dx * dx + dz * dz)) * (180 / Math.PI);
            this._setupPointerLock();
        }
        this._behaviorCameras.push(cam);
    }
    _updateBehaviors() {
        for (const cam of this._behaviorCameras) {
            if (!cam._behavior) continue;
            if (cam._behavior === 'fps') this._updateFPS(cam, this.dt);
        }
    }
    _updateFPS(cam, dt) {
        if (document.pointerLockElement === this.canvas) {
            cam._yaw -= this._mouseDeltaX * cam._sensitivity;
            cam._pitch -= this._mouseDeltaY * cam._sensitivity;
            cam._pitch = Math.max(-89, Math.min(89, cam._pitch));
        }
        this._mouseDeltaX = 0;
        this._mouseDeltaY = 0;

        const yawRad = (cam._yaw * Math.PI) / 180;
        const fwdX = Math.sin(yawRad);
        const fwdZ = Math.cos(yawRad);
        const rightX = fwdZ;
        const rightZ = -fwdX;

        let mx = 0,
            mz = 0,
            my = 0;
        if (this._keys[_KEY_MAP.w]) {
            mx += fwdX;
            mz += fwdZ;
        }
        if (this._keys[_KEY_MAP.s]) {
            mx -= fwdX;
            mz -= fwdZ;
        }
        if (this._keys[_KEY_MAP.d]) {
            mx -= rightX;
            mz -= rightZ;
        }
        if (this._keys[_KEY_MAP.a]) {
            mx += rightX;
            mz += rightZ;
        }
        if (this._keys[_KEY_MAP.space]) {
            my += 1;
        }
        if (this._keys[_KEY_MAP.shift]) {
            my -= 1;
        }

        const len = Math.sqrt(mx * mx + mz * mz);
        if (len > 0) {
            mx /= len;
            mz /= len;
        }

        const speed = cam._speed * dt;
        cam.position._x += mx * speed;
        cam.position._y += my * speed;
        cam.position._z += mz * speed;

        const pitchRad = (cam._pitch * Math.PI) / 180;
        const cosPitch = Math.cos(pitchRad);
        cam.target._x = cam.position._x + Math.sin(yawRad) * cosPitch;
        cam.target._y = cam.position._y + Math.sin(pitchRad);
        cam.target._z = cam.position._z + Math.cos(yawRad) * cosPitch;

        this._cameraPos(cam._id, cam.position._x, cam.position._y, cam.position._z);
        this._cameraTarget(cam._id, cam.target._x, cam.target._y, cam.target._z);
    }
    get mouse_x() {
        return this._mouseX;
    }
    get mouse_y() {
        return this._mouseY;
    }
    get mouse_left() {
        return (this._mouseButtons & 1) !== 0;
    }
    get mouse_right() {
        return (this._mouseButtons & 4) !== 0;
    }
    gotoScene(name) {
        for (const obj of this._sceneObjects) this._objectDestroy(obj.id);
        this._sceneObjects = [];
        this._behaviorCameras = [];
        for (let i = 0; i < 8; i++) {
            if (this._sceneLights[i]) {
                this._lightOff(i);
                this._sceneLights[i] = null;
            }
            if (this._sceneCameras[i]) {
                this._cameraDestroy(i);
                this._sceneCameras[i] = null;
            }
        }
        this._activeScene = name;
        const scene = this.scenes[name];
        this._scope = scene.setup();
    }
    start() {
        this._startTime = performance.now();
        this._lastTime = this._startTime;
        this.gotoScene(this._firstScene);

        this._frameCount = 0;
        this._fpsAccum = 0;
        this._fpsDisplay = 0;
        this._ftDisplay = 0;

        const fpsEl = document.getElementById('fps');
        const ftEl = document.getElementById('frametime');
        const frameInterval = 1000 / TARGET_FPS;
        this._nextFrame = this._lastTime + frameInterval;

        const render = () => {
            this._animFrameId = requestAnimationFrame(render);

            const now = performance.now();
            if (now < this._nextFrame) return;
            this._nextFrame += frameInterval;
            if (this._nextFrame < now) this._nextFrame = now + frameInterval;

            this.dt = (now - this._lastTime) / 1000;
            this.t = (now - this._startTime) / 1000;
            this._lastTime = now;

            this._frameCount++;
            this._fpsAccum += this.dt * 1000;
            if (this._fpsAccum >= FPS_SAMPLE_MS) {
                this._fpsDisplay = (this._frameCount / this._fpsAccum) * 1000;
                this._ftDisplay = this._fpsAccum / this._frameCount;
                this._frameCount = 0;
                this._fpsAccum = 0;
                if (fpsEl) fpsEl.textContent = 'FPS: ' + this._fpsDisplay.toFixed(1);
                if (ftEl) ftEl.textContent = 'Frame: ' + this._ftDisplay.toFixed(2) + ' ms';
            }

            this._frameBegin();
            this._updateBehaviors();

            const scene = this.scenes[this._activeScene];
            if (scene.update) scene.update(this._scope);
            this._renderScene();

            const fbPtr = this._getFramebuffer();
            const pixels = this.module.HEAPU8.subarray(fbPtr, fbPtr + this.width * this.height * 4);
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

// --- Lexer ---

const TK = {
    INDENT: 'INDENT',
    DEDENT: 'DEDENT',
    NEWLINE: 'NL',
    EOF: 'EOF',
    NUMBER: 'NUM',
    IDENT: 'ID',
    LBRACKET: '[',
    RBRACKET: ']',
    COLON: ':',
    COMMA: ',',
    DOT: '.',
    EQ: '=',
    PLUS: '+',
    MINUS: '-',
    STAR: '*',
    SLASH: '/',
    PERCENT: '%',
    EQEQ: '==',
    NEQ: '!=',
    LT: '<',
    GT: '>',
    LTE: '<=',
    GTE: '>=',
};

const KEYWORDS = new Set([
    'assets',
    'model',
    'skin',
    'scene',
    'update',
    'spawn',
    'kill',
    'goto',
    'off',
    'on',
    'use',
    'sky',
    'fog',
    'camera',
    'ambient',
    'directional',
    'point',
    'spot',
    'if',
    'elif',
    'else',
    'while',
    'for',
    'in',
    'fn',
    'return',
    'and',
    'or',
    'not',
    'true',
    'false',
]);

function slopLex(src) {
    const tokens = [];
    const indentStack = [0];
    let pos = 0,
        line = 1,
        col = 1;
    const len = src.length;

    function ch() {
        return pos < len ? src[pos] : '';
    }
    function adv() {
        const c = src[pos++];
        if (c === '\n') {
            line++;
            col = 1;
        } else {
            col++;
        }
        return c;
    }
    function isIdentStart(c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c === '_';
    }
    function isIdentChar(c) {
        return isIdentStart(c) || (c >= '0' && c <= '9');
    }
    function skipComment() {
        while (pos < len && ch() !== '\n') adv();
    }
    function tok(type, val) {
        return { type, val, line, col };
    }

    while (pos < len) {
        // start of line — handle indentation
        let spaces = 0;
        while (ch() === ' ') {
            adv();
            spaces++;
        }
        // blank line or comment — skip
        if (ch() === '\n') {
            adv();
            continue;
        }
        if (ch() === '#') {
            skipComment();
            if (ch() === '\n') adv();
            continue;
        }
        if (ch() === '') break;

        // emit indent/dedent
        const top = indentStack[indentStack.length - 1];
        if (spaces > top) {
            indentStack.push(spaces);
            tokens.push(tok(TK.INDENT, spaces));
        } else if (spaces < top) {
            while (indentStack[indentStack.length - 1] > spaces) {
                indentStack.pop();
                tokens.push(tok(TK.DEDENT, spaces));
            }
            if (indentStack[indentStack.length - 1] !== spaces) throw new Error(`Bad indent at line ${line}`);
        }

        // tokenize line content
        while (pos < len && ch() !== '\n') {
            if (ch() === ' ') {
                adv();
                continue;
            }
            if (ch() === '#') {
                skipComment();
                break;
            }
            const ln = line,
                cl = col;
            // two-char ops
            if (pos + 1 < len) {
                const two = src[pos] + src[pos + 1];
                if (two === '==' || two === '!=' || two === '<=' || two === '>=') {
                    adv();
                    adv();
                    tokens.push({ type: two, val: two, line: ln, col: cl });
                    continue;
                }
            }
            // single-char tokens
            const sc = {
                '[': TK.LBRACKET,
                ']': TK.RBRACKET,
                ':': TK.COLON,
                ',': TK.COMMA,
                '.': TK.DOT,
                '=': TK.EQ,
                '+': TK.PLUS,
                '-': TK.MINUS,
                '*': TK.STAR,
                '/': TK.SLASH,
                '%': TK.PERCENT,
                '<': TK.LT,
                '>': TK.GT,
            };
            if (sc[ch()]) {
                const c = adv();
                tokens.push({ type: sc[c], val: c, line: ln, col: cl });
                continue;
            }
            // number
            if (
                (ch() >= '0' && ch() <= '9') ||
                (ch() === '.' && pos + 1 < len && src[pos + 1] >= '0' && src[pos + 1] <= '9')
            ) {
                let num = '';
                let dots = 0;
                while (pos < len && ((ch() >= '0' && ch() <= '9') || ch() === '.')) {
                    if (ch() === '.') dots++;
                    num += adv();
                }
                if (dots > 1) throw new Error(`Invalid number '${num}' at line ${ln}:${cl}`);
                tokens.push({
                    type: TK.NUMBER,
                    val: parseFloat(num),
                    line: ln,
                    col: cl,
                });
                continue;
            }
            // ident / keyword
            if (isIdentStart(ch())) {
                let id = '';
                while (pos < len && isIdentChar(ch())) id += adv();
                tokens.push({ type: TK.IDENT, val: id, line: ln, col: cl });
                continue;
            }
            throw new Error(`Unexpected char '${ch()}' at line ${line}:${col}`);
        }
        tokens.push(tok(TK.NEWLINE, '\\n'));
        if (ch() === '\n') adv();
    }
    // close remaining indents
    while (indentStack.length > 1) {
        indentStack.pop();
        tokens.push(tok(TK.DEDENT, 0));
    }
    tokens.push(tok(TK.EOF, ''));
    return tokens;
}

// --- Parser ---

const CALL_STMTS = new Set(['goto', 'kill', 'off', 'on', 'use', 'sky', 'fog']);
const LIGHT_TYPES = new Set(['ambient', 'directional', 'point', 'spot']);

function slopParse(tokens) {
    let pos = 0;
    function peek() {
        return tokens[pos];
    }
    function at(type, val) {
        const t = peek();
        return t.type === type && (val === undefined || t.val === val);
    }
    function eat(type, val) {
        const t = peek();
        if (type && t.type !== type) throw new Error(`Expected ${type} got ${t.type} (${t.val}) at line ${t.line}`);
        if (val !== undefined && t.val !== val) throw new Error(`Expected '${val}' got '${t.val}' at line ${t.line}`);
        pos++;
        return t;
    }
    function skipNL() {
        while (at(TK.NEWLINE)) pos++;
    }

    // --- Top level ---
    function parseProgram() {
        skipNL();
        let assets = null;
        const scenes = [];
        while (!at(TK.EOF)) {
            skipNL();
            if (at(TK.EOF)) break;
            if (at(TK.IDENT, 'assets')) assets = parseAssets();
            else if (at(TK.IDENT, 'scene')) scenes.push(parseScene());
            else throw new Error(`Expected 'assets' or 'scene' at line ${peek().line}`);
            skipNL();
        }
        return { type: 'Program', assets, scenes };
    }

    function parseAssets() {
        eat(TK.IDENT, 'assets');
        eat(TK.NEWLINE);
        eat(TK.INDENT);
        const models = [],
            skins = [];
        skipNL();
        while (!at(TK.DEDENT) && !at(TK.EOF)) {
            const ln = peek().line;
            if (at(TK.IDENT, 'model')) {
                eat(TK.IDENT);
                const name = eat(TK.IDENT).val;
                eat(TK.EQ);
                const path = readFilepath();
                models.push({ type: 'ModelDecl', name, path, line: ln });
            } else if (at(TK.IDENT, 'skin')) {
                eat(TK.IDENT);
                const name = eat(TK.IDENT).val;
                eat(TK.EQ);
                const path = readFilepath();
                skins.push({ type: 'SkinDecl', name, path, line: ln });
            } else throw new Error(`Expected 'model' or 'skin' at line ${peek().line}`);
            skipNL();
        }
        if (at(TK.DEDENT)) eat(TK.DEDENT);
        return { type: 'AssetBlock', models, skins };
    }

    function readFilepath() {
        let path = '';
        while (!at(TK.NEWLINE) && !at(TK.EOF)) {
            path += eat(peek().type).val;
        }
        return path;
    }

    function parseScene() {
        const ln = peek().line;
        eat(TK.IDENT, 'scene');
        const name = eat(TK.IDENT).val;
        eat(TK.NEWLINE);
        eat(TK.INDENT);
        const body = [];
        let update = null;
        skipNL();
        while (!at(TK.DEDENT) && !at(TK.EOF)) {
            if (at(TK.IDENT, 'update')) {
                update = parseUpdate();
            } else {
                body.push(parseStmt());
            }
            skipNL();
        }
        if (at(TK.DEDENT)) eat(TK.DEDENT);
        return { type: 'Scene', name, body, update, line: ln };
    }

    function parseUpdate() {
        eat(TK.IDENT, 'update');
        eat(TK.NEWLINE);
        eat(TK.INDENT);
        const stmts = [];
        skipNL();
        while (!at(TK.DEDENT) && !at(TK.EOF)) {
            stmts.push(parseStmt());
            skipNL();
        }
        if (at(TK.DEDENT)) eat(TK.DEDENT);
        return stmts;
    }

    // --- Statements ---
    function parseStmt() {
        const t = peek();
        if (at(TK.IDENT, 'if')) return parseIf();
        if (at(TK.IDENT, 'while')) return parseWhile();
        if (at(TK.IDENT, 'for')) return parseFor();
        if (at(TK.IDENT, 'fn')) return parseFn();
        if (at(TK.IDENT, 'return')) return parseReturn();
        // colon-style statement calls: goto:, destroy:, or ident:
        if (at(TK.IDENT) && CALL_STMTS.has(t.val) && tokens[pos + 1] && tokens[pos + 1].type === TK.COLON)
            return parseCallStmt();
        // assignment or expression
        return parseAssignOrExpr();
    }

    function parseCallStmt() {
        const ln = peek().line;
        const name = eat(TK.IDENT).val;
        eat(TK.COLON);
        const args = [];
        if (!at(TK.NEWLINE) && !at(TK.EOF)) {
            args.push(parseExpr());
            while (at(TK.COMMA)) {
                eat(TK.COMMA);
                args.push(parseExpr());
            }
        }
        if (at(TK.NEWLINE)) eat(TK.NEWLINE);
        return { type: 'CallStmt', name, args, line: ln };
    }

    function parseAssignOrExpr() {
        const ln = peek().line;
        const left = parseExpr();
        if (at(TK.EQ)) {
            eat(TK.EQ);
            // spawn: special case
            if (at(TK.IDENT, 'spawn')) {
                eat(TK.IDENT);
                eat(TK.COLON);
                const mesh = eat(TK.IDENT).val;
                let skin = null;
                if (at(TK.COMMA)) {
                    eat(TK.COMMA);
                    skin = eat(TK.IDENT).val;
                }
                if (at(TK.NEWLINE)) eat(TK.NEWLINE);
                return {
                    type: 'SpawnAssign',
                    target: left,
                    mesh,
                    skin,
                    line: ln,
                };
            }
            // terrain: skin, cols, rows [, cx, cy, cz]
            if (at(TK.IDENT, 'terrain')) {
                eat(TK.IDENT);
                eat(TK.COLON);
                const args = [];
                args.push(parseExpr());
                while (at(TK.COMMA)) {
                    eat(TK.COMMA);
                    args.push(parseExpr());
                }
                if (at(TK.NEWLINE)) eat(TK.NEWLINE);
                return {
                    type: 'TerrainAssign',
                    target: left,
                    args,
                    line: ln,
                };
            }
            // camera creation: camera: px, py, pz [, tx, ty, tz]
            if (at(TK.IDENT, 'camera')) {
                eat(TK.IDENT);
                eat(TK.COLON);
                const args = [];
                if (!at(TK.NEWLINE) && !at(TK.EOF)) {
                    args.push(parseExpr());
                    while (at(TK.COMMA)) {
                        eat(TK.COMMA);
                        args.push(parseExpr());
                    }
                }
                if (at(TK.NEWLINE)) eat(TK.NEWLINE);
                return {
                    type: 'CameraAssign',
                    target: left,
                    args,
                    line: ln,
                };
            }
            // light creation: ambient:, directional:, point:, spot:
            for (const lt of LIGHT_TYPES) {
                if (at(TK.IDENT, lt)) {
                    eat(TK.IDENT);
                    eat(TK.COLON);
                    const args = [];
                    if (!at(TK.NEWLINE) && !at(TK.EOF)) {
                        args.push(parseExpr());
                        while (at(TK.COMMA)) {
                            eat(TK.COMMA);
                            args.push(parseExpr());
                        }
                    }
                    if (at(TK.NEWLINE)) eat(TK.NEWLINE);
                    return {
                        type: 'LightAssign',
                        target: left,
                        lightType: lt,
                        args,
                        line: ln,
                    };
                }
            }
            const val = parseTupleOrExpr();
            return { type: 'Assign', target: left, value: val, line: ln };
        }
        // bare expression as statement (e.g. fn call with colon)
        if (at(TK.NEWLINE)) eat(TK.NEWLINE);
        return { type: 'ExprStmt', expr: left, line: ln };
    }

    function parseTupleOrExpr() {
        const first = parseExpr();
        if (at(TK.COMMA)) {
            const elems = [first];
            while (at(TK.COMMA)) {
                eat(TK.COMMA);
                elems.push(parseExpr());
            }
            if (at(TK.NEWLINE)) eat(TK.NEWLINE);
            return { type: 'Tuple', elements: elems };
        }
        if (at(TK.NEWLINE)) eat(TK.NEWLINE);
        return first;
    }

    // --- Control flow ---
    function parseIf() {
        const ln = peek().line;
        eat(TK.IDENT, 'if');
        const condition = parseExpr();
        eat(TK.NEWLINE);
        eat(TK.INDENT);
        const body = parseBlock();
        const elifs = [];
        skipNL();
        while (at(TK.IDENT, 'elif')) {
            eat(TK.IDENT);
            const c = parseExpr();
            eat(TK.NEWLINE);
            eat(TK.INDENT);
            elifs.push({ condition: c, body: parseBlock() });
            skipNL();
        }
        let elseBody = null;
        if (at(TK.IDENT, 'else')) {
            eat(TK.IDENT);
            eat(TK.NEWLINE);
            eat(TK.INDENT);
            elseBody = parseBlock();
        }
        return { type: 'If', condition, body, elifs, elseBody, line: ln };
    }

    function parseWhile() {
        const ln = peek().line;
        eat(TK.IDENT, 'while');
        const condition = parseExpr();
        eat(TK.NEWLINE);
        eat(TK.INDENT);
        return { type: 'While', condition, body: parseBlock(), line: ln };
    }

    function parseFor() {
        const ln = peek().line;
        eat(TK.IDENT, 'for');
        const varName = eat(TK.IDENT).val;
        eat(TK.IDENT, 'in');
        const iter = parseExpr();
        eat(TK.NEWLINE);
        eat(TK.INDENT);
        return { type: 'For', varName, iter, body: parseBlock(), line: ln };
    }

    function parseFn() {
        const ln = peek().line;
        eat(TK.IDENT, 'fn');
        const name = eat(TK.IDENT).val;
        eat(TK.COLON);
        const params = [eat(TK.IDENT).val];
        while (at(TK.COMMA)) {
            eat(TK.COMMA);
            params.push(eat(TK.IDENT).val);
        }
        eat(TK.NEWLINE);
        eat(TK.INDENT);
        return { type: 'FnDef', name, params, body: parseBlock(), line: ln };
    }

    function parseReturn() {
        const ln = peek().line;
        eat(TK.IDENT, 'return');
        let val = null;
        if (!at(TK.NEWLINE) && !at(TK.EOF) && !at(TK.DEDENT)) val = parseExpr();
        if (at(TK.NEWLINE)) eat(TK.NEWLINE);
        return { type: 'Return', value: val, line: ln };
    }

    function parseBlock() {
        const stmts = [];
        skipNL();
        while (!at(TK.DEDENT) && !at(TK.EOF)) {
            stmts.push(parseStmt());
            skipNL();
        }
        if (at(TK.DEDENT)) eat(TK.DEDENT);
        return stmts;
    }

    // --- Expressions (precedence climbing) ---
    const PREC = {
        or: 1,
        and: 2,
        '==': 3,
        '!=': 3,
        '<': 3,
        '>': 3,
        '<=': 3,
        '>=': 3,
        '+': 4,
        '-': 4,
        '*': 5,
        '/': 5,
        '%': 5,
    };
    const OPS = new Set(Object.keys(PREC));

    function isBinOp() {
        const t = peek();
        if (t.type === TK.IDENT && (t.val === 'and' || t.val === 'or')) return t.val;
        if (OPS.has(t.type)) return t.type;
        return null;
    }

    function parseExpr(minPrec) {
        if (minPrec === undefined) minPrec = 0;
        let left = parseUnary();
        let op;
        while ((op = isBinOp()) && PREC[op] >= minPrec) {
            eat(peek().type);
            const right = parseExpr(PREC[op] + 1);
            left = { type: 'BinOp', op, left, right };
        }
        return left;
    }

    function parseUnary() {
        if (at(TK.MINUS)) {
            eat(TK.MINUS);
            return { type: 'Unary', op: '-', operand: parseUnary() };
        }
        if (at(TK.IDENT, 'not')) {
            eat(TK.IDENT);
            return { type: 'Unary', op: 'not', operand: parseUnary() };
        }
        return parsePrimary();
    }

    function parsePrimary() {
        // number
        if (at(TK.NUMBER)) return { type: 'Num', val: eat(TK.NUMBER).val };
        // bool
        if (at(TK.IDENT, 'true')) {
            eat(TK.IDENT);
            return { type: 'Bool', val: true };
        }
        if (at(TK.IDENT, 'false')) {
            eat(TK.IDENT);
            return { type: 'Bool', val: false };
        }
        // ident, possibly with call [] or dot chain
        if (at(TK.IDENT)) {
            let node = { type: 'Ident', name: eat(TK.IDENT).val };
            // call: ident[args]
            if (at(TK.LBRACKET)) {
                eat(TK.LBRACKET);
                const args = [];
                if (!at(TK.RBRACKET)) {
                    args.push(parseExpr());
                    while (at(TK.COMMA)) {
                        eat(TK.COMMA);
                        args.push(parseExpr());
                    }
                }
                eat(TK.RBRACKET);
                node = { type: 'Call', name: node.name, args };
            }
            // dot chain
            while (at(TK.DOT)) {
                eat(TK.DOT);
                const prop = eat(TK.IDENT).val;
                node = { type: 'Dot', object: node, property: prop };
            }
            return node;
        }
        // grouping: [expr]
        if (at(TK.LBRACKET)) {
            eat(TK.LBRACKET);
            const expr = parseExpr();
            eat(TK.RBRACKET);
            return { type: 'Group', expr };
        }
        throw new Error(`Unexpected token '${peek().val}' at line ${peek().line}`);
    }

    return parseProgram();
}

// --- Code Generator ---

const SLOP_BUILTINS = new Set(['t', 'dt', 'mouse_x', 'mouse_y', 'mouse_left', 'mouse_right']);
const SLOP_MATH = new Set(['sin', 'cos', 'tan', 'lerp', 'clamp', 'random', 'abs', 'min', 'max', 'range', 'key_down']);
const SLOP_CONSTANTS = { ps1: '0', n64: '1', fps: "'fps'", none: 'null' };

function slopGenerate(ast) {
    const lines = [];
    let indent = 0;
    function emit(s) {
        lines.push('  '.repeat(indent) + s);
    }

    // assets
    if (ast.assets) {
        const loads = [];
        const names = [];
        for (const m of ast.assets.models) {
            names.push('_mesh_' + m.name);
            loads.push(`_rt.loadOBJ('assets/${m.path}')`);
        }
        for (const s of ast.assets.skins) {
            names.push('_tex_' + s.name);
            loads.push(`_rt.loadTexture('assets/${s.path}')`);
        }
        if (loads.length) {
            emit(`const [${names.join(', ')}] = await Promise.all([`);
            indent++;
            for (const l of loads) emit(l + ',');
            indent--;
            emit(']);');
            for (const m of ast.assets.models) emit(`_rt.assets.meshes['${m.name}'] = _mesh_${m.name};`);
            for (const s of ast.assets.skins) emit(`_rt.assets.textures['${s.name}'] = _tex_${s.name};`);
        }
    }

    // scenes
    let first = null;
    for (const scene of ast.scenes) {
        if (!first) first = scene.name;
        emit(`_rt.scenes['${scene.name}'] = {`);
        indent++;
        // setup
        emit('setup: function() {');
        indent++;
        emit('const _s = {};');
        for (const stmt of scene.body) emitStmt(stmt);
        emit('return _s;');
        indent--;
        emit('},');
        // update
        if (scene.update) {
            emit('update: function(_s) {');
            indent++;
            for (const stmt of scene.update) emitStmt(stmt);
            indent--;
            emit('},');
        }
        indent--;
        emit('};');
    }

    if (first) {
        emit(`_rt._firstScene = '${first}';`);
        emit('_rt.start();');
    }

    return lines.join('\n');

    // --- statement emitters ---
    function emitStmt(node) {
        switch (node.type) {
            case 'Assign':
                emitAssign(node);
                break;
            case 'SpawnAssign': {
                const tgt = emitExpr(node.target);
                const args = node.skin ? `'${node.mesh}', '${node.skin}'` : `'${node.mesh}'`;
                emit(`${tgt} = _rt.spawn(${args});`);
                break;
            }
            case 'TerrainAssign': {
                const tgt = emitExpr(node.target);
                const args = node.args.map((a, i) => {
                    if (i === 0) return `'${a.name}'`;
                    return emitExpr(a);
                });
                emit(`${tgt} = _rt.terrain(${args.join(', ')});`);
                break;
            }
            case 'CameraAssign': {
                const tgt = emitExpr(node.target);
                const args = node.args.map(emitExpr).join(', ');
                emit(`${tgt} = _rt.camera(${args});`);
                break;
            }
            case 'LightAssign': {
                const tgt = emitExpr(node.target);
                const args = node.args.map(emitExpr).join(', ');
                emit(`${tgt} = _rt.light('${node.lightType}', ${args});`);
                break;
            }
            case 'CallStmt':
                if (node.name === 'goto') {
                    emit(`_rt.gotoScene('${node.args[0].name}'); return;`);
                } else if (node.name === 'kill') {
                    emit(`_rt.kill(${emitExpr(node.args[0])});`);
                } else if (node.name === 'off') {
                    emit(`_rt.off(${emitExpr(node.args[0])});`);
                } else if (node.name === 'on') {
                    emit(`_rt.on(${emitExpr(node.args[0])});`);
                } else if (node.name === 'use') {
                    emit(`_rt.use(${emitExpr(node.args[0])});`);
                } else if (node.name === 'sky') {
                    const args = node.args.map(emitExpr).join(', ');
                    emit(`_rt.sky(${args});`);
                } else if (node.name === 'fog') {
                    const args = node.args.map(emitExpr).join(', ');
                    emit(`_rt.fog(${args});`);
                } else {
                    const args = node.args.map(emitExpr).join(', ');
                    emit(`_s.${node.name}(${args});`);
                }
                break;
            case 'If':
                emitIf(node);
                break;
            case 'While':
                emit(`while (${emitExpr(node.condition)}) {`);
                indent++;
                for (const s of node.body) emitStmt(s);
                indent--;
                emit('}');
                break;
            case 'For':
                emit(`for (const ${node.varName} of ${emitExpr(node.iter)}) {`);
                indent++;
                for (const s of node.body) emitStmt(s);
                indent--;
                emit('}');
                break;
            case 'FnDef':
                emit(`_s.${node.name} = function(${node.params.join(', ')}) {`);
                indent++;
                for (const s of node.body) emitStmt(s);
                indent--;
                emit('};');
                break;
            case 'Return':
                emit(node.value ? `return ${emitExpr(node.value)};` : 'return;');
                break;
            case 'ExprStmt':
                emit(emitExpr(node.expr) + ';');
                break;
        }
    }

    function emitAssign(node) {
        const tgt = emitExpr(node.target);
        if (node.value.type === 'Tuple') {
            emit(`${tgt}.setAll(${node.value.elements.map(emitExpr).join(', ')});`);
        } else {
            emit(`${tgt} = ${emitExpr(node.value)};`);
        }
    }

    function emitIf(node) {
        emit(`if (${emitExpr(node.condition)}) {`);
        indent++;
        for (const s of node.body) emitStmt(s);
        indent--;
        for (const elif of node.elifs) {
            emit(`} else if (${emitExpr(elif.condition)}) {`);
            indent++;
            for (const s of elif.body) emitStmt(s);
            indent--;
        }
        if (node.elseBody) {
            emit('} else {');
            indent++;
            for (const s of node.elseBody) emitStmt(s);
            indent--;
        }
        emit('}');
    }

    // --- expression emitter ---
    function emitExpr(node) {
        switch (node.type) {
            case 'Num':
                return String(node.val);
            case 'Bool':
                return String(node.val);
            case 'Ident':
                if (node.name in SLOP_CONSTANTS) return SLOP_CONSTANTS[node.name];
                if (SLOP_BUILTINS.has(node.name)) return '_rt.' + node.name;
                return '_s.' + node.name;
            case 'Dot':
                return emitExpr(node.object) + '.' + node.property;
            case 'BinOp': {
                const op = node.op === 'and' ? '&&' : node.op === 'or' ? '||' : node.op;
                return `(${emitExpr(node.left)} ${op} ${emitExpr(node.right)})`;
            }
            case 'Unary':
                return node.op === 'not' ? `(!${emitExpr(node.operand)})` : `(-${emitExpr(node.operand)})`;
            case 'Call':
                if (node.name === 'key_down') {
                    const keyArgs = node.args.map(a => (a.type === 'Ident' ? `'${a.name}'` : emitExpr(a))).join(', ');
                    return `_key_down(${keyArgs})`;
                }
                if (SLOP_MATH.has(node.name)) return `_${node.name}(${node.args.map(emitExpr).join(', ')})`;
                return `_s.${node.name}(${node.args.map(emitExpr).join(', ')})`;
            case 'Group':
                return `(${emitExpr(node.expr)})`;
            default:
                return '/* ??? */';
        }
    }
}

// --- Loader ---

class SlopScript {
    static async exec(js, rt) {
        const fn = new Function(
            '_rt',
            '_sin',
            '_cos',
            '_tan',
            '_lerp',
            '_clamp',
            '_random',
            '_abs',
            '_min',
            '_max',
            '_range',
            '_key_down',
            'return (async function(){' + js + '})();'
        );
        _key_down_rt = rt;
        await fn(rt, _sin, _cos, _tan, _lerp, _clamp, _random, _abs, _min, _max, _range, _key_down);
    }
    static async run(source, canvasId) {
        const rt = new SlopRuntime(canvasId);
        await rt.init();
        const js = slopGenerate(slopParse(slopLex(source)));
        await SlopScript.exec(js, rt);
    }
    static async load(url, canvasId) {
        const r = await fetch(url);
        await SlopScript.run(await r.text(), canvasId);
    }
    static init() {
        document.querySelectorAll('script[type="text/slopscript"]').forEach(el => {
            const canvas = el.getAttribute('data-canvas') || 'game-canvas';
            if (el.src) SlopScript.load(el.src, canvas);
            else if (el.textContent.trim()) SlopScript.run(el.textContent, canvas);
        });
    }
}
if (typeof document !== 'undefined') {
    if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', () => SlopScript.init());
    else SlopScript.init();
}
