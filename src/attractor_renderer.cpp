#include "attractor_renderer.h"
#include "attractor.h"
#include "glm/fwd.hpp"
#include "glm/geometric.hpp"
#include "glm/matrix.hpp"
#include "imgui/imgui.h"
#include "imgui_util.hpp"
#include "opengl_util.h"
#include <LeapC.h>
#include <cassert>
#include <cstdio>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <GLFW/glfw3.h>

#include "glm/gtx/rotate_vector.hpp"

void randArray(float* array, int size, float range){
    for(int i=0; i<size; i+=4){
        array[i] = range * (((float)rand()/RAND_MAX) *2 -1);
        array[i+1] = range * (((float)rand()/RAND_MAX) *2 -1);
        array[i+2] = range * (((float)rand()/RAND_MAX) *2 -1);
        array[i+3] = 1;
    }
}

//to delete probably
void origindArray(float* array, int size){
    for(int i=0; i<size; i+=4){
        //which is x, whihc is y, which is z, which is w ?
        array[i] = 0.0f; 
        array[i+1] = 0.0f;
        array[i+2] = 0.0f;
        array[i+3] = 1.0f;
    }
}

inline float maprange(float v, float old_min, float old_max, float new_min, float new_max) {
    // Check for division by zero
    if (old_max == old_min) {
        return new_min;  // Return new_min if old range is invalid
    }
    
    // Calculate the scale factor and map the value
    float scale = (new_max - new_min) / (old_max - old_min);
    return new_min + (v - old_min) * scale;
}


//todo : all those Lerp function should not take an Attractor but a transform

//lerp - per matrix coefficient on 2 matrix genned by attractors A and B
static glm::mat4 matrixLerp(const Attractor& attractorA, const Attractor& attractorB, int index, float t) {
    glm::mat4 matA = attractorA.attr_funcs[index]->getMatrix();
    glm::mat4 matB = attractorB.attr_funcs[index]->getMatrix();
    return (1.0f - t) * matA + t * matB;
}

//fucked up if attractorfucntion aren't all fixed process. I plan on just deleting rawmatrix anyways
static glm::mat4 componentLerp(const Attractor& attractorA, const Attractor& attractorB, int index, float t) {
    Transform* genA = attractorA.attr_funcs[index];
    Transform* genB = attractorB.attr_funcs[index];
    
    //if overwrite matrix mode can't lerp by component
    if(genA->overwriteMatrix || genB->overwriteMatrix)
        return matrixLerp(attractorA, attractorB, index, t);

    
    glm::vec3 l_scale_factors = (1.0f-t)*genA->scale_factors + t * genB->scale_factors;
    glm::vec3 l_rot_axis = glm::normalize((1.0f-t)*genA->rot_axis + t *genB->rot_axis);
    float l_rot_angle = (1.0f-t)*genA->rot_angle + t * genB->rot_angle;
    glm::vec3 l_Sh_xy_xz_yx = (1.0f-t)*genA->Sh_xy_xz_yx + t * genB->Sh_xy_xz_yx;
    glm::vec3 l_Sh_yz_zx_zy = (1.0f-t)*genA->Sh_yz_zx_zy + t * genB->Sh_yz_zx_zy;
    glm::vec3 l_translation_vector = (1.0f-t)*genA->translation_vector + t * genB->translation_vector;

    glm::mat4 matrix =
        glm::scale(glm::mat4(1.0f), l_scale_factors) *
        glm::rotate(glm::mat4(1.0f), l_rot_angle, l_rot_axis) *
        glm::transpose(glm::mat4(
            1.0f, l_Sh_xy_xz_yx.x,  l_Sh_xy_xz_yx.y, 0.0f,  //     1.0f, Shxy,  Shxz, 0.0f,
            l_Sh_xy_xz_yx.z, 1.0f,  l_Sh_yz_zx_zy.x, 0.0f,  //     Shyx, 1.0f,  Shyz, 0.0f,
            l_Sh_yz_zx_zy.y, l_Sh_yz_zx_zy.z,  1.0f, 0.0f,  //     Shzx, Shzy,  1.0f, 0.0f,
            0.0f, 0.0f,  0.0f, 1.0f)) *                 //     0.0f, 0.0f,  0.0f, 1.0f))
        glm::translate(glm::mat4(1.0f), l_translation_vector);
    return matrix;
}

