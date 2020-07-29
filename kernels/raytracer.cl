#define TRIANGLE_EPSILON 0.0000001f

#define MIN_DISTANCE 0.001f
#define MAX_DISTANCE 1000.0f
#define DEPTH 10
#define NUM_MATERIALS 8
#define NUM_SPHERES 7
#define NUM_PLANES 1
#define NUM_LENSES 1
#define NUM_MESHES 0

#define RANDOM_BUFFER_SIZE 100000

typedef float3 vec3;
typedef float3 col;

__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE | CLK_FILTER_NEAREST;

typedef struct {
    vec3 origin; //starting location
    vec3 dir; //has to be normalized
    float param; //has to be set to 0 when initalized
} Ray;

typedef enum { t_refractive, t_reflective, t_dielectric, t_diffuse, t_light } MatType;

typedef struct {
    MatType type;
    col color;
    float extra_data; // refractive index etc
} Material;

typedef struct {
    float t;
    vec3 p;
    vec3 normal;
    __global Material* mat;
} HPI; //HitPointInfo

typedef struct {
    vec3 pos;
    float r;
    __global Material* mat;
} Sphere;

typedef struct {
    vec3 pos;
    vec3 normal;
    __global Material* mat;
} Plane;

typedef struct {
    vec3 pos; // pos of the centre
    vec3 p1; // both curvatures
    vec3 p2;
    float r1;
    float r2;
    __global Material* mat;
} Lens;

inline Lens createLens(vec3 pos, vec3 normal, float r1, float r2, float h, __global Material* mat) {
    //if(r1 < h && r2 < h) abort();
    Lens lens;
    lens.pos = pos;
    lens.p1 = pos + normal * sqrt(r1 * r1 - h * h);;
    lens.p2 = pos - normal * sqrt(r2 * r2 - h * h);;
    lens.r1 = r1;
    lens.r2 = r2;
    lens.mat = mat;
    
    return lens;
}

typedef struct {
    int size;
    int index_anchor;
    __global vec3* vertex_buffer;
    __global Material* mat;
} Mesh;

inline __global vec3* getMeshVertex(__global const Mesh* mesh, int i) {
    return mesh->vertex_buffer + mesh->index_anchor + i;
}

typedef struct {
    Material materials[NUM_MATERIALS];
    
    Sphere spheres[NUM_SPHERES];
    Plane planes[NUM_PLANES];
    Lens lenses[NUM_LENSES];
    Mesh meshes[NUM_MESHES];
} Scene;

inline vec3 getVec(__global const float* buff, int id) {
    return (vec3)(buff[id], buff[id + 1], buff[id + 2]);
}

inline vec3 randomVec(__global const float* random_buffer, const Ray* r, int s_seed) {
    int rand_val = (int)(dot(r->dir, (vec3)(123.9898, 758.233, 433.3314)) * 44378.5453);
    int seed = ((rand_val + s_seed * 2683 + get_global_id(0) * 3931 + get_global_id(1)) * 3) % RANDOM_BUFFER_SIZE;
    
    return getVec(random_buffer, seed);
}

inline float random(__global const float* random_buffer, const Ray* r, int s_seed) {
    int rand_val = (int)(dot(r->dir, (vec3)(123.9898, 758.233, 433.3314)) * 44378.5453);
    int seed = RANDOM_BUFFER_SIZE * 3 + ((rand_val + s_seed * 2683 + get_global_id(0) * 3931 + get_global_id(1))) % RANDOM_BUFFER_SIZE;
    
    return random_buffer[seed];
}

inline bool inRayRange(float x) { return (x - MAX_DISTANCE) * (x - MIN_DISTANCE) <= 0; }

Ray genInitRay(__global const float* camera_buffer, const vec3* origin, float s, float t) {
    Ray r;
    
    vec3 camera_llc = getVec(camera_buffer, 3);
    vec3 horizontal = getVec(camera_buffer, 6);
    vec3 vertical = getVec(camera_buffer, 9);
    
    r.origin = *origin;
    r.dir = normalize(camera_llc + s * horizontal + t * vertical);
    return r;
}

