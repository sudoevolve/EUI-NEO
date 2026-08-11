void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    vec4 imageSample = texture(iChannel0, uv);
    vec4 bufferSample = texture(iChannel1, uv);
    vec4 previous = texture(iChannel2, uv);
    vec4 emptySample = texture(iChannel3, uv);
    fragColor = vec4(
        bufferSample.r,
        previous.g + 0.1,
        emptySample.r,
        iChannelResolution[0].x / 360.0
    );
    fragColor.rgb += imageSample.rgb * 0.0;
}
