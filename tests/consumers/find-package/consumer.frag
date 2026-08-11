void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    vec3 first = texture(iChannel0, uv).rgb;
    vec3 second = texture(iChannel1, uv).rgb;
    fragColor = vec4(mix(first, second, 0.5), 1.0);
}
