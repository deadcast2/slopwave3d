// Monaco language definition for SlopScript
// Called after Monaco AMD modules are loaded

function registerSlopScript(monaco) {
    monaco.languages.register({ id: 'slopscript', extensions: ['.slop'] });

    monaco.languages.setMonarchTokensProvider('slopscript', {
        keywords: [
            'assets', 'scene', 'update', 'spawn', 'model', 'skin',
            'if', 'elif', 'else', 'while', 'for', 'fn', 'return',
            'in', 'range', 'and', 'or', 'not', 'true', 'false',
            'goto', 'kill', 'off', 'on',
        ],
        lights: ['ambient', 'directional', 'point', 'spot'],
        builtins: ['sin', 'cos', 'tan', 'lerp', 'clamp', 'random', 'abs', 'min', 'max'],
        globals: ['camera', 't', 'dt'],

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

    monaco.editor.defineTheme('slop3d-dark', {
        base: 'vs-dark',
        inherit: true,
        rules: [
            { token: 'keyword', foreground: 'c792ea', fontStyle: 'bold' },
            { token: 'keyword.light', foreground: 'ffcb6b', fontStyle: 'bold' },
            { token: 'predefined', foreground: '82aaff' },
            { token: 'variable.global', foreground: 'f07178' },
            { token: 'number', foreground: 'f78c6c' },
            { token: 'comment', foreground: '546e7a', fontStyle: 'italic' },
            { token: 'operator', foreground: '89ddff' },
            { token: 'delimiter', foreground: '89ddff' },
            { token: 'delimiter.colon', foreground: 'ffcb6b' },
            { token: 'identifier', foreground: 'eeffff' },
        ],
        colors: {
            'editor.background': '#0d1117',
            'editor.foreground': '#c9d1d9',
            'editor.lineHighlightBackground': '#161b22',
            'editor.selectionBackground': '#264f78',
            'editorLineNumber.foreground': '#484f58',
            'editorLineNumber.activeForeground': '#c9d1d9',
            'editorCursor.foreground': '#58a6ff',
            'editorIndentGuide.background': '#21262d',
            'editorIndentGuide.activeBackground': '#30363d',
            'editorBracketMatch.background': '#264f7833',
            'editorBracketMatch.border': '#58a6ff',
        },
    });
}
