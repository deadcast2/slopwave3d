const { app, BrowserWindow, Menu, dialog, ipcMain } = require('electron');
const path = require('path');
const fs = require('fs');

const PROJECT_ROOT = path.resolve(__dirname, '..');

let mainWindow;
let currentFilePath = null;
let isDirty = false;

function updateTitle() {
    const name = currentFilePath ? path.basename(currentFilePath) : 'untitled.slop';
    const dirty = isDirty ? '*' : '';
    mainWindow.setTitle(`${name}${dirty} — slop3d IDE`);
}

function createWindow() {
    mainWindow = new BrowserWindow({
        width: 1400,
        height: 850,
        minWidth: 800,
        minHeight: 500,
        backgroundColor: '#000000',
        titleBarStyle: 'hiddenInset',
        trafficLightPosition: { x: 12, y: 12 },
        webPreferences: {
            preload: path.join(__dirname, 'preload.js'),
            contextIsolation: true,
            nodeIntegration: false,
            webSecurity: false,
        },
    });

    mainWindow.loadFile(path.join(__dirname, 'renderer', 'index.html'));
    updateTitle();
}

// --- Menu ---

function buildMenu() {
    const isMac = process.platform === 'darwin';
    const template = [
        ...(isMac
            ? [{ label: 'Slopwave3D', submenu: [{ role: 'about' }, { type: 'separator' }, { role: 'quit' }] }]
            : []),
        {
            label: 'File',
            submenu: [
                { label: 'New', accelerator: 'CmdOrCtrl+N', click: () => mainWindow.webContents.send('menu-action', 'new') },
                { label: 'Open...', accelerator: 'CmdOrCtrl+O', click: () => mainWindow.webContents.send('menu-action', 'open') },
                { type: 'separator' },
                { label: 'Save', accelerator: 'CmdOrCtrl+S', click: () => mainWindow.webContents.send('menu-action', 'save') },
                { label: 'Save As...', accelerator: 'CmdOrCtrl+Shift+S', click: () => mainWindow.webContents.send('menu-action', 'save-as') },
                ...(!isMac ? [{ type: 'separator' }, { role: 'quit' }] : []),
            ],
        },
        {
            label: 'Edit',
            submenu: [{ role: 'undo' }, { role: 'redo' }, { type: 'separator' }, { role: 'cut' }, { role: 'copy' }, { role: 'paste' }, { role: 'selectAll' }],
        },
        {
            label: 'View',
            submenu: [
                { label: 'Toggle Preview', accelerator: 'CmdOrCtrl+P', click: () => mainWindow.webContents.send('menu-action', 'toggle-preview') },
                { type: 'separator' },
                { role: 'toggleDevTools' },
                { type: 'separator' },
                { role: 'resetZoom' },
                { role: 'zoomIn' },
                { role: 'zoomOut' },
            ],
        },
    ];
    Menu.setApplicationMenu(Menu.buildFromTemplate(template));
}

// --- IPC Handlers ---

ipcMain.handle('get-project-root', () => PROJECT_ROOT);

ipcMain.handle('get-asset-base', () => {
    if (currentFilePath) return path.dirname(currentFilePath);
    return path.join(PROJECT_ROOT, 'web');
});

ipcMain.handle('open-file', async () => {
    const result = await dialog.showOpenDialog(mainWindow, {
        filters: [{ name: 'SlopScript', extensions: ['slop'] }],
        properties: ['openFile'],
    });
    if (result.canceled || !result.filePaths.length) return null;
    const filePath = result.filePaths[0];
    const content = fs.readFileSync(filePath, 'utf-8');
    currentFilePath = filePath;
    isDirty = false;
    updateTitle();
    return { path: filePath, content };
});

ipcMain.handle('save-file', async (_e, content) => {
    if (!currentFilePath) {
        return await saveFileAs(content);
    }
    fs.writeFileSync(currentFilePath, content, 'utf-8');
    isDirty = false;
    updateTitle();
    return { path: currentFilePath };
});

ipcMain.handle('save-file-as', async (_e, content) => {
    return await saveFileAs(content);
});

async function saveFileAs(content) {
    const result = await dialog.showSaveDialog(mainWindow, {
        filters: [{ name: 'SlopScript', extensions: ['slop'] }],
        defaultPath: currentFilePath || 'untitled.slop',
    });
    if (result.canceled) return null;
    fs.writeFileSync(result.filePath, content, 'utf-8');
    currentFilePath = result.filePath;
    isDirty = false;
    updateTitle();
    return { path: result.filePath };
}

ipcMain.handle('new-file', () => {
    currentFilePath = null;
    isDirty = false;
    updateTitle();
    return true;
});

ipcMain.handle('set-dirty', (_e, dirty) => {
    isDirty = dirty;
    updateTitle();
});

// --- App lifecycle ---

app.whenReady().then(() => {
    buildMenu();
    createWindow();
});

app.on('window-all-closed', () => app.quit());
