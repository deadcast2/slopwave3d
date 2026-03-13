// slop3d IDE — renderer process

const DEBOUNCE_MS = 500;

const DEFAULT_SOURCE = `assets
    model cube = cube.obj
    skin crate = crate.jpg

scene main
    fill = ambient: 0.3, 0.3, 0.3
    sun = directional: 1.0, 0.9, 0.8, -1, -1, -1

    box = spawn: cube, crate
    cam = camera: 0, 1.5, 5

    update
        box.rotation.y = t * 30
`;

let editor = null;
let currentEngine = null;
let reloadTimeout = null;
let projectRoot = '';
let assetBasePath = '';

// --- Script loading ---

function loadScript(src) {
    return new Promise((resolve, reject) => {
        const s = document.createElement('script');
        s.src = src;
        s.onload = resolve;
        s.onerror = () => reject(new Error('Failed to load: ' + src));
        document.head.appendChild(s);
    });
}

// --- Engine loading ---

async function loadEngine() {
    projectRoot = await window.slop3dIDE.getProjectRoot();
    assetBasePath = await window.slop3dIDE.getAssetBase();

    await loadScript('file://' + projectRoot + '/web/slop3d_wasm.js');
    await loadScript('file://' + projectRoot + '/js/slop3d.js');

    // Monkey-patch asset loading to resolve paths against the asset base
    // instead of using a <base> tag (which breaks Monaco's URL resolution)
    const origLoadOBJ = Slop3D.prototype.loadOBJ;
    Slop3D.prototype.loadOBJ = function (url) {
        if (!url.startsWith('http') && !url.startsWith('file:') && !url.startsWith('/')) {
            url = 'file://' + assetBasePath + '/' + url;
        }
        return origLoadOBJ.call(this, url);
    };

    const origLoadTexture = Slop3D.prototype.loadTexture;
    Slop3D.prototype.loadTexture = function (url) {
        if (!url.startsWith('http') && !url.startsWith('file:') && !url.startsWith('/')) {
            url = 'file://' + assetBasePath + '/' + url;
        }
        return origLoadTexture.call(this, url);
    };

    setStatus('Engine loaded');
}

// --- Monaco setup ---

function initMonaco() {
    return new Promise((resolve) => {
        // Monaco workers need absolute paths in Electron
        const monacoBase = projectRoot + '/ide/node_modules/monaco-editor/min/';
        self.MonacoEnvironment = {
            getWorkerUrl: function () {
                return `data:text/javascript;charset=utf-8,${encodeURIComponent(
                    `self.MonacoEnvironment = { baseUrl: 'file://${monacoBase}' };
                     importScripts('file://${monacoBase}vs/base/worker/workerMain.js');`
                )}`;
            },
        };

        require.config({
            paths: { vs: '../node_modules/monaco-editor/min/vs' },
        });

        require(['vs/editor/editor.main'], function (monaco) {
            registerSlopScript(monaco);

            editor = monaco.editor.create(document.getElementById('editor-pane'), {
                value: DEFAULT_SOURCE,
                language: 'slopscript',
                theme: 'slop3d-dark',
                fontSize: 13,
                fontFamily: "'SF Mono', 'JetBrains Mono', 'Fira Code', 'Cascadia Code', monospace",
                lineHeight: 20,
                minimap: { enabled: false },
                lineNumbers: 'on',
                renderWhitespace: 'selection',
                tabSize: 4,
                insertSpaces: true,
                automaticLayout: true,
                scrollBeyondLastLine: false,
                padding: { top: 12, bottom: 12 },
                overviewRulerLanes: 0,
                hideCursorInOverviewRuler: true,
                scrollbar: {
                    verticalScrollbarSize: 8,
                    horizontalScrollbarSize: 8,
                },
                bracketPairColorization: { enabled: false },
                guides: { indentation: true, bracketPairs: false },
                smoothScrolling: true,
                cursorBlinking: 'smooth',
                cursorSmoothCaretAnimation: 'on',
            });

            // Debounced live reload on edit
            editor.onDidChangeModelContent(() => {
                window.slop3dIDE.setDirty(true);
                scheduleReload();
            });

            resolve();
        });
    });
}

