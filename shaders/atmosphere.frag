#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 inUV;
layout(location = 2) in float inTime;

layout(location = 0) out vec4 outColor;


/*
* Constants
*/
const float INFINITY = 1e20f;
const float PI = 3.14159265359;
const float RADIUS_P = 1.0f;
const float RADIUS_A = RADIUS_P + 0.8f;
const int NUM_IN_SCATTER = 25;
const int NUM_OUT_SCATTER = 80;

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
    
    //*
    float a = (1.0f - gg) + (1.0f + cos_angle2);
    float b = (2.0f + gg) * (1.0f + gg - 2.0f * g * cos_angle);
    b = pow(b, 3.0f / 2.0f);

    return (3.0f / (8.0f * PI)) * (a / b);
    //*

    /*
    float a = ( 1.0 - gg ) * ( 1.0 + cos_angle2 );

	float b = 1.0 + gg - 2.0 * g * cos_angle;
	b *= sqrt( b );
	b *= 2.0 + gg;	
	
	return ( 3.0 / 8.0 / PI ) * a / b;
    //*/
}

float phase_rayleigh(float cos_angle) {
    float cos_angle2 = cos_angle * cos_angle;
    return (3.0f / 16.0f / PI) * (1.0f + cos_angle2);
}

float density(vec3 position, float ph) {
    return exp( -max( length(position) - RADIUS_P, 0.0f) / ph);
}

float optic(vec3 p, vec3 q, float ph) {
    vec3 s = ( q - p ) / float(NUM_OUT_SCATTER);
    vec3 v = p + s * 0.5f;

    float sum = 0.0f;
    for (int i = 0; i < NUM_OUT_SCATTER; i++ ) {
        sum += density(v, ph);
        v += s;
    }
    sum *= length(s);

    return sum;
}

vec3 scattering_function(vec3 position, vec3 direction, vec3 sun, vec2 roots) {
	const float ph_rayleigh = 0.05f;
	const float ph_mie = 0.02f;
	const vec3 coef_rayleigh = vec3(3.8f, 13.5f, 33.1f);
	//const vec3 coef_rayleigh = vec3(5.5f, 13.0f, 22.4f);
	const vec3 coef_mie = vec3(21.0f);
	const float mie_ex = 1.1f;

	vec3 sum_ray = vec3(0.0f);
	vec3 sum_mie = vec3(0.0f);

    float n_ray0 = 0.0f;
    float n_mie0 = 0.0f;

    float len = ( roots.y - roots.x) / float(NUM_IN_SCATTER);
    vec3 s = direction * len;
    vec3 v = position + direction * (roots.x + len * 0.5);

    for (int i = 0; i < NUM_IN_SCATTER; i++, v += s) {
        float d_ray = density(v, ph_rayleigh) * len;
        float d_mie = density(v, ph_mie) * len;

        n_ray0 += d_ray;
        n_mie0 += d_mie;

        vec2 f = sphere_intersect(v, sun, RADIUS_A);
        vec3 u = v + sun * f.y;

        float n_ray1 = optic(v, u, ph_rayleigh);
        float n_mie1 = optic(v, u, ph_mie);

        vec3 att = exp(- (n_ray0 + n_ray1) * coef_rayleigh - (n_mie0 + n_mie1) * coef_mie * mie_ex);

        sum_ray += d_ray * att;
        sum_mie += d_mie * att;
    }

    float c = dot(direction, -sun);
    float cc = c * c;
    vec3 scatter = sum_ray * coef_rayleigh * phase_rayleigh(cc) + 
					sum_mie * coef_mie * phase_mie(-0.78, c);

    return 10.0f * scatter;
}

mat3 rot3xy( vec2 angle ) {
	vec2 c = cos( angle );
	vec2 s = sin( angle );
	
	return mat3(
		c.y      ,  0.0, -s.y,
		s.y * s.x,  c.x,  c.y * s.x,
		s.y * c.x, -s.x,  c.y * c.x
	);
}

void main() {
    //Some basic setup
	vec2 resolution = vec2(1280.0f, 720.0f);
    vec3 fragColor = vec3(0.0f);
    vec2 uv = inUV;
	uv.y = ((uv.y - 1.0f) / 2.0f) + 0.5f;
	
    //Atmosphere scattering
    vec3 dir = normalize(vec3(uv, 1.0f));
    vec3 camera = vec3(0.0f, 0.0f, -3.0f);
	vec3 sun_dir = vec3(0.0f, 0.0f, 1.0f);
    
    //Rotation
    mat3 rot = rot3xy( vec2( 0.0, inTime * 0.5 ) );
    dir = rot * dir;
    camera = rot * camera;

    vec2 roots_outer = sphere_intersect(camera, dir, RADIUS_A);
    if(roots_outer.x > roots_outer.y) {
        outColor = vec4(fragColor, 1.0 );
        return;
    }
	vec2 roots_inner = sphere_intersect(camera, dir, RADIUS_P);
	roots_outer.y = min(roots_outer.y, roots_inner.x);

    vec3 I = scattering_function(camera, dir, sun_dir, roots_outer);

	outColor = vec4( pow( I, vec3(1.0 / 2.2) ), 1.0 );
}