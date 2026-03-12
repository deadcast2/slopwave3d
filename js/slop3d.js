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
        this._clearColor = this.module.cwrap('s3d_clear_color', null, ['number', 'number', 'number', 'number']);
        this._frameBegin = this.module.cwrap('s3d_frame_begin', null, []);
        this._getFramebuffer = this.module.cwrap('s3d_get_framebuffer', 'number', []);
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
        this._cameraClip = this.module.cwrap('s3d_camera_clip', null, ['number', 'number']);
        this._textureCreate = this.module.cwrap('s3d_texture_create', 'number', ['number', 'number']);
        this._textureGetDataPtr = this.module.cwrap('s3d_texture_get_data_ptr', 'number', ['number']);
        this._meshLoadObj = this.module.cwrap('s3d_mesh_load_obj', 'number', ['number', 'number']);
        this._objectCreate = this.module.cwrap('s3d_object_create', 'number', ['number', 'number']);
        this._objectDestroy = this.module.cwrap('s3d_object_destroy', null, ['number']);
        this._objectPosition = this.module.cwrap('s3d_object_position', null, ['number', 'number', 'number', 'number']);
        this._objectRotation = this.module.cwrap('s3d_object_rotation', null, ['number', 'number', 'number', 'number']);
        this._objectScale = this.module.cwrap('s3d_object_scale', null, ['number', 'number', 'number', 'number']);
        this._objectColor = this.module.cwrap('s3d_object_color', null, ['number', 'number', 'number', 'number']);
        this._objectAlpha = this.module.cwrap('s3d_object_alpha', null, ['number', 'number']);
        this._objectActive = this.module.cwrap('s3d_object_active', null, ['number', 'number']);
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
        this._cameraSet(pos.x, pos.y, pos.z, target.x, target.y, target.z, up.x, up.y, up.z);
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

    setLightAmbient(id, r, g, b) {
        this._lightAmbient(id, r, g, b);
    }

    setLightDirectional(id, r, g, b, dx, dy, dz) {
        this._lightDirectional(id, r, g, b, dx, dy, dz);
    }

    setLightPoint(id, r, g, b, x, y, z, range) {
        this._lightPoint(id, r, g, b, x, y, z, range);
    }

    setLightSpot(id, r, g, b, x, y, z, dx, dy, dz, range, innerDeg, outerDeg) {
        this._lightSpot(id, r, g, b, x, y, z, dx, dy, dz, range, innerDeg, outerDeg);
    }

    setLightOff(id) {
        this._lightOff(id);
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
                if (fpsEl) fpsEl.textContent = 'FPS: ' + this._fpsDisplay.toFixed(1);
                if (ftEl) ftEl.textContent = 'Frame: ' + this._ftDisplay.toFixed(2) + ' ms';
            }

            this._frameBegin();

            if (this._onUpdate) {
                this._onUpdate();
            }

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

// ============================================================
// SlopScript — DSL transpiler for slop3d
// ============================================================

// --- Phase 1: Runtime Helpers ---

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
    constructor(engine, id) {
        this._e = engine;
        this._id = id;
        this._alpha = 1;
        this._active = true;
        const e = engine,
            oid = id;
        this.position = new SlopVec3(v => e.setObjectPosition(oid, v._x, v._y, v._z));
        this.rotation = new SlopVec3(v => e.setObjectRotation(oid, v._x, v._y, v._z));
        this.scale = new SlopVec3(v => e.setObjectScale(oid, v._x, v._y, v._z), 1, 1, 1);
        this.color = new SlopVec3(v => e.setObjectColor(oid, v._x, v._y, v._z), 1, 1, 1);
    }
    get id() {
        return this._id;
    }
    get alpha() {
        return this._alpha;
    }
    set alpha(v) {
        this._alpha = v;
        this._e.setObjectAlpha(this._id, v);
    }
    get active() {
        return this._active;
    }
    set active(v) {
        this._active = v;
        this._e.setObjectActive(this._id, v ? 1 : 0);
    }
}

class SlopCamera {
    constructor(engine) {
        this._e = engine;
        this._fov = 60;
        this._near = 0.1;
        this._far = 100;
        this.position = new SlopVec3(() => this._flush(), 0, 0, 5);
        this.target = new SlopVec3(() => this._flush());
    }
    _flush() {
        const p = this.position,
            t = this.target;
        this._e.setCamera({ x: p._x, y: p._y, z: p._z }, { x: t._x, y: t._y, z: t._z });
    }
    get fov() {
        return this._fov;
    }
    set fov(v) {
        this._fov = v;
        this._e.setCameraFov(v);
    }
    get near() {
        return this._near;
    }
    set near(v) {
        this._near = v;
        this._e.setCameraClip(this._near, this._far);
    }
    get far() {
        return this._far;
    }
    set far(v) {
        this._far = v;
        this._e.setCameraClip(this._near, this._far);
    }
}

