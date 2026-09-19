#pragma once

namespace modules::plot::detail {
inline constexpr const char* sceneVertexShader = R"GLSL(#version 330 core
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.0-1.0,0,1);}
)GLSL";
inline constexpr const char* sceneFragmentShader = R"GLSL(#version 330 core
uniform samplerBuffer nodes, primitives, transfer;
uniform sampler3D volume;
uniform int nodeCount, primitiveCount, transferCount, hasVolume, perspective, clipVolume;
uniform vec2 viewport;
uniform vec3 eye, backward, rightAxis, upAxis, boundsMin, boundsMax, light;
uniform vec3 volumeMin, volumeMax;
uniform float halfSpan, nearPlane, farPlane, sampleStep, referenceStep, worldScale;
uniform vec4 background;
layout(location=0) out vec4 pixel;
layout(location=1) out float pixelDepth;
vec4 record(int p,int offset){return texelFetch(primitives,p*12+offset);}
bool box(vec3 ro,vec3 rd,vec3 low,vec3 high,inout float a,inout float b){
    for(int i=0;i<3;++i){
        if(abs(rd[i])<1e-20){if(ro[i]<low[i]||ro[i]>high[i])return false;}
        else{float x=(low[i]-ro[i])/rd[i],y=(high[i]-ro[i])/rd[i];a=max(a,min(x,y));b=min(b,max(x,y));if(a>b)return false;}
    }return true;
}
float sphere(vec3 ro,vec3 rd,vec3 c,float radius){
    vec3 oc=ro-c;float b=dot(oc,rd);vec3 closest=oc-b*rd;float h=radius*radius-dot(closest,closest);
    if(h<0.0)return -1.0;float t=-b-sqrt(h);return t>=0.0?t:-b+sqrt(h);
}
float capsule(vec3 ro,vec3 rd,vec3 a,vec3 b,float radius,out float weight){
    vec3 ba=b-a,oa=ro-a;float baba=dot(ba,ba);weight=0.0;
    if(baba<1e-20)return sphere(ro,rd,a,radius);
    float bard=dot(ba,rd),baoa=dot(ba,oa),rdoa=dot(rd,oa);
    vec3 perpendicular=rd-ba*(bard/baba),offset=oa-ba*(baoa/baba);
    float A=dot(perpendicular,perpendicular);
    if(A>1e-20){float center=-dot(perpendicular,offset)/A;vec3 closest=offset+center*perpendicular;
        float h=radius*radius-dot(closest,closest);
        if(h>=0.0){float t=center-sqrt(h/A),y=baoa+t*bard;if(t>=0.0&&y>=0.0&&y<=baba){weight=y/baba;return t;}}}
    float ta=sphere(ro,rd,a,radius),tb=sphere(ro,rd,b,radius);
    if(ta>=0.0&&(tb<0.0||ta<=tb))return ta;weight=1.0;return tb;
}
vec4 colormap(float t,int map){
    if(map==0)return vec4(0.2,0.65,1,1);
    if(map==1)return vec4(t,t,t,1);
    if(map==3)return mix(vec4(0.05,0.03,0.53,1),vec4(0.94,0.98,0.13,1),t);
    if(map==4)return mix(vec4(0.19,0.07,0.23,1),vec4(0.48,0.02,0.01,1),t);
    return mix(vec4(0.27,0,0.33,1),vec4(0.99,0.91,0.14,1),t);
}
bool intersect(int p,vec3 ro,vec3 rd,float low,float high,float previous,int previousObject,
               out float t,out int object,out vec4 color){
    vec4 a=record(p,0),b=record(p,1),c=record(p,2);
    object=int(c.w);vec3 weights=vec3(1,0,0),normal=vec3(0,0,1);t=-1.0;
    if(a.w<0.5){
        vec3 e1=b.xyz-a.xyz,e2=c.xyz-a.xyz,h=cross(rd,e2);float det=dot(e1,h);
        float epsilon=length(e1)*length(e2)*1e-7;
        if(abs(det)<=epsilon||(record(p,5).w>0.5&&det<=epsilon))return false;
        vec3 s=ro-a.xyz;float u=dot(s,h)/det;if(u<-1e-6||u>1.0+1e-6)return false;
        vec3 q=cross(s,e1);float v=dot(rd,q)/det;if(v<-1e-6||u+v>1.0+1e-6)return false;
        t=dot(e2,q)/det;weights=max(vec3(1.0-u-v,u,v),vec3(0));weights/=dot(weights,vec3(1));
        normal=record(p,3).xyz*weights.x+record(p,4).xyz*weights.y+record(p,5).xyz*weights.z;
        if(dot(normal,normal)<1e-20)normal=cross(e1,e2);normal=normalize(normal);
    }else if(a.w<1.5){
        float w;t=capsule(ro,rd,a.xyz,b.xyz,b.w,w);weights=vec3(1.0-w,w,0);
        normal=normalize(ro+rd*t-mix(a.xyz,b.xyz,w));
    }else{t=sphere(ro,rd,a.xyz,b.w);normal=normalize(ro+rd*t-a.xyz);}
    if(t<low||t>high)return false;
    // Float depth equality deduplicates shared edges. Different coplanar objects retain scene order.
    float epsilon=2e-6*max(1.0,abs(t));
    if(t<previous-epsilon||(t<=previous+epsilon&&object<=previousObject))return false;
    vec4 config=record(p,10);vec3 world=ro+rd*t;
    if(config.w>0.5&&(any(lessThan(world,boundsMin))||any(greaterThan(world,boundsMax))))return false;
    color=record(p,6)*weights.x+record(p,7)*weights.y+record(p,8)*weights.z;
    vec4 scalar=record(p,9);
    if(scalar.w>0.5){
        float f=0.0;bool missing=false;
        for(int i=0;i<3;++i)if(weights[i]>0.0){if(isnan(scalar[i])||isinf(scalar[i]))missing=true;else f+=scalar[i]*weights[i];}
        if(config.z>=0.0){float raw=mix(config.z,1.0,f);if(raw<=0.0)missing=true;else f=1.0-log(raw)/log(config.z);}
        if(missing)color=record(p,11);
        else{f=clamp(f,0.0,1.0);if(config.y>1.0)f=round(f*(config.y-1.0))/(config.y-1.0);color*=colormap(f,int(config.x));}
    }
    if(color.a<=0.0)return false;
    if(record(p,3).w>0.5){float ambient=record(p,4).w;color.rgb*=ambient+(1.0-ambient)*abs(dot(normal,light));}
    return true;
}
bool nearest(vec3 ro,vec3 rd,float low,float high,float previous,int previousObject,
             out float distance,out int object,out vec4 color){
    distance=high;object=2147483647;bool found=false;
    if(nodeCount==0)return false;
    int stack[64];int size=1;stack[0]=0;
    while(size>0){int index=stack[--size];float a=low,b=distance;
        if(!box(ro,rd,texelFetch(nodes,index*3).xyz,texelFetch(nodes,index*3+1).xyz,a,b))continue;
        ivec4 meta=ivec4(texelFetch(nodes,index*3+2));
        if(meta.y>0){for(int i=0;i<meta.y;++i){float t;int obj;vec4 c;
            if(intersect(meta.x+i,ro,rd,low,distance,previous,previousObject,t,obj,c)&&
               (!found||t<distance||(t==distance&&obj<object))){found=true;distance=t;object=obj;color=c;}
        }}else{stack[size++]=meta.z;stack[size++]=meta.w;}
    }return found;
}
vec4 transferColor(float v){
    vec4 first=texelFetch(transfer,0),last=texelFetch(transfer,(transferCount-1)*2);
    if(v<=first.x)return texelFetch(transfer,1);
    if(v>=last.x)return texelFetch(transfer,(transferCount-1)*2+1);
    int a=0,b=transferCount-1;
    while(b-a>1){int m=(a+b)/2;if(texelFetch(transfer,m*2).x<=v)a=m;else b=m;}
    float low=texelFetch(transfer,a*2).x,high=texelFetch(transfer,b*2).x;
    return mix(texelFetch(transfer,a*2+1),texelFetch(transfer,b*2+1),(v-low)/(high-low));
}
void blend(vec4 c,float t,inout vec3 rgb,inout float trans,inout float depth){
    if(c.a>0.0&&depth<0.0)depth=t;rgb+=trans*c.a*c.rgb;trans*=1.0-c.a;
}
void integrate(vec3 ro,vec3 rd,float start,float end,inout vec3 rgb,inout float trans,inout float depth){
    if(hasVolume==0||trans<=0.0)return;
    if(!box(ro,rd,volumeMin,volumeMax,start,end)||start>=end)return;
    if(clipVolume!=0&&(!box(ro,rd,boundsMin,boundsMax,start,end)||start>=end))return;
    int count=int(ceil((end-start)/sampleStep));float step=(end-start)/float(count);
    vec3 dims=vec3(textureSize(volume,0));
    for(int i=0;i<count&&trans>0.0;++i){float t=start+(float(i)+0.5)*step;
        vec3 unit=(ro+rd*t-volumeMin)/(volumeMax-volumeMin);
        vec2 v=texture(volume,(unit*(dims-1.0)+0.5)/dims).rg;
        if(v.y<0.999999)continue;
        vec4 c=transferColor(v.x);c.a=1.0-pow(1.0-c.a,step/referenceStep);blend(c,t,rgb,trans,depth);
    }
}
void main(){
    vec2 uv=vec2((2.0*gl_FragCoord.x/viewport.x-1.0)*viewport.x/viewport.y,1.0-2.0*gl_FragCoord.y/viewport.y);
    vec3 ro=eye,rd=-backward;
    if(perspective==0)ro+=(rightAxis*uv.x+upAxis*uv.y)*halfSpan;
    else rd=normalize((rightAxis*uv.x+upAxis*uv.y)*halfSpan-backward);
    float cosine=-dot(rd,backward),low=nearPlane/cosine,high=farPlane/cosine;
    vec3 rgb=vec3(0);float trans=1.0,depth=-1.0,previous=low-0.0001;int prevObject=-1;
    for(int i=0;i<=primitiveCount;++i){float t;int obj;vec4 c;
        bool hit=nearest(ro,rd,low,high,previous,prevObject,t,obj,c);
        integrate(ro,rd,max(low,previous),hit?t:high,rgb,trans,depth);
        if(!hit)break;blend(c,t,rgb,trans,depth);if(trans<=0.0)break;previous=t;prevObject=obj;
    }
    float alpha=1.0-trans+trans*background.a;rgb+=trans*background.a*background.rgb;
    pixel=vec4(alpha>0.0?rgb/alpha:vec3(0),alpha);pixelDepth=depth<0.0?3.402823e38:depth*cosine*worldScale;
}
)GLSL";
} // namespace modules::plot::detail
