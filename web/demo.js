(async function () {
    const engine = new Slop3D('game-canvas');
    await engine.init();

    const textureId = await engine.loadTexture('assets/crate.jpg');
    const meshId = await engine.loadOBJ('assets/cube.obj');

    let t = 0;
    engine.onUpdate(() => {
        t += 0.02;
        const camX = Math.sin(t) * 3;
        const camZ = Math.cos(t) * 3;
        engine.setCamera({ x: camX, y: 1.5, z: camZ }, { x: 0, y: 0, z: 0 });

        engine.drawMesh(meshId, textureId);
    });

    engine.start();
})();