static glm::mat4 leapLerp(const Attractor& attractorA, const LeapToAttractorModifier& leapInfo, int index, const LeapInfluenceFacs& facs) {
    //uses fac to control how strong the influence of the hand is.
    Transform* genA = attractorA.attr_funcs[index];

    //todo change this depending on leapInfo
    glm::vec3 l_scale_factors = genA->scale_factors;
    glm::vec3 l_rot_axis = glm::normalize((1.0f-facs.fac_axis)*genA->rot_axis + facs.fac_axis * leapInfo.ofs_axis);
    float l_rot_angle = genA->rot_angle + leapInfo.ofs_rotate_angle * facs.fac_angle;
    glm::vec3 l_Sh_xy_xz_yx = genA->Sh_xy_xz_yx;
    glm::vec3 l_Sh_yz_zx_zy = genA->Sh_yz_zx_zy;
    glm::vec3 l_translation_vector = genA->translation_vector * glm::pow(leapInfo.ofs_translate, facs.fac_tr);
    

    glm::mat4 matrix =
                glm::scale(glm::mat4(1.0f), l_scale_factors) *
                glm::rotate(glm::mat4(1.0f), l_rot_angle, l_rot_axis) *
                glm::transpose(glm::mat4(
                    1.0f, l_Sh_xy_xz_yx.x,  l_Sh_xy_xz_yx.y, 0.0f,  //     1.0f, Shxy,  Shxz, 0.0f,
                    l_Sh_xy_xz_yx.z, 1.0f,  l_Sh_yz_zx_zy.x, 0.0f,  //     Shyx, 1.0f,  Shyz, 0.0f,
                    l_Sh_yz_zx_zy.y, l_Sh_yz_zx_zy.z,  1.0f, 0.0f,  //     Shzx, Shzy,  1.0f, 0.0f,
                    0.0f, 0.0f,  0.0f, 1.0f)) *                 //     0.0f, 0.0f,  0.0f, 1.0f))
                glm::translate(glm::mat4(1.0f), l_translation_vector);
    return matrix;
}

void AttractorRenderer::update_ubo_matrices(){
    switch (lerp_mode)
    {
    case Matrix:
        for(int i=0; i < matrix_per_attractor; i++){
            ubo_matrices[i] = (matrixLerp(attractorA, attractorB, i,  lerp_factor));
        }
        break;
    case PerComponent:
        for(int i=0; i < matrix_per_attractor; i++){
            ubo_matrices[i] = (componentLerp(attractorA, attractorB, i,  lerp_factor));
        }
        break;
    case FromLeapLerp:
        for(int i=0; i < matrix_per_attractor; i++){
            ubo_matrices[i] = (leapLerp(attractorA, leapInfos[i], i,  leapFacs));
        }
        break;
    default:
        throw std::runtime_error("Unhandled lerping mode");
        break;
    }
    float total_weight=0.0;
    for(int i=0; i < matrix_per_attractor; i++){
        glm::mat3 m = glm::mat3(ubo_matrices[i]);
        total_weight+= glm::length(glm::cross(m[0],m[1]))+glm::length(glm::cross(m[1],m[2]))+glm::length(glm::cross(m[2],m[0]));
        ubo_weights[i] = total_weight;
    }
    for(int i=0; i < matrix_per_attractor; i++){
        ubo_weights[i] /= total_weight;
    }

}

void AttractorRenderer::update_ubo_samples(glm::vec4* samples){
    if(skipSampleUpdt) return;
    for(size_t i=0; i <HALFSPHERE_SAMPLES; i++){
        //random point in halfsphere
        glm::vec4 sample = (float)rand()/RAND_MAX * glm::normalize(glm::vec4(
            (float)rand()/RAND_MAX *2 - 1,
            (float)rand()/RAND_MAX *2 - 1,
            (float)rand()/RAND_MAX,
            1.0f
        ));

        //cluster near origin
        float scale = float(i) / HALFSPHERE_SAMPLES;
        sample *= 0.1f+0.9f*scale*scale;

        samples[i] = sample;

    }
}

void AttractorRenderer::transformInit(){

    printf("starting Attractor assingment ...\n");
    Transform* mga[MAX_FUNC_PER_ATTRACTOR];
    Transform* mgb[MAX_FUNC_PER_ATTRACTOR];

    for(int i=0; i<MAX_FUNC_PER_ATTRACTOR; i++){
        mga[i] = new Transform();
        mgb[i] = new Transform();
        attractorA.attr_funcs[i] = mga[i];
        attractorB.attr_funcs[i] = mgb[i];
        ubo_matrices.push_back(glm::mat4(1.0f));
        ubo_weights.push_back(1.0);
    }
    
    matrix_per_attractor = 3; //can be any value between 3 or 10

    printf("done\n");
}

void AttractorRenderer::allIdentity(){
    for(int i=0; i<MAX_FUNC_PER_ATTRACTOR; i++){
        attractorA.attr_funcs[i]->setIdentity();
        attractorA.attr_funcs[i]->overwriteMatrix = false;
        attractorB.attr_funcs[i]->setIdentity();
        attractorB.attr_funcs[i]->overwriteMatrix = false;
    }
}

