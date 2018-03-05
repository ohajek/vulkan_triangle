#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec4 outColor;


/*
* Constants
*/
const float INFINITY = 1e20f;
const float PI = 3.14159265359;
const float RADIUS_P = 1.0f;
const float RADIUS_A = RADIUS_P + 0.5f;


vec2 sphere_intersect(vec3 position, vec3 direction, float r) {
    float b = dot(position, direction);
    float c = dot(position, position) - r * r;
    float d = b * b - c;

    if(d < 0.0f) {
        return vec2(INFINITY, -INFINITY);
    }
    d = sqrt(d);

    return vec2(-b -d, -b + d);
}

float phase_mie(float g, float cos_angle) {
    float cos_angle2 = cos_angle * cos_angle;
    float gg = g * g;

    float a = (3 * (1 - gg)) / (2 * (2 + gg));
    float b = (1 + cos_angle2) / (1 + gg - 2 * g * cos_angle);
    return a * b;
}

float phase_raleigh(float cos_angle) {
    float cos_angle2 = cos_angle * cos_angle;
    return (3.0f / 16.0f / PI) * (1.0 + cos_angle2);
}


vec3 scattering_function(vec3 position, vec3 direction, vec3 sun, vec3 roots) {
	const float phase_rayleigh = 0.05f;
	const float phase_mie = 0.02f;
	const vec3 coef_rayleigh = vec3(3.8f, 13.5f, 33.1f);
	const vec3 coef_mie = vec3(21.f);
	const float mie_ex = 1.1;

	vec3 sum_ray = vec3(0.0f);
	vec3 sum_mie = vec3(0.0f);
}


void main() {
    //Some basic setup
	vec2 resolution = vec2(1280, 720);
    vec3 fragColor = vec3(0.0f);
    vec2 uv = inUV;
	uv.y = ((uv.y - 1) / 2.f) + 0.5f;
	
    //Atmosphere scattering
    vec3 dir = normalize(vec3(uv, 1.0f));
    vec3 camera = vec3(0.0f, 0.0f, -3.0f);
	vec3 sun_dir = vec3(0.0f, 0.0f, -1.0f);
    
    vec2 roots_outer = sphere_intersect(camera, dir, RADIUS_A);
    if(roots.x > roots.y) {
		fragColor = vec3(1.0f, 0.0f, 0.0f);
    }

	vec2 root_inner = sphere_intersect(camera, dir, RADIUS_P);
	//roots_outer.y = min(roots_outer.y, roots_inner.x);


	outColor = vec4(fragColor, 1.0 );
}