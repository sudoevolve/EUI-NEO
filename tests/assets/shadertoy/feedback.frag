void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec4 previous = texture(iChannel0, fragCoord / iResolution.xy);
    fragColor = vec4(previous.r + 0.1, float(iFrame) / 10.0, 0.25, 1.0);
}
