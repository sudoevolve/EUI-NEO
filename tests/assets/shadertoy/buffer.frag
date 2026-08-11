void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    fragColor = vec4(uValue, fragCoord.x / iResolution.x, iTime, 1.0);
}
