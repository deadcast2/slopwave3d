// Monaco language definition for SlopScript
// Called after Monaco AMD modules are loaded

// --- OKLCH color science ---
// Perceptually uniform color space for adaptive syntax highlighting.
// Pipeline: sRGB → linear RGB → XYZ (D65) → OKLAB → OKLCH

function srgbToLinear(c) {
    c /= 255;
    return c <= 0.04045 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4);
}

function linearToSrgb(c) {
    c = Math.max(0, Math.min(1, c));
    return c <= 0.0031308 ? c * 12.92 : 1.055 * Math.pow(c, 1 / 2.4) - 0.055;
}

function rgbToOklch(r, g, b) {
    const lr = srgbToLinear(r), lg = srgbToLinear(g), lb = srgbToLinear(b);

    // Linear RGB → LMS (cone response)
    const l = 0.4122214708 * lr + 0.5363325363 * lg + 0.0514459929 * lb;
    const m = 0.2119034982 * lr + 0.6806995451 * lg + 0.1073969566 * lb;
    const s = 0.0883024619 * lr + 0.2817188376 * lg + 0.6299787005 * lb;

    const l_ = Math.cbrt(l), m_ = Math.cbrt(m), s_ = Math.cbrt(s);

    // LMS → OKLAB
    const L = 0.2104542553 * l_ + 0.7936177850 * m_ - 0.0040720468 * s_;
    const a = 1.9779984951 * l_ - 2.4285922050 * m_ + 0.4505937099 * s_;
    const bv = 0.0259040371 * l_ + 0.7827717662 * m_ - 0.8086757660 * s_;

    // OKLAB → OKLCH
    const C = Math.sqrt(a * a + bv * bv);
    let H = Math.atan2(bv, a) * 180 / Math.PI;
    if (H < 0) H += 360;

    return { L, C, H };
}

function oklchToRgb(L, C, H) {
    const hRad = H * Math.PI / 180;
    const a = C * Math.cos(hRad);
    const bv = C * Math.sin(hRad);

    // OKLAB → LMS
    const l_ = L + 0.3963377774 * a + 0.2158037573 * bv;
    const m_ = L - 0.1055613458 * a - 0.0638541728 * bv;
    const s_ = L - 0.0894841775 * a - 1.2914855480 * bv;

    const l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;

    // LMS → linear RGB
    const lr =  4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s;
    const lg = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s;
    const lb = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s;

    return [
        Math.round(linearToSrgb(lr) * 255),
        Math.round(linearToSrgb(lg) * 255),
        Math.round(linearToSrgb(lb) * 255),
    ];
}

function hexToRgb(hex) {
    return [
        parseInt(hex.slice(1, 3), 16),
        parseInt(hex.slice(3, 5), 16),
        parseInt(hex.slice(5, 7), 16),
    ];
}

function rgbToHex(r, g, b) {
    return '#' + [r, g, b].map(v =>
        Math.max(0, Math.min(255, v)).toString(16).padStart(2, '0')
    ).join('');
}

// --- Adaptive token colors ---
// Each token is defined by its hue identity and chroma.
// Lightness is computed at runtime to contrast against the background.

const TOKEN_DEFS = {
    'keyword':         { H: 295, C: 0.14 },  // purple
    'keyword.light':   { H:  75, C: 0.14 },  // gold
    'predefined':      { H: 255, C: 0.12 },  // blue
    'variable.global': { H:  20, C: 0.15 },  // red-orange
    'number':          { H:  45, C: 0.14 },  // orange
    'comment':         { H: 230, C: 0.02 },  // muted grey-blue
    'operator':        { H: 200, C: 0.10 },  // cyan
    'delimiter':       { H: 200, C: 0.10 },  // cyan
    'delimiter.colon': { H:  75, C: 0.14 },  // gold
    'identifier':      { H: 210, C: 0.01 },  // near-white/near-black
};

const TOKEN_STYLES = {
    'keyword': 'bold',
    'keyword.light': 'bold',
    'comment': 'italic',
};

// Polarity state for hysteresis (prevents flickering on mid-tones)
let currentPolarity = 'light'; // 'light' = light text on dark bg

function hueDist(a, b) {
    const d = Math.abs(a - b) % 360;
    return d > 180 ? 360 - d : d;
}