vec3 rayPointAtParam(const Ray* r, float t) {
    return r->origin + r->dir * t;
}

bool hitSphere(const Ray* r, __global const Sphere* s, HPI* hpi) {
    vec3 oc = s->pos - r->origin;
    float b = dot(oc, r->dir);
    float c = dot(oc, oc) - s->r * s->r;
    float dis = b * b - c; //discriminant
    if (dis > 0) {
        float d = sqrt(dis);
        float temp = b - d;
        if(inRayRange(temp)) {
            hpi->t = temp;
            hpi->p = rayPointAtParam(r, temp);
            hpi->normal = (hpi->p - s->pos) / s->r; // FIXME: ?? NORMALIZE
            hpi->mat = s->mat;
            return true;
        }
        temp = b + d;
        if(inRayRange(temp)) {
            hpi->t = temp;
            hpi->p = rayPointAtParam(r, temp);
            hpi->normal = (hpi->p - s->pos) / s->r; // FIXME: ?? NORMALIZE
            hpi->mat = s->mat;
            return true;
        }
    }
    return false;
}

bool hitPlane(const Ray* r, __global const Plane* p, HPI* hpi) {
    float a = dot(r->dir, p->normal);
    
    //TODO: if hpi->mat.type == refractive use one-sided normal, otherwise double-sided
    
    float b = dot(p->pos - r->origin, p->normal);
    float temp = b / a;
    
    if(inRayRange(temp)) {
        hpi->t = temp;
        hpi->p = rayPointAtParam(r, temp);
        hpi->normal = -p->normal * sign(a); //!!!!!!
        hpi->mat = p->mat;
        
        return true;
    }
    
    return false;
}

bool hitLens(const Ray* r, __global const Lens* lens, HPI* hpi) {
    vec3 oc = lens->p1 - r->origin;
    float b1 = dot(oc, r->dir);
    float c = dot(oc, oc) - lens->r1 * lens->r1;
    float dis1 = b1 * b1 - c;
    
    oc = lens->p2 - r->origin;
    float b2 = dot(oc, r->dir);
    c = dot(oc, oc) - lens->r2 * lens->r2;
    float dis2 = b2 * b2 - c;
    
    if(dis1 > 0 && dis2 > 0) {
        float d1 = sqrt(dis1);
        float d2 = sqrt(dis2);
        
        float t1A = b1 - d1; // first intersection of the sphere 1
        float t1B = b1 + d1; // second intersection of the sphere 1
        float t2A = b2 - d2; // first intersection of the sphere 2
        float t2B = b2 + d2; // second intersection of the sphere 2

        __global const vec3* s;
        float rad;
        float temp;
        
        if((t1B < t2A) || (t2B < t1A)) return false; // missing the lens
        else if(MIN_DISTANCE <= t1A || MIN_DISTANCE <= t2A) {
            // outside the lens
            if(t2A <= t1A) {
                s = &(lens->p1);
                rad = lens->r1;
                temp = t1A;
            } else {
                s = &(lens->p2);
                rad = lens->r2;
                temp = t2A;
            }
        } else if(MIN_DISTANCE <= t1B && MIN_DISTANCE <= t2B) {
            // inside the lens
            if(t1B <= t2B) { // TODO: FOR SOME REASON NO BANDS IF >= IS TAKEN
                s = &(lens->p1);
                rad = lens->r1;
                temp = t1B;
            } else {
                s = &(lens->p2);
                rad = lens->r2;
                temp = t2B;
            }
        } else return false; // outside the lens facing an opposite dir
            
        if(temp <= MAX_DISTANCE) {
            hpi->t = temp;
            hpi->p = rayPointAtParam(r, temp);
            hpi->normal = (hpi->p - *s) / rad; // FIXME: ?? NORMALIZE
            hpi->mat = lens->mat;
            return true;
        }
    }
    
    return false;
}

