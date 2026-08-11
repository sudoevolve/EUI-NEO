// httpswww.shadertoy.comviewldd3DB
// Created by sebastien durand - 2016
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
//-----------------------------------------------------

#define NB      40.
#define MAX_ACC  3.
#define MAX_VEL .5
#define RESIST  .2



vec2 hash(float n) { return fract(sin(vec2(n,n*7.))*43758.5); }
vec4 Fish(float i) { return texelFetch(iChannel0, ivec2(i,0),0);}

void mainImage(out vec4 fc, vec2 uv) {   
    if(uv.y > .5 || uv.x > NB) discard;
    
    vec2  w, vel, acc, sumF, R = iResolution.xy, res = R/R.y;
    float d, a, v, dt = .03, id = floor(uv.x);  
    
// = Initialization ===================================
    if (iFrame < 5) fc = vec4(.1+.8*hash(id)*res,0,0);
            
// = Animation step ===================================
    else { 
        vec4 fish = Fish(id);
        
// - Sum Forces -----------------------------  
	// Borders action
        sumF = .8*(1./abs(fish.xy) - 1./abs(res-fish.xy));         
        
    // Mouse action        
        bool mouseDown = iMouse.w >= 0.0;
        vec2 mPos = mouseDown ? iMouse.xy : -iMouse.zw;
        w = fish.xy - mPos/iResolution.y;
        sumF += normalize(w)*.65/dot(w,w);

    // Calculate repulsion force with other fishs
        for(float i=0.;i<NB;i++)
            if (i != id) {                                            // only other fishs
                d = length(w = fish.xy - Fish(i).xy);
    			sumF -= d > 0. ? w*(6.3+log(d*d*.02))/exp(d*d*2.4)/d  // attractive/repulsive force from otehrs
                    : .01*hash(id);                                   // if same pos : small ramdom force
            }
    // Friction    
        sumF -= fish.zw*RESIST/dt;
        
// - Dynamic calculation ---------------------     
    // Calculate acceleration A = (1/m * sumF) [cool m=1. here!]
        a = length(acc = sumF); 
        acc *= a>MAX_ACC ? MAX_ACC/a : 1.; // limit acceleration
    // Calculate speed
        v = length(vel = fish.zw + acc*dt);
        vel *= v>MAX_VEL ? MAX_VEL/v : 1.; // limit velocity
// - Save position and velocity of fish (xy = position, zw = velocity) 
        fc = vec4(fish.xy + vel*dt, vel);  
    }
}