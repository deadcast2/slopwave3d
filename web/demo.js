(async function() {
    const engine = new Slop3D('game-canvas');
    await engine.init();

    let t = 0;
    engine.onUpdate(() => {
        t += 0.02;
        const r = Math.floor(Math.sin(t) * 127 + 128);
        const g = Math.floor(Math.sin(t + 2.094) * 127 + 128);
        const b = Math.floor(Math.sin(t + 4.189) * 127 + 128);
        engine.setClearColor(r, g, b);
    });

    engine.start();
})();