bool hitTriangle(const Ray* r, __global const vec3* A, __global const vec3* B, __global const vec3* C, HPI* hpi) {
    // Moller-Trumbore algorithm
    
    vec3 edge1 = (*B) - (*A);
    vec3 edge2 = (*C) - (*A);
    vec3 h = cross(r->dir, edge2);
    float a = dot(edge1, h);
    if(a > -TRIANGLE_EPSILON && a < TRIANGLE_EPSILON) return false; // ray parallel to this triangle
    
    float f = 1.0f / a;
    vec3 s = r->origin - (*A);
    float u = f * dot(s, h);
    if(u < 0.0f || u > 1.0f) return false;
    
    vec3 q = cross(s, edge1);
    float v = f * dot(r->dir, q);
    if(v < 0.0f || u + v > 1.0f) return false;
    
    float temp = f * dot(edge2, q);
    if(inRayRange(temp)) {
        hpi->t = temp;
        hpi->p = rayPointAtParam(r, temp);
        // ASSUME COUNTER-CLOCKWISE WINDING ORDER
        hpi->normal = normalize(cross(edge1, edge2));
        //hpi->mat = lens->mat; the mesh takes care of this
        return true;
    } else return false;
}

bool hitMeshOut(const Ray* r, __global const Mesh* mesh, HPI* hpi) {
    // ASSUME THAT THE MESH IS CONVEX
    int vertex_count = mesh->size;
    for(int i = 0; i < vertex_count; i++) {
        if(hitTriangle(r, getMeshVertex(mesh, i), getMeshVertex(mesh, i + 1), getMeshVertex(mesh, i + 2), hpi)) {
            if(dot(hpi->normal, r->dir) < 0.0f) {
                hpi->mat = mesh->mat;
                return true;
            }
        }
    }
    
    return false;
}

bool hitScene(const Ray* r, __global const Scene* scene, HPI* hpi) {
    bool hit_any = false;
    float hit_min = MAX_DISTANCE;
    HPI hpi_result;
    
    for(int i = 0; i < NUM_SPHERES; i++) {
        if(hitSphere(r, scene->spheres + i, &hpi_result) && hpi_result.t < hit_min) {
            hit_any = true;
            *hpi = hpi_result;
            hit_min = hpi_result.t;
        }
    }
    
    for(int i = 0; i < NUM_PLANES; i++) {
        if(hitPlane(r, scene->planes + i, &hpi_result) && hpi_result.t < hit_min) {
            hit_any = true;
            *hpi = hpi_result;
            hit_min = hpi_result.t;
        }
    }
    
    for(int i = 0; i < NUM_LENSES; i++) {
        if(hitLens(r, scene->lenses + i, &hpi_result) && hpi_result.t < hit_min) {
            hit_any = true;
            *hpi = hpi_result;
            hit_min = hpi_result.t;
        }
    }
    
    for(int i = 0; i < NUM_MESHES; i++) {
        if(hitMeshOut(r, scene->meshes + i, &hpi_result) && hpi_result.t < hit_min) {
            hit_any = true;
            *hpi = hpi_result;
            hit_min = hpi_result.t;
        }
    }
    
    return hit_any;
}

void rayReflect(Ray* r, col* c, const HPI* hpi) {
    r->origin = hpi->p;
    r->dir = normalize(r->dir - 2.0f * dot(r->dir, hpi->normal) * hpi->normal);
    
    if(hpi->mat->type == t_reflective) (*c) *= hpi->mat->extra_data;
}

