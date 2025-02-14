#pragma once
#include <LeapC.h>
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <vector>
#include "attractor.h"


struct AttractorRenderArgs {
    unsigned int nbpts;
};

enum LerpMode{
    Matrix,
    PerComponent
};

class AttractorRenderer{
public:
    AttractorRenderer(int w,int h, AttractorRenderArgs construction_args);
    void leap_update(const LEAP_TRACKING_EVENT& frame);
    void draw_ui(float& speed,glm::vec3& pos);
    void render(float width,float height, glm::vec3& pos,glm::mat4& inv_view, glm::mat4& proj, glm::vec3& light_pos);
    void resize(float w,float h);
private:
    void default_values();
    void ssaoOnlyValues();
    void allIdentity();
    void sierpinski();
    void sierpintagon();
    void sierpolygon();
    void barsnley_fern();
    void romanesco();
    glm::mat4 get_idle_view(float time);
    void update_ubo_matrices();
    void update_ubo_samples(glm::vec4* samples);
    void transformInit();
    GLuint attractor_program;
    GLuint shading_program;

    unsigned int nbpts_gpu; //number of point sent to gpu at instantiation AND dispatch compute count each frame
    int matrix_per_attractor = 0;
    float lerp_factor = 0.0f;
    LerpMode lerp_mode = PerComponent;


    //jump distance maprange
    float JD_FR_MIN;
    float JD_FR_MAX;
    glm::vec3 col_jd_low;
    glm::vec3 col_jd_high;

    //phong param
    float k_a;
    float k_d;
    float k_s;
    float alpha;
    glm::vec3 col_specular;

    //AO
    int ssao_version;
    float ao_fac;
    float ao_size;
    glm::vec3 col_ao;

    //Animation 
    bool no_clear;
    bool idle;
    int iter; //the number of iteration over the animation (counted in lerp)
    float spin_period = 30.0f; //time in second to make a full circle
    float height_and_distance[2];
    float lerp_period; //time between 2 seed change
    float lerp_stiffness; //parmeter k of the function used for lerping curve. Function is (1/(1+exp(-k(2x-1))) - mv) * 1/(1-2mv) where mv =  1+(1+exp(k)) Not used currently.

    //Attractors
    Attractor attractorA;
    Attractor attractorB;
    std::vector<glm::mat4> ubo_matrices;
    std::vector<float> ubo_weights;
    GLuint depth_texture;
    GLuint jumpdist_texture;

    bool skipSampleUpdt;
    GLuint ssbo_pts; //ssbo of points

    //glm::mat4 for attractors
    GLuint ubo;
    GLuint ubo_samples;
    glm::mat4 old_view;
};