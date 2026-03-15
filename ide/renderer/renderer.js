// slop3d IDE — renderer process

const DEBOUNCE_MS = 500;
const DEFAULT_BG = '#000000';

const DEFAULT_SOURCE = `assets
    model cube = cube.obj

scene main
    fill = ambient: 0.3, 0.3, 0.3
    sun = directional: 1.0, 0.9, 0.8, -1, -1, -1

    cam = camera: 0, 1.5, 5
    box = spawn: cube
    box.color = 1.0, 0.6, 0.2

    update
        box.rotation.y = t * 30
`;

let editor = null;
let monacoRef = null;
let currentEngine = null;
let reloadTimeout = null;
let projectRoot = '';
let assetBasePath = '';
let lastBgHex = DEFAULT_BG;

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
    const origLoadOBJ = SlopRuntime.prototype.loadOBJ;
    SlopRuntime.prototype.loadOBJ = function (url) {
        if (!url.startsWith('http') && !url.startsWith('file:') && !url.startsWith('/')) {
            url = 'file://' + assetBasePath + '/' + url;
        }
        return origLoadOBJ.call(this, url);
    };

    const origLoadTexture = SlopRuntime.prototype.loadTexture;
    SlopRuntime.prototype.loadTexture = function (url) {
        if (!url.startsWith('http') && !url.startsWith('file:') && !url.startsWith('/')) {
            url = 'file://' + assetBasePath + '/' + url;
        }
        return origLoadTexture.call(this, url);
    };

    // Monkey-patch sky() to sync editor background color
    const origSky = SlopRuntime.prototype.sky;
    SlopRuntime.prototype.sky = function (r, g, b) {
        origSky.call(this, r, g, b);
        updateEditorBackground(r, g, b);
    };

    setStatus('Engine loaded');
}

// --- Dynamic editor background ---

function updateEditorBackground(r, g, b) {
    const toHex = (v) => Math.round(Math.max(0, Math.min(1, v)) * 255)
        .toString(16).padStart(2, '0');
    const hex = '#' + toHex(r) + toHex(g) + toHex(b);

    // Skip if color hasn't changed (avoids per-frame theme rebuilds)
    if (hex === lastBgHex) return;
    lastBgHex = hex;

    // Update the background div (smooth CSS transition)
    document.getElementById('editor-bg').style.backgroundColor = hex;
    document.getElementById('titlebar-spacer').style.backgroundColor = hex;

    // Update Monaco theme to match
    if (monacoRef) {
        defineSlopTheme(monacoRef, hex);
        monacoRef.editor.setTheme('slop3d-dark');
    }
}

function resetEditorBackground() {
    updateEditorBackground(0, 0, 0); // engine default clear color
}

// --- Monaco setup ---