void rayRefract(Ray* r, col* c, HPI* hpi) {
    vec3 normal;
    float idx_ratio;
    float cai = dot(r->dir, hpi->normal); //cos_angle_incident
    if(cai > 0) {
        normal = -hpi->normal;
        idx_ratio = hpi->mat->extra_data;
        cai = -cai;
    } else {
        normal = hpi->normal;
        idx_ratio = 1.0f / hpi->mat->extra_data;
    }

    float discriminant = 1.0f - idx_ratio * idx_ratio * (1.0f - cai * cai);
    
    if(discriminant > 0.0f) {
        r->origin = hpi->p;
        r->dir = idx_ratio * r->dir - normal * (idx_ratio * cai + sqrt(discriminant));
    } else {
        hpi->normal = normal;
        rayReflect(r, c, hpi);
    }
}

void rayScatter(Ray* r, col* c, const HPI* hpi, __global const float* random_buffer, int s_seed) {
    vec3 random_in_sphere = randomVec(random_buffer, r, s_seed);
    r->dir = normalize(hpi->normal + random_in_sphere);
    r->origin = hpi->p;
    
    (*c) *= hpi->mat->extra_data;
}

float schlick(float cai, float idx_ratio) {
    float r0 = (1.0f - idx_ratio) / (1.0f + idx_ratio);
    r0 *= r0;
    return r0 + (1.0f - r0) * pow((1.0f - cai), 5);
}

void rayRefractDielectric(Ray* r, col* c, HPI* hpi, __global const float* random_buffer, int s_seed) {
    vec3 normal;
    float idx_ratio;
    float cai = dot(r->dir, hpi->normal); //cos_angle_incident
    if(cai > 0) {
        normal = -hpi->normal;
        idx_ratio = hpi->mat->extra_data;
        cai = -cai;
    } else {
        normal = hpi->normal;
        idx_ratio = 1.0f / hpi->mat->extra_data;
    }
    
    float reflect_prob = schlick(-cai, idx_ratio);
    float rand = random(random_buffer, r, s_seed);
    
    if(reflect_prob < rand) {
        float discriminant = 1.0f - idx_ratio * idx_ratio * (1.0f - cai * cai);
        
        if(discriminant > 0.0f) {
            r->origin = hpi->p;
            r->dir = idx_ratio * r->dir - normal * (idx_ratio * cai + sqrt(discriminant));
            return;
        }
    }
    
    hpi->normal = normal;
    rayReflect(r, c, hpi);
}

inline col bkgCol(const Ray *r) {
    float y = -r->dir.y * 0.25f + 0.6f;
    return (col)(y * 0.6f + 0.1f, y, 1.0f);
}

col getCol(Ray* r, __global const float* random_buffer, __global const Scene* scene, int sample) {
    col out = (col)(1.0f);
    
    for(int i = 0; i < DEPTH; i++) {
        HPI hpi;
        bool hit = hitScene(r, scene, &hpi);
        if(!hit) {
            out = (col)(0.0f);//min(out, bkgCol(r));
            break;
        } else {
            switch(hpi.mat->type) {
                case t_diffuse:
                    rayScatter(r, &out, &hpi, random_buffer, i + sample);
                    break;
                case t_light:
                    i = DEPTH;
                    break;
                case t_reflective:
                    rayReflect(r, &out, &hpi);
                    break;
                case t_refractive:
                    rayRefract(r, &out, &hpi);
                    break;
                case t_dielectric:
                    rayRefractDielectric(r, &out, &hpi, random_buffer, i + sample);
                    break;
            }
            
            out = min(out, hpi.mat->color); // mix colors
        }
    }
    
    return out;
}

inline col gamma_corr(const col* color) {
    return sqrt(*color);
}

inline col gamma_corr_inv(const col* color) {
    return (*color) * (*color); // for anny other GAMMA value, use: pow(*color, GAMMA);
}