function buildTokenRules(bgR, bgG, bgB) {
    const bg = rgbToOklch(bgR, bgG, bgB);

    // Determine polarity with hysteresis
    if (bg.L < 0.45) currentPolarity = 'light';
    else if (bg.L > 0.55) currentPolarity = 'dark';
    // else: keep current polarity (hysteresis band 0.45–0.55)

    const isLightText = currentPolarity === 'light';

    const rules = [];
    for (const [token, def] of Object.entries(TOKEN_DEFS)) {
        let { H, C } = def;

        // Target lightness: push away from background
        let targetL;
        if (isLightText) {
            targetL = Math.max(0.82, bg.L + 0.50);
            targetL = Math.min(1.0, targetL);
        } else {
            targetL = Math.min(0.25, bg.L - 0.45);
            targetL = Math.max(0.0, targetL);
        }

        // Comments: less contrast (secondary text)
        if (token === 'comment') {
            if (isLightText) {
                targetL = Math.max(0.60, bg.L + 0.25);
                targetL = Math.min(0.75, targetL);
            } else {
                targetL = Math.min(0.45, bg.L - 0.20);
                targetL = Math.max(0.30, targetL);
            }
        }

        // Hue conflict: if token hue is close to background hue and bg is chromatic
        if (bg.C > 0.04 && hueDist(H, bg.H) < 35) {
            // Shift hue away from background
            const shift = 50;
            const dir = ((H - bg.H + 540) % 360 - 180) > 0 ? 1 : -1;
            H = (H + shift * dir + 360) % 360;
            // Also boost chroma to compensate
            C = Math.min(0.18, C + 0.03);
        }

        // Mid-grey backgrounds: boost chroma for extra separation
        if (bg.L > 0.35 && bg.L < 0.65 && token !== 'comment' && token !== 'identifier') {
            C = Math.min(0.20, C + 0.04);
        }

        // Clamp to sRGB gamut by reducing chroma if needed
        let rgb;
        let attempts = 20;
        let c = C;
        while (attempts-- > 0) {
            rgb = oklchToRgb(targetL, c, H);
            const inGamut = rgb.every(v => v >= 0 && v <= 255);
            if (inGamut) break;
            c *= 0.85; // reduce chroma to fit gamut
        }

        const hex = rgbToHex(rgb[0], rgb[1], rgb[2]).slice(1);
        const rule = { token, foreground: hex };
        if (TOKEN_STYLES[token]) rule.fontStyle = TOKEN_STYLES[token];
        rules.push(rule);
    }

    return rules;
}

function defineSlopTheme(monaco, bgHex = '#000000') {
    const [bgR, bgG, bgB] = hexToRgb(bgHex);
    const bg = rgbToOklch(bgR, bgG, bgB);
    const isLight = bg.L > 0.50;

    // Adaptive UI colors
    const fgColor       = isLight ? '#1e1e1e' : '#c9d1d9';
    const lineNumColor  = isLight ? '#6e7681' : '#484f58';
    const lineNumActive = isLight ? '#1e1e1e' : '#c9d1d9';
    const cursorColor   = isLight ? '#0550ae' : '#58a6ff';
    const selectionBg   = isLight ? '#add6ff80' : '#264f78';
    const bracketBg     = isLight ? '#0550ae22' : '#264f7833';
    const bracketBorder = isLight ? '#0550ae' : '#58a6ff';

    monaco.editor.defineTheme('slop3d-dark', {
        base: isLight ? 'vs' : 'vs-dark',
        inherit: true,
        rules: buildTokenRules(bgR, bgG, bgB),
        colors: {
            'editor.background': bgHex,
            'editor.foreground': fgColor,
            'editor.lineHighlightBackground': '#00000000',
            'editor.lineHighlightBorder': '#00000000',
            'editor.selectionBackground': selectionBg,
            'editorLineNumber.foreground': lineNumColor,
            'editorLineNumber.activeForeground': lineNumActive,
            'editorCursor.foreground': cursorColor,
            'editorIndentGuide.background': '#00000000',
            'editorIndentGuide.activeBackground': '#00000000',
            'editorBracketMatch.background': bracketBg,
            'editorBracketMatch.border': bracketBorder,
        },
    });
}

function registerSlopScript(monaco) {
    monaco.languages.register({ id: 'slopscript', extensions: ['.slop'] });

    monaco.languages.setMonarchTokensProvider('slopscript', {
        keywords: [
            'assets', 'scene', 'update', 'spawn', 'model', 'skin',
            'if', 'elif', 'else', 'while', 'for', 'fn', 'return',
            'in', 'range', 'and', 'or', 'not', 'true', 'false',
            'goto', 'kill', 'off', 'on', 'use', 'camera',
        ],
        lights: ['ambient', 'directional', 'point', 'spot'],
        builtins: ['sin', 'cos', 'tan', 'lerp', 'clamp', 'random', 'abs', 'min', 'max'],
        globals: ['t', 'dt'],

        tokenizer: {
            root: [
                [/#.*$/, 'comment'],
                [/\b\d+\.?\d*\b/, 'number'],
                [/[a-zA-Z_]\w*/, {
                    cases: {
                        '@keywords': 'keyword',
                        '@lights': 'keyword.light',
                        '@builtins': 'predefined',
                        '@globals': 'variable.global',
                        '@default': 'identifier',
                    },
                }],
                [/[=!<>]=?/, 'operator'],
                [/[+\-*/%]/, 'operator'],
                [/:/, 'delimiter.colon'],
                [/[,.\[\]]/, 'delimiter'],
                [/\s+/, 'white'],
            ],
        },
    });

    defineSlopTheme(monaco);
}
