(async function () {
    const engine = new Slop3D('game-canvas');
    await engine.init();

    let t = 0;
    engine.onUpdate(() => {
        t += 0.02;
        const camX = Math.sin(t) * 5;
        const camZ = Math.cos(t) * 5;
        engine.setCamera({ x: camX, y: 2, z: camZ }, { x: 0, y: 0, z: 0 });

        // Front triangle — RGB corners (CW winding from front)
        engine.drawTriangle(
            { x: 0, y: 1, z: 0, r: 1, g: 0, b: 0 },
            { x: 1, y: -1, z: 0, r: 0, g: 1, b: 0 },
            { x: -1, y: -1, z: 0, r: 0, g: 0, b: 1 }
        );

        // Back triangle — CMY corners, partially occluded
        engine.drawTriangle(
            { x: 0, y: 1.5, z: -2, r: 1, g: 1, b: 0 },
            { x: 1.5, y: -1, z: -2, r: 0, g: 1, b: 1 },
            { x: -1.5, y: -1, z: -2, r: 1, g: 0, b: 1 }
        );
    });

    engine.start();
})();