function initMonaco() {
    return new Promise((resolve) => {
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
            monacoRef = monaco;
            registerSlopScript(monaco);

            editor = monaco.editor.create(document.getElementById('editor-pane'), {
                value: DEFAULT_SOURCE,
                language: 'slopscript',
                theme: 'slop3d-dark',
                fontSize: 13,
                fontFamily: "'SF Mono', 'JetBrains Mono', 'Fira Code', 'Cascadia Code', monospace",
                lineHeight: 20,
                minimap: { enabled: false },
                lineNumbers: 'off',
                glyphMargin: false,
                folding: false,
                renderWhitespace: 'selection',
                tabSize: 4,
                insertSpaces: true,
                automaticLayout: true,
                scrollBeyondLastLine: false,
                padding: { top: 12, bottom: 12 },
                lineDecorationsWidth: 24,
                overviewRulerLanes: 0,
                hideCursorInOverviewRuler: true,
                scrollbar: {
                    verticalScrollbarSize: 8,
                    horizontalScrollbarSize: 8,
                },
                bracketPairColorization: { enabled: false },
                guides: { indentation: false, bracketPairs: false },
                smoothScrolling: true,
                cursorBlinking: 'smooth',
                cursorSmoothCaretAnimation: 'on',
            });

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
    let js;
    try {
        const tokens = slopLex(source);
        const ast = slopParse(tokens);
        js = slopGenerate(ast);
    } catch (err) {
        showError(err.message);
        return;
    }

    hideError();
    setStatus('Loading scene...');
    resetEditorBackground();

    try {
        const rt = new SlopRuntime('game-canvas');
        await rt.init();
        await SlopScript.exec(js, rt);

        if (currentEngine) {
            try { currentEngine.stop(); } catch (_) {}
        }
        currentEngine = rt;
        setStatus('Running');
    } catch (err) {
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

// --- Floating preview panel ---

function initFloatingPanel() {
    const panel = document.getElementById('preview-float');
    const edges = panel.querySelectorAll('.preview-edge');
    const app = document.getElementById('app');

    let dragging = false;
    let resizeEdge = null; // e.g. 'top-left', 'bottom', 'right'
    let dragOffsetX = 0;
    let dragOffsetY = 0;
    let resizeStart = null; // { x, y, left, top, width, height }
    let positionedByUser = false;

    const MIN_W = 200;
    const MIN_H = 150;

    function switchToAbsolute() {
        if (positionedByUser) return;
        const rect = panel.getBoundingClientRect();
        const appRect = app.getBoundingClientRect();
        panel.style.top = (rect.top - appRect.top) + 'px';
        panel.style.left = (rect.left - appRect.left) + 'px';
        panel.style.bottom = 'auto';
        panel.style.right = 'auto';
        positionedByUser = true;
    }

    // --- Drag (whole panel surface, except edges) ---
    panel.addEventListener('mousedown', (e) => {
        if (e.target.classList.contains('preview-edge')) return;
        dragging = true;
        const rect = panel.getBoundingClientRect();
        dragOffsetX = e.clientX - rect.left;
        dragOffsetY = e.clientY - rect.top;
        switchToAbsolute();
        e.preventDefault();
    });

    // --- Resize from any edge/corner ---
    edges.forEach((edge) => {
        edge.addEventListener('mousedown', (e) => {
            // Determine which edge from class name
            const classes = edge.className;
            if (classes.includes('edge-top-left')) resizeEdge = 'top-left';
            else if (classes.includes('edge-top-right')) resizeEdge = 'top-right';
            else if (classes.includes('edge-bottom-left')) resizeEdge = 'bottom-left';
            else if (classes.includes('edge-bottom-right')) resizeEdge = 'bottom-right';

            switchToAbsolute();
            const rect = panel.getBoundingClientRect();
            const appRect = app.getBoundingClientRect();
            resizeStart = {
                mouseX: e.clientX,
                mouseY: e.clientY,
                left: rect.left - appRect.left,
                top: rect.top - appRect.top,
                width: rect.width,
                height: rect.height,
            };
            e.preventDefault();
            e.stopPropagation();
        });
    });

    window.addEventListener('mousemove', (e) => {
        if (dragging) {
            const appRect = app.getBoundingClientRect();
            let x = e.clientX - dragOffsetX - appRect.left;
            let y = e.clientY - dragOffsetY - appRect.top;

            x = Math.max(0, Math.min(appRect.width - panel.offsetWidth, x));
            y = Math.max(0, Math.min(appRect.height - panel.offsetHeight, y));

            panel.style.left = x + 'px';
            panel.style.top = y + 'px';
        } else if (resizeEdge && resizeStart) {
            const dx = e.clientX - resizeStart.mouseX;
            const dy = e.clientY - resizeStart.mouseY;
            const appRect = app.getBoundingClientRect();
            const ASPECT = 4 / 3;

            let { left, top, width, height } = resizeStart;

            // Determine new width from the dominant drag axis, derive height from 4:3 ratio
            if (resizeEdge === 'bottom-right') {
                width = Math.max(MIN_W, resizeStart.width + dx);
                height = width / ASPECT;
            } else if (resizeEdge === 'bottom-left') {
                width = Math.max(MIN_W, resizeStart.width - dx);
                height = width / ASPECT;
                left = resizeStart.left + (resizeStart.width - width);
            } else if (resizeEdge === 'top-right') {
                width = Math.max(MIN_W, resizeStart.width + dx);
                height = width / ASPECT;
                top = resizeStart.top + (resizeStart.height - height);
            } else if (resizeEdge === 'top-left') {
                width = Math.max(MIN_W, resizeStart.width - dx);
                height = width / ASPECT;
                left = resizeStart.left + (resizeStart.width - width);
                top = resizeStart.top + (resizeStart.height - height);
            }

            // Clamp to app bounds
            left = Math.max(0, left);
            top = Math.max(0, top);
            if (left + width > appRect.width) {
                width = appRect.width - left;
                height = width / ASPECT;
            }
            if (top + height > appRect.height) {
                height = appRect.height - top;
                width = height * ASPECT;
            }

            panel.style.left = left + 'px';
            panel.style.top = top + 'px';
            panel.style.width = width + 'px';
            panel.style.height = height + 'px';
        }
    });

    window.addEventListener('mouseup', () => {
        dragging = false;
        resizeEdge = null;
        resizeStart = null;
    });

    // --- Keep panel in bounds on window resize ---
    window.addEventListener('resize', () => {
        if (!positionedByUser) return;
        const appRect = app.getBoundingClientRect();
        const panelRect = panel.getBoundingClientRect();
        let x = panelRect.left - appRect.left;
        let y = panelRect.top - appRect.top;

        x = Math.max(0, Math.min(appRect.width - panel.offsetWidth, x));
        y = Math.max(0, Math.min(appRect.height - panel.offsetHeight, y));

        panel.style.left = x + 'px';
        panel.style.top = y + 'px';
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
        case 'toggle-preview': {
            const panel = document.getElementById('preview-float');
            panel.classList.toggle('collapsed');
            break;
        }
    }
}

// --- Init ---

async function init() {
    try {
        setStatus('Loading editor...');
        projectRoot = await window.slop3dIDE.getProjectRoot();
        await initMonaco();

        setStatus('Loading engine...');
        await loadEngine();

        initFloatingPanel();
        window.slop3dIDE.onMenuAction(handleMenuAction);

        runScene(DEFAULT_SOURCE);
    } catch (err) {
        setStatus('Failed: ' + err.message);
        console.error('IDE init error:', err);
    }
}

init();