void AttractorRenderer::sierpinski(){
    matrix_per_attractor = 3;
    allIdentity();
    Transform** a_funcs = attractorA.attr_funcs;
    Transform** b_funcs = attractorB.attr_funcs;
    {//attractor A functions
        a_funcs[0]->scale_factors = glm::vec3(0.5f,0.5f,0.0f);
        a_funcs[0]->translation_vector = glm::vec3(0.0f,0.36f,0.0f);

        a_funcs[1]->scale_factors = glm::vec3(0.5f,0.5f,0.0f);
        a_funcs[1]->translation_vector = glm::vec3(0.5f,-0.5f,0.0f);

        a_funcs[2]->scale_factors = glm::vec3(0.5f,0.5f,0.0f);
        a_funcs[2]->translation_vector = glm::vec3(-0.5f,-0.5f,0.0f);
    }
    {//attractor B functions
        b_funcs[0]->scale_factors = glm::vec3(0.5f,0.5f,0.0f);
        b_funcs[0]->translation_vector = glm::vec3(0.0f,-0.36f,0.0f);
        
        b_funcs[1]->scale_factors = glm::vec3(0.5f,0.5f,0.0f);
        b_funcs[1]->translation_vector = glm::vec3(-0.5f,0.5f,0.0f);
        
        b_funcs[2]->scale_factors = glm::vec3(0.5f,0.5f,0.0f);
        b_funcs[2]->translation_vector = glm::vec3(0.5f,0.5f,0.0f);
    }
}
void AttractorRenderer::sierpintagon(){
    matrix_per_attractor = 5;
    allIdentity();
    Transform** a_funcs = attractorA.attr_funcs;
    //Transform** b_funcs = attractorB.attr_funcs;
    {//attractor A functions
        a_funcs[0]->scale_factors = glm::vec3(0.5f,0.5f,0.0f);
        a_funcs[0]->translation_vector = glm::vec3(0.5f,0.0f,0.0f);

        a_funcs[1]->scale_factors = glm::vec3(0.5f,0.5f,0.0f);
        a_funcs[1]->translation_vector = glm::vec3(0.15f,0.48f,0.0f);

        a_funcs[2]->scale_factors = glm::vec3(0.5f,0.5f,0.0f);
        a_funcs[2]->translation_vector = glm::vec3(-0.4f,0.29f,0.0f);
        
        a_funcs[3]->scale_factors = glm::vec3(0.5f,0.5f,0.0f);
        a_funcs[3]->translation_vector = glm::vec3(-0.4f,-0.29f,0.0f);

        a_funcs[4]->scale_factors = glm::vec3(0.5f,0.5f,0.0f);
        a_funcs[4]->translation_vector = glm::vec3(0.15f,-0.48f,0.0f);
    }
}
int nb_cote = 5;
void AttractorRenderer::sierpolygon(){
    matrix_per_attractor = nb_cote;
    allIdentity();
    Transform** a_funcs = attractorA.attr_funcs;
    //Transform** b_funcs = attractorB.attr_funcs;

    float radius = 0.5f;
    float angle_incr = 2 * PI / (float)nb_cote;

    float cx, cy;
    for(int i=0; i < nb_cote; i++){
        cx = radius * cos((float)i*angle_incr);
        cy = radius * sin((float)i*angle_incr);

        a_funcs[i]->scale_factors = glm::vec3(0.5f,0.5f,0.0f);
        a_funcs[i]->translation_vector = glm::vec3(cx,cy,0.0f);
    }
}

void AttractorRenderer::barsnley_fern(){
    matrix_per_attractor = nb_cote;
    allIdentity();
    Transform** a_funcs = attractorA.attr_funcs;
    matrix_per_attractor = 4;
    {
        a_funcs[0]->overwriteMatrix = true;
        a_funcs[0]->matrix = glm::transpose(glm::mat4(
                    0.0f, 0.0f,  0.0f, 0.0f,
                    0.0f, 0.16f,  0.0f, 0.0f,
                    0.0f, 0.0f,  0.0f, 0.0f,
                    0.0f, 0.0f,  0.0f, 1.0f));

        a_funcs[1]->overwriteMatrix = true;
        a_funcs[1]->matrix = glm::transpose(glm::mat4(
                    0.85f, 0.04f,  0.0f, 0.0f,
                    -0.04f, 0.85f,  0.0f, 1.6f,
                    0.0f, 0.0f,  0.0f, 0.0f,
                    0.0f, 0.0f,  0.0f, 1.0f));
        
        a_funcs[2]->overwriteMatrix = true;
        a_funcs[2]->matrix = glm::transpose(glm::mat4(
                    0.2f, -0.26f,  0.0f, 0.0f,
                    0.23f, 0.22f,  0.0f, 1.6f,
                    0.0f, 0.0f,  0.0f, 0.0f,
                    0.0f, 0.0f,  0.0f, 1.0f));

        a_funcs[3]->overwriteMatrix = true;
        a_funcs[3]->matrix = glm::transpose(glm::mat4(
                    -0.15f, 0.28f,  0.0f, 0.0f,
                    0.26f, 0.24f,  0.0f, 0.44f,
                    0.0f, 0.0f,  0.0f, 0.0f,
                    0.0f, 0.0f,  0.0f, 1.0f));
    }
}

void AttractorRenderer::romanesco(){
    matrix_per_attractor = 2;
    allIdentity();
    Transform** a_funcs = attractorA.attr_funcs;
    Transform** b_funcs = attractorB.attr_funcs;
    {//attractor A functions
        a_funcs[0]->scale_factors = glm::vec3(0.975f,0.98f,0.975f);
        a_funcs[0]->translation_vector = glm::vec3(0.0f,0.01f,0.0f);
        a_funcs[0]->rot_axis = glm::vec3(0.0,1,0.145);
        a_funcs[0]->rot_angle = PI/(sqrt(5)+1.0)*2.0;

        a_funcs[1]->scale_factors = glm::vec3(0.3f,0.3f,0.3f);
        a_funcs[1]->rot_axis = glm::vec3(0.6,0.98,0.185);
        a_funcs[1]->rot_angle = PI/2;
        a_funcs[1]->translation_vector = glm::vec3(0.0f,0.69f,0.0f);

    }
    {//attractor B functions
        b_funcs[0]->scale_factors = glm::vec3(0.9f,0.9f,0.9f);
        b_funcs[0]->translation_vector = glm::vec3(0.0f,0.001f,0.0f);
        b_funcs[0]->rot_axis = glm::vec3(0.0,1,0);
        b_funcs[0]->rot_angle = PI/(sqrt(5)+1)*2;

        b_funcs[1]->scale_factors = glm::vec3(0.2f,0.2f,0.2f);
        b_funcs[1]->rot_axis = glm::vec3(1.0,0.0,0.0);
        b_funcs[1]->rot_angle = PI/2;
        b_funcs[1]->translation_vector = glm::vec3(0.0f,1.0f,0.0f);
    }
}