__kernel void trace(__write_only image2d_t image, __global const float* camera_buffer, __global const float* random_buffer, __global const Scene* scene) {
    int x = get_global_id(0);
    int y = get_global_id(1);
    
    float s = (float)x / (float)get_image_width(image);
    float t = (float)y / (float)get_image_height(image);
    
    vec3 camera_pos = getVec(camera_buffer, 0);
    
    Ray r_main = genInitRay(camera_buffer, &camera_pos, s, t);
    
    col out = getCol(&r_main, random_buffer, scene, 0);
    
    write_imagef(image, (int2)(x, y), (float4)(gamma_corr(&out), 1.0f));
}

__kernel void retrace(__read_only image2d_t image_in, __write_only image2d_t image_out, __global const float* camera_buffer, __global const float* random_buffer, __global const Scene* scene, const int sample) {
    int x = get_global_id(0);
    int y = get_global_id(1);
    int2 loc = (int2)(x, y);
    
    float s = (float)x / (float)get_image_width(image_in);
    float t = (float)y / (float)get_image_height(image_in);
    
    vec3 camera_pos = getVec(camera_buffer, 0);
    
    Ray r_main = genInitRay(camera_buffer, &camera_pos, s, t);
    
    col prev_col = read_imagef(image_in, sampler, loc).xyz;
    
    float mixing_param = (float)(sample)/(float)(sample + 1);
    
    col out = mix(getCol(&r_main, random_buffer, scene, sample), gamma_corr_inv(&prev_col), mixing_param);
    
    // use sqrt for gamma_corr correction
    write_imagef(image_out, loc, (float4)(gamma_corr(&out), 1.0f));
}


inline __global Material* getMaterial(__global Scene* scene, int id) {
    return scene->materials + id;
}

__kernel void createScene(__global Scene* scene, float time) {
    scene->materials[0] = (Material){t_reflective, (col)(1.0f), 0.8f};
    scene->materials[1] = (Material){t_refractive, (col)(1.0f, 1.0f, 1.0f), 1.1f};
    scene->materials[2] = (Material){t_refractive, (col)(1.0f), 2.0f};
    scene->materials[3] = (Material){t_diffuse, (col)(1.0f, 0.0f, 0.0f), 0.5f};
    scene->materials[4] = (Material){t_diffuse, (col)(0.0f, 1.0f, 0.0f), 0.5f};
    scene->materials[5] = (Material){t_dielectric, (col)(1.0f), 1.3f};
    scene->materials[6] = (Material){t_light, (col)(1.0f), 0.0f};
    scene->materials[7] = (Material){t_diffuse, (col)(1.0f), 1.0f};
    
    scene->spheres[0] = (Sphere){(vec3)(0.0f, 0.0f, 3.0f), 1.5f, getMaterial(scene, 0)};
    scene->spheres[1] = (Sphere){(vec3)(0.0f, 0.0f, -3.0f), 1.0f, getMaterial(scene, 0)};
    scene->spheres[2] = (Sphere){(vec3)(0.0f, 3.0f, 0.0f), 1.0f, getMaterial(scene, 1)};
    scene->spheres[3] = (Sphere){(vec3)(-0.02f, -3.0f, 0.0f), 1.0f, getMaterial(scene, 3)};
    scene->spheres[4] = (Sphere){(vec3)(2.02f, -3.0f, 0.0f), 1.0f, getMaterial(scene, 4)};
    scene->spheres[5] = (Sphere){(vec3)(1.0f, -200.0f, 0.0f), 100.0f, getMaterial(scene, 6)};
    scene->spheres[6] = (Sphere){(vec3)(-3.0f, 0.0f, 0.0f), 1.0f, getMaterial(scene, 5)};
    scene->planes[0] = (Plane){(vec3)(0.0f, 5.0f, 0.0f), (vec3)(0.0f, 1.0f, 0.0f), getMaterial(scene, 7)};
    scene->lenses[0] = createLens((vec3)(5.0f, 0.0f, 0.0f), (vec3)(1.0f, 0.0f, 0.0f), 10.0f, 10.0f, 2.0f, getMaterial(scene, 2));
}
