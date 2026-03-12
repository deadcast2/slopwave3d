const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('slop3dIDE', {
    getProjectRoot: () => ipcRenderer.invoke('get-project-root'),
    getAssetBase: () => ipcRenderer.invoke('get-asset-base'),
    openFile: () => ipcRenderer.invoke('open-file'),
    saveFile: (content) => ipcRenderer.invoke('save-file', content),
    saveFileAs: (content) => ipcRenderer.invoke('save-file-as', content),
    newFile: () => ipcRenderer.invoke('new-file'),
    setDirty: (dirty) => ipcRenderer.invoke('set-dirty', dirty),
    onMenuAction: (callback) => ipcRenderer.on('menu-action', (_e, action) => callback(action)),
});