float smooth_curve(float v, float k, float os = 0.0f){
    //k=3 by default which is nearly identical to smoothstep
    const float mv = 1.0f/(1+exp(k));
    return 0.5 + (1.0f/(1+exp(-k*(2*v-1)) ) - 0.5) * ( (0.5f-os)/(0.5-mv));
    ///smooth step : //return v*v*3-v*v*v*2;
    //without edgeclamp : //return (1.0f/(1+exp(-k*(2*v-1)) ) - mv) * (1.0f/(1-2*mv));    
}
glm::mat4 AttractorRenderer::get_idle_view(float time){
    float angle = 2*PI*time / spin_period;
    float intpart;
    lerp_factor = smooth_curve(std::modf(time/lerp_period, &intpart), lerp_stiffness); //todo edgeclamp
    if(intpart > (float)iter){ //a little messy but modf takes a pointer to float so ....
        iter = intpart;
        if(iter%2) attractorB.setRandom(matrix_per_attractor);
        else attractorA.setRandom(matrix_per_attractor);
    }
    lerp_factor = iter%2 ? lerp_factor : 1.0f - lerp_factor;

    glm::vec3 eye = glm::vec3(
        height_and_distance[1] * cos(angle),
        height_and_distance[0],
        height_and_distance[1] * sin(angle)
    );
    return glm::lookAt(eye,glm::vec3(0.0f),glm::vec3(0.0f,1.0f,0.0f));
}

void AttractorRenderer::default_values(){
    //jumpdist
    JD_FR_MIN = 0.6f;
    JD_FR_MAX = 1.3f;
    col_jd_low = glm::vec3(1.0f,0.22f,0.22f);
    col_jd_high = glm::vec3(0.05f,1.0f,0.95f);
    bg_color = glm::vec3(0.1f, 0.2f, 0.4f);

    //phong param
    k_a = 0.2f;
    k_d = 0.5f;
    k_s = 1.0f;
    alpha = 0.9f;
    col_specular = glm::vec3(0.9,0.05f,1.0f);

    //ao
    ssao_version = 2;
    ao_fac = 0.015f;
    ao_size = 0.2f;
    col_ao = glm::vec3(0.0f,1.0f,1.0f);

    //animation
    no_clear = true;
    idle = false;
    iter = 0;
    spin_period = 30.0f; //time in second to make a full circle
    height_and_distance[0] = 2.0f;
    height_and_distance[1] = 1.75f;
    lerp_period = 3.0f;
    lerp_stiffness = 3.0f;

    //leap Motion
    for(int i=0; i<10; i++){ //todo hardcoded max func per attractor
        leapInfos[i].default_init();
    }
    leapFacs.default_init();
    
    
    //other
    skipSampleUpdt = false;
    forceRedraw = false;


}

void AttractorRenderer::ssaoOnlyValues(){
    k_d =0.0f;
    k_s = 0.0f;
    col_jd_low = glm::vec3(1.0f);
    col_jd_high = glm::vec3(1.0f);
    col_ao = glm::vec3(1.0f);
}
AttractorRenderer::AttractorRenderer(int w,int h, AttractorRenderArgs construction_args){

    default_values();
    nbpts_gpu = construction_args.nbpts;


    {//compute_program for attractor
        attractor_program = glCreateProgram();
        GLint comp_attractor = loadshader("shaders/attractor.comp", GL_COMPUTE_SHADER);
        assert(comp_attractor);
        glAttachShader(attractor_program, comp_attractor);
        assert(linkProgram(attractor_program));

        //ugly fix bc my compiler ignore aserts :(
        #ifdef FORCE_ASSERT
        linkProgram(attractor_program);
        #endif

        shading_program = glCreateProgram();
        GLint ssao_shader = loadshader("shaders/attractor_shading.comp", GL_COMPUTE_SHADER);
        assert(ssao_shader);
        glAttachShader(shading_program, ssao_shader);
        assert(linkProgram(shading_program));

        //ugly fix bc my compiler ignore aserts :(
        #ifdef FORCE_ASSERT
        linkProgram(shading_program);
        #endif
    }

    {//depth texture (texture1, binding 1)
        glGenTextures(1, &depth_texture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, depth_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32I, w, h, 0, GL_RED_INTEGER, GL_INT, nullptr);
        //required for some reason on my computer
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glBindImageTexture(1, depth_texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32I); //bind to channel 1
    }

    {//distance from last jump texture (texture 2, binding4)
        glGenTextures(1, &jumpdist_texture);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, jumpdist_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32I, w, h, 0, GL_RED_INTEGER, GL_INT, nullptr); 
        //required for some reason on my computer
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0); 
        glBindImageTexture(4, jumpdist_texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32I); //bind to channel 4
    }
    {//points SSBO
        //generates points SSBO
        float* data = new float[nbpts_gpu*4];
        randArray(data, nbpts_gpu, 1);
        glGenBuffers(1, &ssbo_pts);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_pts);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * 4 * nbpts_gpu, data, GL_DYNAMIC_DRAW); //GL_DYNAMIC_DRAW update occasionel et lecture frequente
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssbo_pts);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // unbind

        delete[] data; //points memory only needed on GPU
    }

    {//attractors (a max of 10 matrices stored as UBO)
        glGenBuffers(1, &ubo);
        glBindBuffer(GL_UNIFORM_BUFFER, ubo);
        glBufferData(GL_UNIFORM_BUFFER, MAX_FUNC_PER_ATTRACTOR * (sizeof(glm::mat4)+sizeof(float)*4), nullptr, GL_DYNAMIC_DRAW); // Allocate space for up to 10 matrices
        glBindBufferBase(GL_UNIFORM_BUFFER, 1, ubo); // Binding point 1
        glBindBuffer(GL_UNIFORM_BUFFER, 0); // Unbind

        //reserve matrices to be pushed to UBO each frame
        ubo_matrices.reserve(MAX_FUNC_PER_ATTRACTOR);
        transformInit();
    }

    {//halfpshere samples in UBO
        //GL_MAX_SHADER_STORAGE_BLOCK_SIZE = 0x90DE = 37086 >>> 64*4*32 = 8192 so it works
        //uses vec4 for alignement
        
        glGenBuffers(1, &ubo_samples);
        glBindBuffer(GL_UNIFORM_BUFFER, ubo_samples);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::vec4) * HALFSPHERE_SAMPLES, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, 5, ubo_samples);
        glBindBuffer(GL_UNIFORM_BUFFER, 0); //Unbind
    }
}

