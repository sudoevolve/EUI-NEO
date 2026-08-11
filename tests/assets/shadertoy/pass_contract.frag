void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    fragColor = vec4(
        iResolution.x / 16.0,
        iResolution.y / 16.0,
        iMouse.x / max(iResolution.x, 1.0),
        iChannelResolution[0].x / 16.0);
}