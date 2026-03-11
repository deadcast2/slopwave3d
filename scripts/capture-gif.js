const puppeteer = require('puppeteer');
const http = require('http');
const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

const ROOT = path.resolve(__dirname, '..');
const WEB_DIR = path.join(ROOT, 'web');
const JS_DIR = path.join(ROOT, 'js');
const FRAME_DIR = path.join(ROOT, 'scripts', 'frames');
const OUT_GIF = path.join(ROOT, 'web', 'assets', 'demo.gif');

const TOTAL_FRAMES = 90; // 3 seconds at 30fps
const CAPTURE_INTERVAL = 1000 / 30;

// Simple static file server
function startServer(port) {
    const mimeTypes = {
        '.html': 'text/html',
        '.js': 'application/javascript',
        '.wasm': 'application/wasm',
        '.jpg': 'image/jpeg',
        '.obj': 'text/plain',
    };

    const server = http.createServer((req, res) => {
        let filePath;
        if (req.url.startsWith('/js/')) {
            filePath = path.join(ROOT, req.url);
        } else {
            filePath = path.join(WEB_DIR, req.url === '/' ? 'index.html' : req.url);
        }

        const ext = path.extname(filePath);
        const contentType = mimeTypes[ext] || 'application/octet-stream';

        fs.readFile(filePath, (err, data) => {
            if (err) {
                res.writeHead(404);
                res.end('Not found');
                return;
            }
            res.writeHead(200, { 'Content-Type': contentType });
            res.end(data);
        });
    });

    return new Promise((resolve) => {
        server.listen(port, () => resolve(server));
    });
}

(async () => {
    const port = 8099;
    const server = await startServer(port);
    console.log(`Server running on port ${port}`);

    // Clean/create frame directory
    if (fs.existsSync(FRAME_DIR)) {
        fs.rmSync(FRAME_DIR, { recursive: true });
    }
    fs.mkdirSync(FRAME_DIR, { recursive: true });

    const browser = await puppeteer.launch({ headless: true });
    const page = await browser.newPage();
    await page.goto(`http://localhost:${port}/index.html`, {
        waitUntil: 'networkidle0',
    });

    // Wait for engine to initialize and render a few frames
    await new Promise((r) => setTimeout(r, 2000));

    console.log(`Capturing ${TOTAL_FRAMES} frames...`);

    for (let i = 0; i < TOTAL_FRAMES; i++) {
        // Capture just the canvas element
        const canvas = await page.$('#game-canvas');
        const framePath = path.join(FRAME_DIR, `frame_${String(i).padStart(4, '0')}.png`);
        await canvas.screenshot({ path: framePath });

        await new Promise((r) => setTimeout(r, CAPTURE_INTERVAL));

        if ((i + 1) % 10 === 0) {
            console.log(`  ${i + 1}/${TOTAL_FRAMES}`);
        }
    }

    await browser.close();
    server.close();

    console.log('Encoding GIF with ffmpeg...');
    execSync(
        `ffmpeg -y -framerate 30 -i "${FRAME_DIR}/frame_%04d.png" ` +
            `-vf "scale=640:480:flags=neighbor" ` +
            `-gifflags +transdiff -f gif "${OUT_GIF}"`,
        { stdio: 'inherit' }
    );

    // Clean up frames
    fs.rmSync(FRAME_DIR, { recursive: true });

    console.log(`Done! GIF saved to ${OUT_GIF}`);
})();