void AttractorRenderer::leap_update(const LEAP_TRACKING_EVENT& frame, glm::vec3& cam_pos, glm::vec3& cam_target, float& yaw, float& pitch){
    //todo voyage arround the seed
    static bool left_was_pinched = false;
    static glm::quat left_start_rotation;

    glm::vec3 pinky_right, pinky_left;
    for (int i = 0; i < frame.nHands; i++){
        //store pinky of both hand to generate a new seed when they touch

        if (frame.pHands[i].type == eLeapHandType_Right && frame.pHands[i].confidence > 0.1){
            LEAP_HAND& hand = frame.pHands[i];
            pinky_right = glm::make_vec3(hand.pinky.distal.next_joint.v);

            if (hand.pinch_distance < 25){
                //move parameter only if pinch
                glm::vec3 vec_thumb = glm::normalize(
                    glm::make_vec3(hand.thumb.distal.next_joint.v)
                    - glm::make_vec3(hand.thumb.metacarpal.prev_joint.v)
                );
                // glm::vec3 vec_index = glm::normalize(
                //     glm::make_vec3(hand.index.distal.next_joint.v)
                //     - glm::make_vec3(hand.index.metacarpal.prev_joint.v)
                // );
                glm::vec3 vec_middle = glm::normalize(
                    glm::make_vec3(hand.middle.distal.next_joint.v)
                    -glm::make_vec3(hand.middle.metacarpal.prev_joint.v)
                );
                glm::vec3 vec_ring = glm::normalize(
                    glm::make_vec3(hand.ring.distal.next_joint.v)
                    - glm::make_vec3(hand.ring.metacarpal.prev_joint.v)
                );
                glm::vec3 vec_pinky = glm::normalize(
                    glm::make_vec3(hand.pinky.distal.next_joint.v)
                    - glm::make_vec3(hand.pinky.metacarpal.prev_joint.v)
                );
                glm::vec3 vec_arm = glm::normalize(
                    glm::make_vec3(hand.arm.next_joint.v)
                    - glm::make_vec3(hand.arm.prev_joint.v)
                );

                leapInfos[0].ofs_axis = 0.9f * leapInfos[0].ofs_axis + 0.1f * glm::normalize(vec_middle- vec_ring);
                leapInfos[1].ofs_axis = 0.9f * leapInfos[1].ofs_axis + 0.1f * glm::normalize(vec_ring - vec_pinky);
                leapInfos[2].ofs_axis = 0.9f * leapInfos[2].ofs_axis + 0.1f * glm::normalize(vec_pinky - vec_middle);

                leapInfos[0].ofs_translate = 0.9f * leapInfos[0].ofs_translate + 0.1f * maprange(abs(vec_thumb.x),0,1,0.7,1.3);
                leapInfos[1].ofs_translate = 0.9f * leapInfos[1].ofs_translate + 0.1f * maprange(abs(vec_thumb.y),0,1,0.7,1.3);
                leapInfos[2].ofs_translate = 0.9f * leapInfos[2].ofs_translate + 0.1f * maprange(abs(vec_thumb.z),0,1,0.7,1.3);

                leapInfos[0].ofs_rotate_angle = 0.9f * leapInfos[0].ofs_rotate_angle + 0.1f *maprange(abs(vec_arm.x),0,1,-2,2);
                leapInfos[1].ofs_rotate_angle = 0.9f * leapInfos[1].ofs_rotate_angle + 0.1f *maprange(abs(vec_arm.y),0,1,-2,2);
                leapInfos[2].ofs_rotate_angle = 0.9f * leapInfos[2].ofs_rotate_angle + 0.1f *maprange(abs(vec_arm.z),0,1,-2,2);

                forceRedraw = true;
            }  
        }
        if (frame.pHands[i].type == eLeapHandType_Left && frame.pHands[i].confidence > 0.1)
        {
            pinky_left = glm::make_vec3(frame.pHands[i].pinky.distal.next_joint.v);
            if (frame.pHands[i].pinch_distance < 25)
            {
                const glm::quat palm_orientation = glm::make_quat(frame.pHands[i].palm.orientation.v);
                if (!left_was_pinched)
                {
                    left_was_pinched = true;
                    left_start_rotation = palm_orientation;
                }
                // Update camera position instead of fractal position
                cam_pos -= glm::rotateY(glm::vec3(0.0f, 0.0f, frame.pHands[i].palm.velocity.z),glm::radians(yaw)) * 5e-4f;
                // cam_target -= glm::rotateY(glm::vec3(frame.pHands[i].palm.velocity.x, frame.pHands[i].palm.velocity.y, 0.0f), glm::radians(yaw)) * 5e-4f;
                
                // Extract rotation changes from the palm orientation
                glm::quat rotation_change = palm_orientation * glm::inverse(left_start_rotation);
            
                // Convert the quaternion rotation to changes in pitch and yaw
                // Extract a simplified rotation around X (pitch) and Y (yaw) axes
                glm::vec3 euler = glm::eulerAngles(rotation_change);
                const auto sav = glm::length(cam_pos);
                // pitch -= glm::degrees(euler.x * 0.9f);
                yaw -= glm::degrees(euler.y * 0.9f);
                // auto pitchRad = glm::radians(-pitch);
                auto yawRad = glm::radians(yaw);
                glm::vec3 direction{sin(yawRad) /** cos(pitchRad)*/, /*sin(pitchRad)*/ 0, cos(yawRad) /** cos(pitchRad)*/};
                cam_pos = cam_target + direction * sav;
                left_start_rotation = palm_orientation;
                left_start_rotation = palm_orientation;
            }
            else
            {
                left_was_pinched = false;
            }
        }
        else
        {
            left_was_pinched = false;
        }
    }
    //start from a new random seed if both pinky are close enough
    if(glm::distance(pinky_left, pinky_right)< 35){
        attractorA.setRandom(matrix_per_attractor);
        attractorB.setRandom(matrix_per_attractor);
    }
}
void AttractorRenderer::draw_ui(float& speed,glm::vec3& pos){
    ImGui::Begin("attractor");
    if(ImGui::CollapsingHeader("Debug & all", ImGuiTreeNodeFlags_DefaultOpen )){
        if(ImGui::SliderFloat("##lerpfactor", &lerp_factor, 0.0f, 1.0f, "lerp : %.3f")) forceRedraw = true;
            HelpMarker("Lerp between [0+x; 1-x] (stop before reaching 0 or 1)"
                "relevant when using more than 3 functions per attractors"
                "keep it 0 if you don't know what your doing");
        const char* items_cb2[] = { "per matrix", "per component", "leapMotion & hand"}; //MUST MATCH ENUM "LerpMode" !
        if(ImGui::Combo("lerping mode", (int*)&lerp_mode, items_cb2, IM_ARRAYSIZE(items_cb2))){
        }
        if(lerp_mode == FromLeapLerp){
            leapFacs.ui();
        }
        
        ImGui::Checkbox("no clear", &no_clear);
            HelpMarker("Do not clear previous frame if view didn't changed");
        ImGui::Checkbox("Idle animation", &idle);
            HelpMarker("camera enter an idle spinning animation if checked");

        if(ImGui::TreeNode("Idle params")){
            ImGui::DragFloat("Spin period", &spin_period, 0.01f, 3.0f,10.0f,"%.2f");
            ImGui::DragFloat("Lerp period", &lerp_period, 0.01f, 3.0f,10.0f,"%.2f");
            ImGui::DragFloat2("heigh & distance", height_and_distance, 0.01f,-3.0f,3.0f,"%.2f");
            ImGui::DragFloat("Lerp stiffness", &lerp_stiffness, 0.01f, 0.5f,20.0f,"%.2f");
            HelpMarker("The stifness of the smoothing curve"
                "The function is a sigmoid mapranged to range 0-1, ie :"
                "(1/(1+exp(-k(2x-1))) - mv) * 1/(1-2mv)"
                "where mv is the value at 0,  1+(1+exp(k))");
            ImGui::TreePop();
        }
        if(ImGui::TreeNode("random range")){
            HelpMarker("The lower / upper born at range random parameter will be generated");
            ImGui::DragFloat2("max scaling",prm.scale, 0.005f, 0.2f, 1.5f, "%.3f");
            ImGui::DragInt("max rotation", &prm.angle, 1, 0, 180, "%d");
            ImGui::DragFloat("max shearing", &prm.shearing, 0.005f, 0.0f, 2.0f, "%.2f");
            ImGui::DragFloat("max translate", &prm.translation, 0.005f, 0.0f, 2.0f, "%.2f");
            ImGui::TreePop();
        }
    }
    if(ImGui::TreeNode("Other Utils")){
        if(ImGui::Button("pref Speed")) speed = 0.025f;
        if(ImGui::Button("reset cam")) pos = glm::vec3(0.0,0.0,-0.5);
        if(ImGui::Button("reset default")) default_values();

        ImGui::TreePop();
    }

    if(ImGui::CollapsingHeader("Cool preset")){
        if(ImGui::Button("Sierpinski")) sierpinski();
        if(ImGui::Button("Sierpintagon")) sierpintagon();
        if(ImGui::Button("Sierpolygon")) sierpolygon(); 
        SL ImGui::SetNextItemWidth(60); ImGui::DragInt("side", &nb_cote, 1, 3, 10, "%d", ImGuiSliderFlags_AlwaysClamp);
        if(ImGui::Button("Barnsley Fern")) barsnley_fern(); 
        if(ImGui::Button("Romanesco")) romanesco();
    }


    if(ImGui::CollapsingHeader("Attractors")){
        ImGui::SliderInt("nb functions", &matrix_per_attractor, 3, MAX_FUNC_PER_ATTRACTOR);
            HelpMarker("The number of different random matrices per attractor. going above ten will crash the program");
        
        if (ImGui::TreeNode("Weights")){
            for(int i=0; i<matrix_per_attractor; i++){
                ImGui::Text("\t%.3f weight %d",ubo_weights[i], i);
            }
            ImGui::TreePop();
        }


        if (ImGui::TreeNode("Attractor A")){
            attractorA.ui(matrix_per_attractor);

            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Attractor B")){
            attractorB.ui(matrix_per_attractor);

            ImGui::TreePop();
        }
    }
    if(ImGui::CollapsingHeader("Coloring", ImGuiTreeNodeFlags_DefaultOpen )){
        ImGui::Text("temporary, must be hardcoded when it'll look nice");
        
        if(ImGui::TreeNode("Presets & debugs")){
            if(ImGui::Button("show only ssao")) ssaoOnlyValues();
            const char* items_cb3[] = { "v1", "v2", "v3", "v4","v5"};
            ImGui::Combo("SSAO shading version", &ssao_version, items_cb3, IM_ARRAYSIZE(items_cb3));
                HelpMarker("One of the few SSAO~ish implementation we've done");
            ImGui::Checkbox("skip update samples", &skipSampleUpdt);
                HelpMarker("if checked, do not generate new random samples position in halfsphere for v4 and v5");
                
            ImGui::TreePop();
        }

        if(ImGui::TreeNode("Jump Distance MapRange")){
            ImGui::ColorEdit3("BG COLOR", glm::value_ptr(bg_color));
            ImGui::ColorEdit3("jd low color", glm::value_ptr(col_jd_low));
            ImGui::ColorEdit3("jd high color", glm::value_ptr(col_jd_high));
            ImGui::DragFloat("from min",&JD_FR_MIN, 0.005f, 0.0f, 2.0f, "%.2f");
            ImGui::DragFloat("from max",&JD_FR_MAX, 0.005f, 0.0f, 2.0f, "%.2f");

            ImGui::TreePop();
        }
        if(ImGui::TreeNode("Phong parameters")){
            ImGui::SliderFloat("k_a##attractor", &k_a, 0.0f, 1.0f);
            ImGui::SliderFloat("k_d##attractor", &k_d, 0.0f, 1.0f);
            ImGui::SliderFloat("k_s##attractor", &k_s, 0.0f, 1.0f);
            ImGui::SliderFloat("alpha##attractor", &alpha, 0.1f, 20.0f);
            ImGui::ColorEdit3("specular##attractor", glm::value_ptr(col_specular));

            ImGui::TreePop();
        }
        if(ImGui::TreeNode("Ambient Occlusions")){
            ImGui::SliderFloat("AO size##attractor", &ao_size, 0.0f, 1.0f);
                HelpMarker("The size of the square in witch AO will be sampled");
            ImGui::SliderFloat("AO factor##attractor", &ao_fac, -0.025f, 0.025f);
                HelpMarker("A multiplicative factor applied to ambient occlusion"
                    "to have darkenning AO, just set color to white and ao_fac to negative");
            ImGui::ColorEdit3("ao colors##attractor", glm::value_ptr(col_ao));

            ImGui::TreePop();
        }
    }

    if(ImGui::Button("Hot reload shaders")){
        GLuint new_program = glCreateProgram();
        GLint comp = loadshader("shaders/attractor.comp", GL_COMPUTE_SHADER);
        if(comp>0){
            glAttachShader(new_program, comp);
            if(linkProgram(new_program)){
                attractor_program=new_program;
            }
            else{
                glDeleteProgram(new_program);
            }
        }
        else{
            glDeleteProgram(new_program);
        }
        new_program = glCreateProgram();
        comp = loadshader("shaders/attractor_shading.comp", GL_COMPUTE_SHADER);
        if(comp>0){
            glAttachShader(new_program, comp);
            if(linkProgram(new_program)){
                shading_program=new_program;
            }
            else{
                glDeleteProgram(new_program);
            }
        }
        else{
            glDeleteProgram(new_program);
        }
    }
    ImGui::End();
}
void AttractorRenderer::resize(float w,float h){
    glBindTexture(GL_TEXTURE_2D, depth_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32I, w, h, 0, GL_RED_INTEGER, 
                GL_INT, nullptr);

    glBindTexture(GL_TEXTURE_2D, jumpdist_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32I, w, h, 0, GL_RED_INTEGER,
                GL_INT, nullptr);
}

