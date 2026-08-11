// Created by sebastien durand - 2016
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
//-----------------------------------------------------

#define NB      40.
#define MAX_ACC  3.
#define MAX_VEL .5
#define RESIST  .2



// Distance to a fish
float sdFish(float i, vec2 p, float a) {
    float ds, c = cos(a), s = sin(a);
    p *= 20.*mat2(c,s,-s,c); // Rotate and rescale
    p.x *= .97 + (.04+.2*p.y)*cos(i+9.*iTime);  // Swiming ondulation (+rotate in Z axes)
    ds = min(length(p-vec2(.8,0))-.45, length(p-vec2(-.14,0))-.12);   // Distance to fish
    p.y = abs(p.y)+.13;
    return max(min(length(p),length(p-vec2(.56,0)))-.3,-ds)*.05;
}


void mainImage(out vec4 cout, vec2 uv) {
    vec2 p = 1./iResolution.xy;
    float d, m = 1e6;
    vec4 c, ct, fish;

    for(float i=0.; i<NB; i++) {     
        fish = texelFetch(iChannel0,ivec2(i,0),0); // (xy = position, zw = velocity) 
        m = min(m, d = sdFish(i, fish.xy-uv.xy*p.y, atan(fish.w,fish.z))); // Draw fish according to its direction
        // Background color sum based on fish velocity (blue => red) + Halo - simple version: c*smoothstep(.5,0.,d);
        ct += mix(vec4(0,0,1,1), vec4(1,0,0,1), length(fish.zw)/MAX_VEL)*(2./(1.+3e3*d*d*d) + .5/(1.+30.*d*d)); 
    }
    // Mix fish color (white) and Halo
    cout = mix(vec4(1.),.5*sqrt(ct/NB), smoothstep(0.,p.y*1.2, m));
}