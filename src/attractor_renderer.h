#pragma once
#include <LeapC.h>
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <vector>
#include "attractor.h"

enum LerpMode{
    Matrix,
    PerComponent
};

class AttractorRenderer{
public:
    AttractorRenderer(int w,int h);
    void leap_update(const LEAP_TRACKING_EVENT& frame);
    void draw_ui(float& speed,glm::vec3& pos);
    void render(float width,float height,glm::vec3& pos,glm::mat4& inv_view, glm::mat4& proj, glm::vec3& light_pos);
    void resize(float w,float h);
    void reset();
private:
    void allIdentity();
    void sierpinski();
    void sierpintagon();
    void sierpolygon();
    void barsnley_fern();
    void romanesco();
    glm::mat4 get_idle_view(float time);
    void update_ubo_matrices();
    void transformInit();
    GLuint attractor_program;
    GLuint shading_program;
    int matrix_per_attractor = 0;
    float lerp_factor = 0.0f;
    LerpMode lerp_mode = PerComponent;


    //jump distance maprange
    float JD_FR_MIN = 0.1f;
    float JD_FR_MAX = 0.9f;

    //phong param
    float k_a = 0.2f;
    float k_d = 0.5f;
    float k_s = 1.0f;
    float alpha = 0.9f;
    glm::vec3 col_specular = glm::vec3(0.7,0.7,0.7);

    float ao_fac = 0.015f;
    float ao_size = 0.2;
    glm::vec3 col_ao = glm::vec3(1.0,0.3,0.1);

    glm::vec3 col_jd_low = glm::vec3(0.3,1.0,0.15);
    glm::vec3 col_jd_high = glm::vec3(0.0,0.8,1.0);

    Attractor attractorA;
    Attractor attractorB;
    std::vector<glm::mat4> ubo_matrices;
    GLuint depth_texture;
    GLuint jumpdist_texture;

    bool no_clear = true;
    bool idle = false;
    int iter = 0; //the number of iteration over the animation (counted in lerp)

    float spin_period = 30.0f; //time in second to make a full circle
    float height_and_distance[2] = {2.0f, 1.75f};
    float lerp_period = 3.0f; //time between 2 seed change
    float lerp_stiffness = 3.0f; //parmeter k of the function used for lerping curve. Function is (1/(1+exp(-k(2x-1))) - mv) * 1/(1-2mv) where mv =  1+(1+exp(k)) Not used currently.

    GLuint ssbo_pts; //ssbo of points

    //glm::mat4 for attractors
    GLuint uboM4;
    glm::mat4 old_view;
};