// --- Live preview ---

async function runScene(source) {
    // Phase 1: try transpiling (fast, no side effects)
    // On error, keep the previous scene running and show overlay
    let js;
    try {
        const tokens = slopLex(source);
        const ast = slopParse(tokens);
        js = slopGenerate(ast);
    } catch (err) {
        showError(err.message);
        return;
    }

    // Phase 2: transpile succeeded — spin up new engine, then swap
    hideError();
    setStatus('Loading scene...');

    try {
        const engine = new Slop3D('game-canvas');
        await engine.init();
        await SlopScript.exec(js, engine);

        // New engine is running — now stop the old one
        if (currentEngine) {
            try { currentEngine.stop(); } catch (_) {}
        }
        currentEngine = engine;
        setStatus('Running');
    } catch (err) {
        // Runtime error — keep previous scene running, show error on top
        showError('Runtime: ' + err.message);
    }
}

function scheduleReload() {
    clearTimeout(reloadTimeout);
    reloadTimeout = setTimeout(() => {
        const source = editor.getValue();
        if (source.trim()) runScene(source);
    }, DEBOUNCE_MS);
}

// --- Error overlay ---

function showError(msg) {
    const banner = document.getElementById('error-banner');
    const text = document.getElementById('error-message');
    text.textContent = msg;
    banner.classList.remove('hidden');
    setStatus('Error');
}

function hideError() {
    document.getElementById('error-banner').classList.add('hidden');
}

function setStatus(msg) {
    document.getElementById('status-text').textContent = msg;
}

// --- Resizable divider ---

function initDivider() {
    const divider = document.getElementById('divider');
    const editorPane = document.getElementById('editor-pane');
    const previewPane = document.getElementById('preview-pane');

    let dragging = false;

    divider.addEventListener('mousedown', (e) => {
        dragging = true;
        divider.classList.add('dragging');
        e.preventDefault();
    });

    window.addEventListener('mousemove', (e) => {
        if (!dragging) return;
        const appRect = document.getElementById('app').getBoundingClientRect();
        const x = e.clientX - appRect.left;
        const dividerW = 3;
        const minEditor = 300;
        const minPreview = 360;
        const maxEditor = appRect.width - dividerW - minPreview;
        const clamped = Math.max(minEditor, Math.min(maxEditor, x));
        editorPane.style.flex = 'none';
        editorPane.style.width = clamped + 'px';
        previewPane.style.width = (appRect.width - clamped - dividerW) + 'px';
    });

    window.addEventListener('mouseup', () => {
        if (dragging) {
            dragging = false;
            divider.classList.remove('dragging');
        }
    });
}

// --- File operations via IPC ---

async function handleMenuAction(action) {
    switch (action) {
        case 'new':
            await window.slop3dIDE.newFile();
            assetBasePath = projectRoot + '/web';
            editor.setValue(DEFAULT_SOURCE);
            break;
        case 'open': {
            const result = await window.slop3dIDE.openFile();
            if (result) {
                // Update asset base to the file's directory
                const dir = result.path.substring(0, result.path.lastIndexOf('/'));
                assetBasePath = dir;
                editor.setValue(result.content);
            }
            break;
        }
        case 'save':
            await window.slop3dIDE.saveFile(editor.getValue());
            break;
        case 'save-as':
            await window.slop3dIDE.saveFileAs(editor.getValue());
            break;
    }
}

// --- Init ---

async function init() {
    try {
        // 1. Load Monaco first (before any URL shenanigans)
        setStatus('Loading editor...');
        projectRoot = await window.slop3dIDE.getProjectRoot();
        await initMonaco();

        // 2. Load engine (monkey-patches asset loading for file:// paths)
        setStatus('Loading engine...');
        await loadEngine();

        // 3. Set up UI
        initDivider();
        window.slop3dIDE.onMenuAction(handleMenuAction);

        // 4. Run initial scene
        runScene(DEFAULT_SOURCE);
    } catch (err) {
        setStatus('Failed: ' + err.message);
        console.error('IDE init error:', err);
    }
}

init();
