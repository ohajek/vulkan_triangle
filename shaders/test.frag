#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec4 outColor;

float map(vec3 p) { 
	vec3 q = fract(p) *  2.f - 1.f;
    return length(q) - 0.22f; //minus radius
}

float trace(vec3 o, vec3 r) {
 	float t = 0.0f;
    for(int i =0; i < 32; i++) {
    	vec3 p = o + r * t;
        float d = map(p);
        t += d * 0.5f;
    }
    return t;
}




void main() {

    vec2 uv = inUV;
	uv.y = ((uv.y - 1) / 2.f) + 0.5f;
	uv.x *= 1280/720;
	vec2 resolution = vec2(1280, 720);
	
    vec3 r = normalize(vec3(uv, 1.0));
    vec3 o = vec3(0.0f, 0.0f, -3.0f);
    float t = trace(o, r);
    float fog = 1.f / (1.0f + t *t  * 0.1f);
    vec3 fc = vec3(fog);
	

	outColor = vec4( fc, 1.0 );
}