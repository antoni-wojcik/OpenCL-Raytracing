#version 410 core

in vec2 fragPos;
out vec4 FragColor;

uniform vec3 origin;
uniform vec3 camera_llc; // camera's lower left corner position
uniform vec3 horizontal;
uniform vec3 vertical;

uniform float time;

const float MIN_DISTANCE = 0.001;
const float MAX_DISTANCE = 10000.0;
const int DEPTH = 10;
const int NUM_OBJECTS = 6;

struct Ray {
    vec3 start; //starting location
    vec3 dir; //has to be normalized
};

vec3 ray_point_at_parameter(in Ray r, float parameter) {
    return r.start + r.dir*parameter;
}

Ray gen_initial_ray(in vec3 start, float s, float t) {
    Ray r;
    r.start = start;
    r.dir = normalize(camera_llc + s*horizontal + t*vertical);
    return r;
}

bool refract_ray(in vec3 dir, in vec3 normal, float index_ratio, inout vec3 refracted) {
    float csi = dot(dir, normal); //cos_angle_incident
    float discriminant = 1.0 - index_ratio*index_ratio*(1-csi*csi);
    if(discriminant > 0) {
        refracted = index_ratio*(dir - normal*csi) - normal*sqrt(discriminant);
        return true;
    } else return false;
}

struct Material {
    int type;
    vec3 color;
    float extra_data;
};

struct HitPointInfo {
    float t;
    vec3 p;
    vec3 normal;
    Material mat;
};

struct Sphere {
    vec3 center;
    float radius;
    Material mat;
};

/*
 enum ObjectType = { Sphere, Plane }
 
 struct Object {
 ObjectType type;
 Sphere sphere;
 Plane plane;
 }
 //Generalize objects this way (heavy effect on memeory)
 */

bool sphere_hit(in Ray r, inout HitPointInfo hpi, in Sphere s) {
    vec3 oc = r.start - s.center;
    float a = dot(r.dir, r.dir);
    float b = dot(oc, r.dir);
    float c = dot(oc, oc) - s.radius*s.radius;
    float discriminant = b*b - a*c;
    if (discriminant > 0) {
        float temp = (-b - sqrt(b*b-a*c))/(a);
        if (temp < MAX_DISTANCE && temp > MIN_DISTANCE) {
            hpi.t = temp;
            hpi.p = ray_point_at_parameter(r, temp);
            hpi.normal = (hpi.p - s.center)/s.radius;
            hpi.mat = s.mat;
            return true;
        }
    }
    return false;
}

bool multiple_sphere_hit(in Ray r, inout HitPointInfo hpi, in Sphere spheres[NUM_OBJECTS]) {
    bool hit_anything = false;
    float closest_hit = MAX_DISTANCE;
    HitPointInfo hpi_result;
    for(int i = 0; i < NUM_OBJECTS; i++) {
        if(sphere_hit(r, hpi, spheres[i])) {
            hit_anything = true;
            if(hpi.t < closest_hit) {
                hpi_result = hpi;
                closest_hit = hpi.t;
            }
        }
    }
    hpi = hpi_result;
    return hit_anything;
}

float rand(in vec2 seed){
    return fract(sin(dot(seed.xy, vec2(12.9898,78.233))) * 43758.5453);
}

vec3 random_in_unit_sphere(in vec3 normal) { // TODO: fix! it's not entirely correct!
    vec3 p;
    //do {
    p = normalize(vec3(rand(normal.xy), rand(normal.yz), rand(normal.xz)));
    //} while (dot(p, p) >= 1.0);
    return p;
}

bool scatter(inout Ray r, in HitPointInfo hpi, inout vec3 color) {
    color = hpi.mat.color;
    r.start = hpi.p;
    if(hpi.mat.type == 0) { // reflect
        //color = (hpi.normal*0.5+0.5);
        r.dir = reflect(r.dir, hpi.normal);
    } else if(hpi.mat.type == 1) { // diffuse
        r.dir = hpi.normal+mix(random_in_unit_sphere(hpi.normal),random_in_unit_sphere(r.dir),0.5);
    } else if(hpi.mat.type == 2) { // refract
        vec3 resulting_dir;
        vec3 normal;
        float index;
        if(dot(r.dir, hpi.normal) > 0) {
            normal = -hpi.normal;
            index = hpi.mat.extra_data;
        } else {
            normal = hpi.normal;
            index = 1.0/hpi.mat.extra_data;
        }
        if(refract_ray(r.dir, normal, index, resulting_dir)) r.dir = resulting_dir;
        else r.dir = reflect(r.dir, normal);
    } else return false;
    return true;
}

vec3 get_color(in Ray r, in Sphere s[NUM_OBJECTS]) {
    vec3 col = vec3(0);
    for(int i = 0; i < DEPTH; i++) {
        vec3 iteration_color;
        HitPointInfo hit;
        if(multiple_sphere_hit(r, hit, s)) {
            if(scatter(r, hit, iteration_color)) {
                if(i > 0) col *= iteration_color; //col = mix(col, iteration_color, 0.5); iteration_color;
                else col = iteration_color;
            } else {
                if(i > 0) col = mix(col, vec3(0), 0.5);
                break;
            }
        } else {
            float t = 0.5*(r.dir.y + 1.0);
            if(i > 0) col *= mix(vec3(1.0, 1.0, 1.0), vec3(0.5, 0.7, 1.0), t);
            else col = mix(vec3(1.0, 1.0, 1.0), vec3(0.5, 0.7, 1.0), t);
            break;
        }
    }
    return col;
}

void main() {
    Sphere s[NUM_OBJECTS];
    s[0].center = vec3(0);
    s[0].radius = 1;
    s[0].mat.type = 2;
    s[0].mat.extra_data = 11+10*sin(time);
    s[0].mat.color = vec3(1,1,1);
    s[1].center = vec3(2+sin(time), 2, 2+cos(time));
    s[1].radius = 1;
    s[1].mat.type = 0;
    s[1].mat.color = vec3(1,0,0);
    s[2].center = vec3(-2);
    s[2].radius = 1.5;
    s[2].mat.type = 2;
    s[2].mat.extra_data = 1.4;
    s[2].mat.color = vec3(1.0);
    s[3].center = vec3(-5);
    s[3].radius = 2;
    s[3].mat.type = 0;
    s[3].mat.color = vec3(1.0);
    s[4].center = vec3(0, -100, 0);
    s[4].radius = 90;
    s[4].mat.type = 1;
    s[4].mat.color = vec3(0, 1, 0);
    s[5].center = vec3(0, -7, 0);
    s[5].radius = 3;
    s[5].mat.type = 1;
    s[5].mat.color = vec3(0.5, 0, 0.5);

    vec3 col = get_color(gen_initial_ray(origin, fragPos.x, fragPos.y), s);
    FragColor = vec4(col, 1.0);
}
