#pragma once
#include <LeapC.h>
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <vector>



//Fuck it this will be a global variable
struct RAND_RANGE{
    float scale[2]; //min, max
    int angle;
    float shearing;
    float translation;
} prm;



class AffineTransfo{
public:
    glm::mat4 matrix;
    bool overwriteMatrix = false;

    glm::vec3 scale_factors;
    glm::vec3 rot_axis;
    float rot_angle;
    glm::vec3 Sh_xy_xz_yx;
    glm::vec3 Sh_yz_zx_zy;
    glm::vec3 translation_vector;

    AffineTransfo();

    void updt_matrix();
    void setIdentity();
    void setRandom();

    void ui();

    const glm::mat4& getMatrix();
};



class Attractor{
public:
    AffineTransfo* attr_funcs[10];

    Attractor();

    void setRandom(size_t nb_func);
    void ui(size_t nb_func);
    const glm::mat4& getMatrix(size_t index) const;
    void freeAll();
    
};



class AttractorRenderer{
    public:
        AttractorRenderer();
        //void leap_update(const LEAP_TRACKING_EVENT& frame);
        void defaultsValues();
        void draw_ui();
        void render(float width,float height,glm::vec3 pos,glm::mat4 inv_view, glm::mat4 inv_proj, glm::vec3 light_pos, glm::vec3 fractal_position,glm::quat fractal_rotation);
    private:
        //Variables linked to animation
        struct ANIM{
            bool no_clear;
            bool idle;
            int iter;               //the number of iteration over the animation (counted in lerp)
            float spin_Period;  //time in second to make a full circle
            float height_and_distance[2];
            float lerp_period;   //time between 2 seed change
            float lerp_stiffness;
        } anim;

        //attractors values
        struct ATR {
            int matrixPerAttractor;
            float lerpFactor;
            float lerpEdgeClamp;
            Attractor A_tractor;
            Attractor B_tractor;
            std::vector<glm::mat4> ubo_matrices;
        } atr;

        struct clr{
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

            //ao
            float ao_fac;
            float ao_size;
            glm::vec3 col_ao;
        } clr;
        
};