class SlopRuntime {
    constructor(engine) {
        this._e = engine;
        this.camera = new SlopCamera(engine);
        this.assets = { meshes: {}, textures: {} };
        this.scenes = {};
        this._activeScene = null;
        this._sceneObjects = [];
        this._scope = null;
        this.t = 0;
        this.dt = 0;
        this._startTime = 0;
        this._lastTime = 0;
    }
    spawn(meshName, texName) {
        const mid = this.assets.meshes[meshName];
        const tid = texName ? this.assets.textures[texName] : -1;
        const oid = this._e.createObject(mid, tid);
        const obj = new SlopObject(this._e, oid);
        this._sceneObjects.push(obj);
        return obj;
    }
    destroy(obj) {
        this._e.destroyObject(obj.id);
        const i = this._sceneObjects.indexOf(obj);
        if (i >= 0) this._sceneObjects.splice(i, 1);
    }
    light_ambient(id, r, g, b) {
        this._e.setLightAmbient(id, r, g, b);
    }
    light_directional(id, r, g, b, dx, dy, dz) {
        this._e.setLightDirectional(id, r, g, b, dx, dy, dz);
    }
    light_point(id, r, g, b, x, y, z, range) {
        this._e.setLightPoint(id, r, g, b, x, y, z, range);
    }
    light_spot(id, r, g, b, x, y, z, dx, dy, dz, range, innerDeg, outerDeg) {
        this._e.setLightSpot(id, r, g, b, x, y, z, dx, dy, dz, range, innerDeg, outerDeg);
    }
    light_off(id) {
        this._e.setLightOff(id);
    }
    gotoScene(name) {
        for (const obj of this._sceneObjects) this._e.destroyObject(obj.id);
        this._sceneObjects = [];
        this._activeScene = name;
        const scene = this.scenes[name];
        this._scope = scene.setup();
    }
    start() {
        this._startTime = performance.now();
        this._lastTime = this._startTime;
        this.gotoScene(this._firstScene);
        this._e.onUpdate(() => this._tick());
        this._e.start();
    }
    _tick() {
        const now = performance.now();
        this.dt = (now - this._lastTime) / 1000;
        this.t = (now - this._startTime) / 1000;
        this._lastTime = now;
        const scene = this.scenes[this._activeScene];
        if (scene.update) scene.update(this._scope);
        this._e.renderScene();
    }
}

// --- Phase 2: Lexer ---

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
    'destroy',
    'goto',
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
            while (pos < len && ch() !== '\n') adv();
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
                while (pos < len && ch() !== '\n') adv();
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
                while (pos < len && ((ch() >= '0' && ch() <= '9') || ch() === '.')) num += adv();
                tokens.push({
                    type: TK.NUMBER,
                    val: parseFloat(num),
                    line: ln,
                    col: cl,
                });
                continue;
            }
            // ident / keyword
            if ((ch() >= 'a' && ch() <= 'z') || (ch() >= 'A' && ch() <= 'Z') || ch() === '_') {
                let id = '';
                while (
                    pos < len &&
                    ((ch() >= 'a' && ch() <= 'z') ||
                        (ch() >= 'A' && ch() <= 'Z') ||
                        (ch() >= '0' && ch() <= '9') ||
                        ch() === '_')
                )
                    id += adv();
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

// --- Phase 3: Parser ---

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
        if (
            at(TK.IDENT) &&
            (t.val === 'goto' ||
                t.val === 'destroy' ||
                t.val === 'light_ambient' ||
                t.val === 'light_directional' ||
                t.val === 'light_point' ||
                t.val === 'light_spot' ||
                t.val === 'light_off') &&
            tokens[pos + 1] &&
            tokens[pos + 1].type === TK.COLON
        )
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

// --- Phase 4: Code Generator ---

const SLOP_BUILTINS = new Set(['t', 'dt', 'camera']);
const SLOP_MATH = new Set(['sin', 'cos', 'tan', 'lerp', 'clamp', 'random', 'abs', 'min', 'max', 'range']);

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
            loads.push(`_e.loadOBJ('assets/${m.path}')`);
        }
        for (const s of ast.assets.skins) {
            names.push('_tex_' + s.name);
            loads.push(`_e.loadTexture('assets/${s.path}')`);
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
            case 'CallStmt':
                if (node.name === 'goto') {
                    emit(`_rt.gotoScene('${node.args[0].name}'); return;`);
                } else if (node.name === 'destroy') {
                    emit(`_rt.destroy(${emitExpr(node.args[0])});`);
                } else if (node.name.startsWith('light_')) {
                    const args = node.args.map(emitExpr).join(', ');
                    emit(`_rt.${node.name}(${args});`);
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
                if (node.name === 't' || node.name === 'dt') return '_rt.' + node.name;
                if (node.name === 'camera') return '_rt.camera';
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
                if (SLOP_MATH.has(node.name)) return `_${node.name}(${node.args.map(emitExpr).join(', ')})`;
                return `_s.${node.name}(${node.args.map(emitExpr).join(', ')})`;
            case 'Group':
                return `(${emitExpr(node.expr)})`;
            default:
                return '/* ??? */';
        }
    }
}

// --- Phase 5: Loader ---

class SlopScript {
    static async run(source, canvasId) {
        const engine = new Slop3D(canvasId);
        await engine.init();
        const tokens = slopLex(source);
        const ast = slopParse(tokens);
        const js = slopGenerate(ast);
        const fn = new Function(
            '_e',
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
            'return (async function(){' + js + '})();'
        );
        const rt = new SlopRuntime(engine);
        await fn(engine, rt, _sin, _cos, _tan, _lerp, _clamp, _random, _abs, _min, _max, _range);
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
