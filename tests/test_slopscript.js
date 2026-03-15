const { describe, it } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('fs');
const path = require('path');

// Load SlopScript functions from slop3d.js
const src = fs.readFileSync(path.join(__dirname, '..', 'js', 'slop3d.js'), 'utf8');
// Strip browser-only code
const stripped = src.replace(/if \(typeof document[\s\S]*$/, '').replace(/class SlopScript \{[\s\S]*?^\}/m, '');
const mod = new Function(
    stripped +
        '\nreturn { SlopVec3, SlopObject, SlopLight, SlopCamera, SlopRuntime, slopLex, slopParse, slopGenerate, _sin, _cos, _tan, _lerp, _clamp, _random, _abs, _min, _max, _range };'
);
const {
    SlopVec3,
    SlopObject,
    SlopLight,
    SlopCamera,
    SlopRuntime,
    slopLex,
    slopParse,
    slopGenerate,
    _sin,
    _cos,
    _tan,
    _lerp,
    _clamp,
    _random,
    _abs,
    _min,
    _max,
    _range,
} = mod();

// --- Lexer Tests ---

describe('Lexer', () => {
    it('tokenizes a minimal program', () => {
        const tokens = slopLex('assets\n    model cube = cube.obj\n');
        const types = tokens.map(t => t.type);
        assert.deepEqual(types, ['ID', 'NL', 'INDENT', 'ID', 'ID', '=', 'ID', '.', 'ID', 'NL', 'DEDENT', 'EOF']);
    });

    it('handles multiple indent levels', () => {
        const tokens = slopLex('scene main\n    update\n        box.x = 1\n');
        const types = tokens.map(t => t.type);
        assert.ok(types.includes('INDENT'));
        const indents = types.filter(t => t === 'INDENT').length;
        const dedents = types.filter(t => t === 'DEDENT').length;
        assert.equal(indents, dedents, 'INDENT/DEDENT should be balanced');
    });

    it('skips comments', () => {
        const tokens = slopLex('# this is a comment\nassets\n');
        assert.equal(tokens[0].type, 'ID');
        assert.equal(tokens[0].val, 'assets');
    });

    it('skips blank lines', () => {
        const tokens = slopLex('assets\n\n\n    model cube = cube.obj\n');
        const types = tokens.map(t => t.type);
        assert.ok(!types.includes(undefined));
        assert.equal(tokens[0].val, 'assets');
    });

    it('tokenizes numbers correctly', () => {
        const tokens = slopLex('scene a\n    x = 3.14\n');
        const nums = tokens.filter(t => t.type === 'NUM');
        assert.equal(nums.length, 1);
        assert.equal(nums[0].val, 3.14);
    });

    it('tokenizes negative number in expression', () => {
        const tokens = slopLex('scene a\n    x = 5 * -2\n');
        const types = tokens.filter(t => t.type !== 'NL').map(t => t.type);
        assert.ok(types.includes('-'));
    });

    it('tokenizes two-char operators', () => {
        const tokens = slopLex('scene a\n    if x == 5\n        y = 1\n');
        const ops = tokens.filter(t => t.type === '==');
        assert.equal(ops.length, 1);
    });

    it('tracks line numbers', () => {
        const tokens = slopLex('assets\n    model a = a.obj\n    skin b = b.jpg\n');
        const skin = tokens.find(t => t.val === 'skin');
        assert.equal(skin.line, 3);
    });

    it('errors on bad indentation', () => {
        assert.throws(() => slopLex('scene a\n    x = 1\n  y = 2\n'), /indent/i);
    });
    it('errors on multiple dots in number', () => {
        assert.throws(() => slopLex('scene a\n    x = 1.2.3\n'), /Invalid number/);
    });
});

// --- Parser Tests ---

describe('Parser', () => {
    function parse(src) {
        return slopParse(slopLex(src));
    }

    it('parses asset declarations', () => {
        const ast = parse('assets\n    model cube = cube.obj\n    skin crate = crate.jpg\n');
        assert.equal(ast.assets.models.length, 1);
        assert.equal(ast.assets.models[0].name, 'cube');
        assert.equal(ast.assets.models[0].path, 'cube.obj');
        assert.equal(ast.assets.skins.length, 1);
        assert.equal(ast.assets.skins[0].name, 'crate');
        assert.equal(ast.assets.skins[0].path, 'crate.jpg');
    });

    it('parses spawn with texture', () => {
        const ast = parse('assets\n    model c = c.obj\nscene main\n    box = spawn: c, crate\n');
        const stmt = ast.scenes[0].body[0];
        assert.equal(stmt.type, 'SpawnAssign');
        assert.equal(stmt.mesh, 'c');
        assert.equal(stmt.skin, 'crate');
    });

    it('parses spawn without texture', () => {
        const ast = parse('assets\n    model c = c.obj\nscene main\n    box = spawn: c\n');
        const stmt = ast.scenes[0].body[0];
        assert.equal(stmt.type, 'SpawnAssign');
        assert.equal(stmt.skin, null);
    });

    it('parses tuple assignment', () => {
        const ast = parse('scene main\n    box.position = 1, 2, 3\n');
        const stmt = ast.scenes[0].body[0];
        assert.equal(stmt.type, 'Assign');
        assert.equal(stmt.value.type, 'Tuple');
        assert.equal(stmt.value.elements.length, 3);
    });

    it('parses scalar assignment', () => {
        const ast = parse('scene main\n    box.alpha = 0.5\n');
        const stmt = ast.scenes[0].body[0];
        assert.equal(stmt.type, 'Assign');
        assert.equal(stmt.value.type, 'Num');
    });

    it('parses dot chain in target', () => {
        const ast = parse('scene main\n    box.rotation.y = 30\n');
        const target = ast.scenes[0].body[0].target;
        assert.equal(target.type, 'Dot');
        assert.equal(target.property, 'y');
        assert.equal(target.object.type, 'Dot');
        assert.equal(target.object.property, 'rotation');
    });

    it('parses function call in expression', () => {
        const ast = parse('scene main\n    x = sin[t * 2] * 5\n');
        const val = ast.scenes[0].body[0].value;
        assert.equal(val.type, 'BinOp');
        assert.equal(val.op, '*');
        assert.equal(val.left.type, 'Call');
        assert.equal(val.left.name, 'sin');
    });

    it('parses grouping brackets', () => {
        const ast = parse('scene main\n    x = [a + b] * c\n');
        const val = ast.scenes[0].body[0].value;
        assert.equal(val.type, 'BinOp');
        assert.equal(val.left.type, 'Group');
    });

    it('parses update block inside scene', () => {
        const ast = parse('scene main\n    box = spawn: c\n    update\n        box.rotation.y = t * 30\n');
        assert.ok(ast.scenes[0].update);
        assert.equal(ast.scenes[0].update.length, 1);
    });

    it('parses if/elif/else', () => {
        const ast = parse(
            'scene main\n    update\n        if x > 5\n            y = 1\n        elif x > 0\n            y = 2\n        else\n            y = 3\n'
        );
        const stmt = ast.scenes[0].update[0];
        assert.equal(stmt.type, 'If');
        assert.equal(stmt.elifs.length, 1);
        assert.ok(stmt.elseBody);
    });

    it('parses while loop', () => {
        const ast = parse('scene main\n    update\n        while x < 10\n            x = x + 1\n');
        assert.equal(ast.scenes[0].update[0].type, 'While');
    });

    it('parses for loop', () => {
        const ast = parse('scene main\n    update\n        for i in range[10]\n            x = i\n');
        const stmt = ast.scenes[0].update[0];
        assert.equal(stmt.type, 'For');
        assert.equal(stmt.varName, 'i');
    });

    it('parses fn definition', () => {
        const ast = parse('scene main\n    fn wobble: obj, speed\n        obj.rotation.y = t * speed\n');
        const stmt = ast.scenes[0].body[0];
        assert.equal(stmt.type, 'FnDef');
        assert.equal(stmt.name, 'wobble');
        assert.deepEqual(stmt.params, ['obj', 'speed']);
    });

    it('parses goto statement', () => {
        const ast = parse('scene main\n    update\n        goto: game\n');
        const stmt = ast.scenes[0].update[0];
        assert.equal(stmt.type, 'CallStmt');
        assert.equal(stmt.name, 'goto');
    });

    it('parses multiple scenes', () => {
        const ast = parse('scene menu\n    x = 1\nscene game\n    y = 2\n');
        assert.equal(ast.scenes.length, 2);
        assert.equal(ast.scenes[0].name, 'menu');
        assert.equal(ast.scenes[1].name, 'game');
    });

    it('respects operator precedence', () => {
        const ast = parse('scene main\n    x = 1 + 2 * 3\n');
        const val = ast.scenes[0].body[0].value;
        // should be (1 + (2 * 3)), so top node is +
        assert.equal(val.type, 'BinOp');
        assert.equal(val.op, '+');
        assert.equal(val.right.type, 'BinOp');
        assert.equal(val.right.op, '*');
    });

    it('parses boolean operators', () => {
        const ast = parse('scene main\n    update\n        if x > 0 and y < 10\n            z = 1\n');
        const cond = ast.scenes[0].update[0].condition;
        assert.equal(cond.type, 'BinOp');
        assert.equal(cond.op, 'and');
    });

    it('parses unary not', () => {
        const ast = parse('scene main\n    update\n        if not done\n            x = 1\n');
        const cond = ast.scenes[0].update[0].condition;
        assert.equal(cond.type, 'Unary');
        assert.equal(cond.op, 'not');
    });

    it('parses ambient light assignment', () => {
        const ast = parse('scene main\n    fill = ambient: 0.3, 0.3, 0.3\n');
        const stmt = ast.scenes[0].body[0];
        assert.equal(stmt.type, 'LightAssign');
        assert.equal(stmt.lightType, 'ambient');
        assert.equal(stmt.args.length, 3);
    });

    it('parses directional light assignment', () => {
        const ast = parse('scene main\n    sun = directional: 1, 0.9, 0.8, -1, -1, -1\n');
        const stmt = ast.scenes[0].body[0];
        assert.equal(stmt.type, 'LightAssign');
        assert.equal(stmt.lightType, 'directional');
        assert.equal(stmt.args.length, 6);
    });

    it('parses point light assignment', () => {
        const ast = parse('scene main\n    glow = point: 1, 1, 1, 0, 5, 0, 10\n');
        const stmt = ast.scenes[0].body[0];
        assert.equal(stmt.type, 'LightAssign');
        assert.equal(stmt.lightType, 'point');
        assert.equal(stmt.args.length, 7);
    });

    it('parses spot light assignment', () => {
        const ast = parse('scene main\n    beam = spot: 1, 1, 1, 0, 5, 0, 0, -1, 0, 20, 15, 30\n');
        const stmt = ast.scenes[0].body[0];
        assert.equal(stmt.type, 'LightAssign');
        assert.equal(stmt.lightType, 'spot');
        assert.equal(stmt.args.length, 12);
    });

    it('parses off statement', () => {
        const ast = parse('scene main\n    update\n        off: sun\n');
        const stmt = ast.scenes[0].update[0];
        assert.equal(stmt.type, 'CallStmt');
        assert.equal(stmt.name, 'off');
    });

    it('parses on statement', () => {
        const ast = parse('scene main\n    update\n        on: sun\n');
        const stmt = ast.scenes[0].update[0];
        assert.equal(stmt.type, 'CallStmt');
        assert.equal(stmt.name, 'on');
    });

    it('parses sky statement', () => {
        const ast = parse('scene main\n    sky: 0.2, 0.1, 0.3\n');
        const stmt = ast.scenes[0].body[0];
        assert.equal(stmt.type, 'CallStmt');
        assert.equal(stmt.name, 'sky');
        assert.equal(stmt.args.length, 3);
    });

    it('parses camera creation', () => {
        const ast = parse('scene main\n    cam = camera: 0, 1.5, 5\n');
        const stmt = ast.scenes[0].body[0];
        assert.equal(stmt.type, 'CameraAssign');
        assert.equal(stmt.args.length, 3);
    });
});

// --- Code Generator Tests ---

describe('CodeGen', () => {
    function gen(src) {
        return slopGenerate(slopParse(slopLex(src)));
    }

    it('generates asset loading', () => {
        const js = gen('assets\n    model cube = cube.obj\n    skin crate = crate.jpg\nscene main\n    x = 1\n');
        assert.ok(js.includes("loadOBJ('assets/cube.obj')"));
        assert.ok(js.includes("loadTexture('assets/crate.jpg')"));
        assert.ok(js.includes('Promise.all'));
    });

    it('generates spawn call', () => {
        const js = gen('scene main\n    box = spawn: cube, crate\n');
        assert.ok(js.includes("_rt.spawn('cube', 'crate')"));
    });

    it('generates setAll for tuples', () => {
        const js = gen('scene main\n    box.position = 1, 2, 3\n');
        assert.ok(js.includes('.setAll(1, 2, 3)'));
    });

    it('generates scalar assignment', () => {
        const js = gen('scene main\n    box.alpha = 0.5\n');
        assert.ok(js.includes('_s.box.alpha = 0.5'));
    });

    it('generates style assignment with ps1 constant', () => {
        const js = gen('scene main\n    box.style = ps1\n');
        assert.ok(js.includes('_s.box.style = 0'));
    });

    it('generates style assignment with n64 constant', () => {
        const js = gen('scene main\n    box.style = n64\n');
        assert.ok(js.includes('_s.box.style = 1'));
    });

    it('generates dot chain assignment', () => {
        const js = gen('scene main\n    update\n        box.rotation.y = t * 30\n');
        assert.ok(js.includes('_s.box.rotation.y = (_rt.t * 30)'));
    });

    it('maps t and dt to runtime', () => {
        const js = gen('scene main\n    update\n        x = t + dt\n');
        assert.ok(js.includes('_rt.t'));
        assert.ok(js.includes('_rt.dt'));
    });

    it('generates camera creation', () => {
        const js = gen('scene main\n    cam = camera: 0, 1, 5\n');
        assert.ok(js.includes('_rt.camera(0, 1, 5)'));
    });

    it('generates act_as with fps constant', () => {
        const js = gen('scene main\n    cam = camera: 0, 1.5, 5\n    cam.act_as = fps\n');
        assert.ok(js.includes('_rt.camera(0, 1.5, 5)'));
        assert.ok(js.includes("_s.cam.act_as = 'fps'"));
    });

    it('generates use call', () => {
        const js = gen('scene main\n    cam = camera: 0, 1, 5\n    update\n        use: cam\n');
        assert.ok(js.includes('_rt.use(_s.cam)'));
    });

    it('generates degree-based math calls', () => {
        const js = gen('scene main\n    update\n        x = sin[t * 30] * 5\n');
        assert.ok(js.includes('_sin('));
    });

    it('generates goto with return', () => {
        const js = gen('scene main\n    update\n        goto: game\n');
        assert.ok(js.includes("_rt.gotoScene('game')"));
        assert.ok(js.includes('return'));
    });

    it('generates if/else', () => {
        const js = gen(
            'scene main\n    update\n        if x > 5\n            y = 1\n        else\n            y = 2\n'
        );
        assert.ok(js.includes('if ('));
        assert.ok(js.includes('} else {'));
    });

    it('generates boolean operators as && ||', () => {
        const js = gen('scene main\n    update\n        if x > 0 and y < 10\n            z = 1\n');
        assert.ok(js.includes('&&'));
    });

    it('generates not as !', () => {
        const js = gen('scene main\n    update\n        if not done\n            x = 1\n');
        assert.ok(js.includes('(!'));
    });

    it('generates for loop', () => {
        const js = gen('scene main\n    update\n        for i in range[10]\n            x = i\n');
        assert.ok(js.includes('for (const i of'));
        assert.ok(js.includes('_range(10)'));
    });

    it('generates fn definition', () => {
        const js = gen('scene main\n    fn wobble: obj, speed\n        obj.rotation.y = t * speed\n');
        assert.ok(js.includes('_s.wobble = function(obj, speed)'));
    });

    it('generates first scene as default', () => {
        const js = gen('scene menu\n    x = 1\nscene game\n    y = 2\n');
        assert.ok(js.includes("_rt._firstScene = 'menu'"));
    });

    it('generates ambient light creation', () => {
        const js = gen('scene main\n    fill = ambient: 0.3, 0.3, 0.3\n');
        assert.ok(js.includes("_rt.light('ambient', 0.3, 0.3, 0.3)"));
    });

    it('generates directional light creation', () => {
        const js = gen('scene main\n    sun = directional: 1, 0.9, 0.8, -1, -1, -1\n');
        assert.ok(js.includes("_rt.light('directional', 1, 0.9, 0.8, (-1), (-1), (-1))"));
    });

    it('generates point light creation', () => {
        const js = gen('scene main\n    glow = point: 1, 1, 1, 0, 5, 0, 10\n');
        assert.ok(js.includes("_rt.light('point', 1, 1, 1, 0, 5, 0, 10)"));
    });

    it('generates off call', () => {
        const js = gen('scene main\n    update\n        off: sun\n');
        assert.ok(js.includes('_rt.off(_s.sun)'));
    });

    it('generates on call', () => {
        const js = gen('scene main\n    update\n        on: sun\n');
        assert.ok(js.includes('_rt.on(_s.sun)'));
    });

    it('generates sky call', () => {
        const js = gen('scene main\n    sky: 0.2, 0.1, 0.3\n');
        assert.ok(js.includes('_rt.sky(0.2, 0.1, 0.3)'));
    });

    it('generates asset loads via _rt', () => {
        const js = gen('assets\n    model cube = cube.obj\n    skin crate = crate.jpg\nscene main\n    x = 1\n');
        assert.ok(js.includes("_rt.loadOBJ('assets/cube.obj')"));
        assert.ok(js.includes("_rt.loadTexture('assets/crate.jpg')"));
    });

    it('full spinning cube generates valid structure', () => {
        const js = gen(
            'assets\n    model cube = cube.obj\nscene main\n    box = spawn: cube\n    cam = camera: 0, 1.5, 5\n    update\n        box.rotation.y = t * 30\n'
        );
        assert.ok(js.includes("_rt.spawn('cube')"));
        assert.ok(js.includes('_rt.camera(0, 1.5, 5)'));
        assert.ok(js.includes('_s.box.rotation.y'));
        assert.ok(js.includes('_rt.start()'));
    });
});

// --- Runtime Tests ---

describe('Runtime helpers', () => {
    it('SlopVec3 fires onChange on x set', () => {
        let called = false;
        const v = new SlopVec3(vec => {
            called = true;
        });
        v.x = 5;
        assert.ok(called);
        assert.equal(v.x, 5);
    });

    it('SlopVec3 setAll fires onChange once', () => {
        let count = 0;
        const v = new SlopVec3(() => count++);
        v.setAll(1, 2, 3);
        assert.equal(count, 1);
        assert.equal(v.x, 1);
        assert.equal(v.y, 2);
        assert.equal(v.z, 3);
    });

    it('math functions are degree-based', () => {
        assert.ok(Math.abs(_sin(90) - 1) < 0.0001);
        assert.ok(Math.abs(_cos(0) - 1) < 0.0001);
        assert.ok(Math.abs(_sin(0)) < 0.0001);
        assert.ok(Math.abs(_cos(90)) < 0.0001);
    });

    it('lerp works', () => {
        assert.equal(_lerp(0, 10, 0.5), 5);
        assert.equal(_lerp(0, 10, 0), 0);
        assert.equal(_lerp(0, 10, 1), 10);
    });

    it('clamp works', () => {
        assert.equal(_clamp(5, 0, 10), 5);
        assert.equal(_clamp(-1, 0, 10), 0);
        assert.equal(_clamp(15, 0, 10), 10);
    });

    it('range generates array', () => {
        assert.deepEqual(_range(5), [0, 1, 2, 3, 4]);
        assert.deepEqual(_range(0), []);
    });

    it('SlopLight flushes on color change', () => {
        let lastCall = null;
        const fakeRt = {
            _lightAmbient: (id, r, g, b) => {
                lastCall = { id, r, g, b };
            },
        };
        const l = new SlopLight(fakeRt, 0, 'ambient');
        l.color.setAll(0.5, 0.6, 0.7);
        assert.deepEqual(lastCall, { id: 0, r: 0.5, g: 0.6, b: 0.7 });
    });

    it('SlopLight flushes on position change for point light', () => {
        let lastCall = null;
        const fakeRt = {
            _lightPoint: (id, r, g, b, x, y, z, range) => {
                lastCall = { id, x, y, z, range };
            },
        };
        const l = new SlopLight(fakeRt, 2, 'point');
        l._range = 15;
        l.position.setAll(1, 2, 3);
        assert.equal(lastCall.id, 2);
        assert.equal(lastCall.x, 1);
        assert.equal(lastCall.y, 2);
        assert.equal(lastCall.z, 3);
        assert.equal(lastCall.range, 15);
    });

    it('SlopLight flushes on range setter', () => {
        let callCount = 0;
        const fakeRt = {
            _lightPoint: () => {
                callCount++;
            },
        };
        const l = new SlopLight(fakeRt, 0, 'point');
        callCount = 0;
        l.range = 20;
        assert.equal(l.range, 20);
        assert.equal(callCount, 1);
    });

    it('SlopLight directional flushes with direction', () => {
        let lastCall = null;
        const fakeRt = {
            _lightDirectional: (id, r, g, b, dx, dy, dz) => {
                lastCall = { id, r, g, b, dx, dy, dz };
            },
        };
        const l = new SlopLight(fakeRt, 1, 'directional');
        l.direction.setAll(-1, -1, 0);
        assert.equal(lastCall.id, 1);
        assert.equal(lastCall.dx, -1);
        assert.equal(lastCall.dy, -1);
        assert.equal(lastCall.dz, 0);
    });
});