void AttractorRenderer::render(float width,float height, glm::vec3& pos,glm::mat4& inv_view, glm::mat4& proj, glm::vec3& light_pos){

    if(!no_clear || inv_view!=old_view || forceRedraw){
        int depth_clear = INT_MIN;
        glClearTexImage(depth_texture,0, GL_RED_INTEGER,GL_INT,&depth_clear);
        glClearTexImage(jumpdist_texture,0, GL_RED_INTEGER,GL_INT,&depth_clear);
        forceRedraw = false;
    }

    //overwrite camera view if idling
    if(idle){
        //some optimizing could be done about inversing multiples times camera view
        inv_view = glm::inverse(get_idle_view((float)glfwGetTime()));
    }
    
    
    old_view=inv_view;
    glUseProgram(attractor_program);

    glUniformMatrix4fv(glGetUniformLocation(attractor_program, "view"),1, GL_FALSE, glm::value_ptr(glm::inverse(inv_view)));
    glUniformMatrix4fv(glGetUniformLocation(attractor_program, "proj"),1, GL_FALSE, glm::value_ptr(proj));
    glUniform2ui(glGetUniformLocation(attractor_program, "screen_size"), width,height);
    glUniform3fv(glGetUniformLocation(attractor_program, "camera"), 1, glm::value_ptr(pos));
    glUniform1i(glGetUniformLocation(attractor_program, "matrix_per_attractor"),matrix_per_attractor);
    glUniform1ui(glGetUniformLocation(attractor_program, "rand_seed"),rand()%RAND_MAX);
    glUniform1ui(glGetUniformLocation(attractor_program, "NBPTS"),nbpts_gpu);
    
    //send attractor data to compute shader
    update_ubo_matrices();
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, matrix_per_attractor * sizeof(glm::mat4), ubo_matrices.data());

    float weights[MAX_FUNC_PER_ATTRACTOR*4];
    for(int i = 0;i<matrix_per_attractor;i++)
    {
        weights[i*4]=ubo_weights[i];
    }
    glBufferSubData(GL_UNIFORM_BUFFER, MAX_FUNC_PER_ATTRACTOR * sizeof(glm::mat4), matrix_per_attractor * sizeof(float)*4, weights);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    {//update ubo samples in halfsphere
        glm::vec4 samples[HALFSPHERE_SAMPLES];
        update_ubo_samples(samples);
        glBindBuffer(GL_UNIFORM_BUFFER, ubo_samples);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, HALFSPHERE_SAMPLES * sizeof(glm::vec4), samples);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    
    glDispatchCompute((nbpts_gpu-1)/1024+1, 1, 1);

    // make sure writing to image has finished before read
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);


    glUseProgram(shading_program);
    glUniform2ui(glGetUniformLocation(shading_program, "screen_size"), width,height);
    glUniformMatrix4fv(glGetUniformLocation(shading_program, "inv_view"),1, GL_FALSE, glm::value_ptr(inv_view));
    glUniformMatrix4fv(glGetUniformLocation(shading_program, "inv_proj"),1, GL_FALSE, glm::value_ptr(glm::inverse(proj)));
    glUniform3fv(glGetUniformLocation(shading_program, "camera"), 1, glm::value_ptr(pos)); //here todo
    glUniform1f(glGetUniformLocation(shading_program, "JD_FR_MIN"), JD_FR_MIN);
    glUniform1f(glGetUniformLocation(shading_program, "JD_FR_MAX"), JD_FR_MAX);
    glUniform3fv(glGetUniformLocation(shading_program, "BGCOLOR"), 1, glm::value_ptr(bg_color));
    glUniform3fv(glGetUniformLocation(shading_program, "col_jd_low"), 1, glm::value_ptr(col_jd_low));
    glUniform3fv(glGetUniformLocation(shading_program, "col_jd_high"), 1, glm::value_ptr(col_jd_high));
    glUniform1f(glGetUniformLocation(shading_program, "k_a"), k_a);
    glUniform1f(glGetUniformLocation(shading_program, "k_d"), k_d);
    glUniform1f(glGetUniformLocation(shading_program, "k_s"), k_s);
    glUniform1f(glGetUniformLocation(shading_program, "alpha"), alpha);
    glUniform3fv(glGetUniformLocation(shading_program, "col_specular"), 1, glm::value_ptr(col_specular));
    glUniform1i(glGetUniformLocation(shading_program, "ssao_version"), ssao_version);
    glUniform1f(glGetUniformLocation(shading_program, "ao_fac"), ao_fac);
    glUniform1f(glGetUniformLocation(shading_program, "ao_size"), ao_size);
    glUniform3fv(glGetUniformLocation(shading_program, "col_ao"), 1, glm::value_ptr(col_ao));
    glUniform3fv(glGetUniformLocation(shading_program, "light_pos"), 1, glm::value_ptr(light_pos));
    glUniform1f(glGetUniformLocation(shading_program, "time"), glfwGetTime());

    glDispatchCompute((width-1)/32+1, (height-1)/32+1, 1);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}
