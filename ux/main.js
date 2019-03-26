// Licensed under the MIT license. See LICENSE file in the project root for full license information.

const electron = require('electron');

const { app, BrowserWindow } = electron;//require('electron')

function createWindow () {
    // Create the browser window.
    //let win = new BrowserWindow({ width: 600, height: 800, x: 0, y: 0, resizable: false, titleBarStyle: "hidden" })
    let win = new BrowserWindow({ width: 600, height: 800, x: 0, y: 0, resizable: false })

    // and load the index.html of the app.
    //win.loadFile('index.html');
    win.loadURL('file://' + __dirname + '/index.html');

    //win.setMenuBarVisibility(false);

    // Open the DevTools.
    //win.webContents.openDevTools();

    win.once('ready-to-show', () => {
        win.show()
    });

    // Emitted when the window is closed.
    win.on('closed', () => {
        // Dereference the window object, usually you would store windows
        // in an array if your app supports multi windows, this is the time
        // when you should delete the corresponding element.
        win = null
    });

    // When UI has finish loading
    /*win.webContents.on('did-finish-load', () => {
        // Send the timer value
        win.webContents.send('timer-change', timerTime);
    });*/
}

app.on('ready', createWindow)

// Quit when all windows are closed.
app.on('window-all-closed', () => {
    // On OS X it is common for applications and their menu bar
    // to stay active until the user quits explicitly with Cmd + Q
    if (process.platform !== 'darwin') {
        app.quit();
    }
});

app.on('activate', () => {
    // On macOS it's common to re-create a window in the app when the
    // dock icon is clicked and there are no other windows open.
    if (win === null) {
      createWindow()
    }
})