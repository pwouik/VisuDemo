#include "attractor_renderer.h"
#include "glm/fwd.hpp"
#include "glm/matrix.hpp"
#include "imgui/imgui.h"
#include "imgui_util.hpp"
#include "opengl_util.h"
#include <LeapC.h>
#include <cstdio>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <GLFW/glfw3.h>

#define NBPTS 20'000'000

void randArray(float* array, int size, float range){
    for(int i=0; i<size; i+=4){
        array[i] = range * (((float)rand()/RAND_MAX) *2 -1);
        array[i+1] = range * (((float)rand()/RAND_MAX) *2 -1);
        array[i+2] = range * (((float)rand()/RAND_MAX) *2 -1);
        array[i+3] = 1;
    }
}

void origindArray(float* array, int size){
    for(int i=0; i<size; i+=4){
        //which is x, whihc is y, which is z, which is w ?
        array[i] = 0.0f; 
        array[i+1] = 0.0f;
        array[i+2] = 0.0f;
        array[i+3] = 1.0f;
    }
}

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

void AttractorRenderer::init_data(int w, int h){

    //generates points SSBO
    float* data = new float[NBPTS*4];
    randArray(data, NBPTS, 1);
    //origindArray(data, NBPTS);
    glGenBuffers(1, &ssbo_pts);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_pts);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * 4 * NBPTS, data, GL_DYNAMIC_DRAW); //GL_DYNAMIC_DRAW update occasionel et lecture frequente
    //glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data); //to update partially
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssbo_pts);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // unbind

    delete[] data; //points memory only needed on GPU

    //attractors (a max of 10 matrices stored as UBO)
    glGenBuffers(1, &uboM4);
    glBindBuffer(GL_UNIFORM_BUFFER, uboM4);
    glBufferData(GL_UNIFORM_BUFFER, 10 * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW); // Allocate space for up to 10 matrices
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, uboM4); // Binding point 0
    glBindBuffer(GL_UNIFORM_BUFFER, 0); // Unbind

    //reserve matrices to be pushed to UBO each frame
    ubo_matrices.reserve(10);
    transformInit();
}

void AttractorRenderer::reset(){
    attractorA.freeAll();
    attractorB.freeAll();
    ubo_matrices.clear();
    ubo_matrices.reserve(10);
}

void AttractorRenderer::update_ubo_matrices(LerpMode mode){
    switch (mode)
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
    default:
        throw std::runtime_error("Unhandled lerping mode");
        break;
    }
    
    //debug todo remove
    // if(matrixPerAttractor == 0) return;
    // ubo_matrices[0] = A_tractor.attr_funcs[0]->getMatrix();
    // ubo_matrices[1] = A_tractor.attr_funcs[1]->getMatrix();
    // ubo_matrices[2] = A_tractor.attr_funcs[2]->getMatrix();
    
    //debug todo remove
    // glm::mat4 mat = A_tractor.attr_funcs[0]->getMatrix();
    // std::cout << "\n\nfoo\n\n";
    // for (int row = 0; row < 4; ++row) {
    //     std::cout << "\t" << mat[row][0]<< "\t" << mat[row][1]<< "\t" << mat[row][2]<< "\t" << mat[row][3] << "\n";
    // }

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
    }
    
    matrix_per_attractor = 3; //can be any value between 3 or 10

    printf("done\n");
}

void AttractorRenderer::allIdentity(){
    for(int i=0; i<MAX_FUNC_PER_ATTRACTOR; i++){
        attractorA.attr_funcs[i]->setIdentity();
        attractorB.attr_funcs[i]->setIdentity();
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
float smooth_curve(float v){
    return v*v*3-v*v*v*2;
}
glm::mat4 AttractorRenderer::get_idle_view(float time){
    float angle = 2*PI*time / spin_period;
    float intpart;
    lerp_factor = smooth_curve(std::modf(time/lerp_period, &intpart));
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
AttractorRenderer::AttractorRenderer(int w,int h){

    //compute_program for attractor
    attractor_program = glCreateProgram();
    GLint comp_attractor = loadshader("shaders/attractor.comp", GL_COMPUTE_SHADER);
    glAttachShader(attractor_program, comp_attractor);
    glLinkProgram(attractor_program);
    linkProgram(attractor_program);

    shading_program = glCreateProgram();
    GLint ssao_shader = loadshader("shaders/attractor_shading.comp", GL_COMPUTE_SHADER);
    glAttachShader(shading_program, ssao_shader);
    glLinkProgram(shading_program);
    linkProgram(shading_program);


    //depth texture (texture1, binding 1)
    glGenTextures(1, &depth_texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depth_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32I, w, h, 0, GL_RED_INTEGER, GL_INT, nullptr);
    //required for some reason on my computer
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glBindImageTexture(1, depth_texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32I); //bind to channel 1

    //distance from last jump texture (texture 2, binding4)
    glGenTextures(1, &jumpdist_texture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, jumpdist_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32I, w, h, 0, GL_RED_INTEGER, GL_INT, nullptr); 
    //required for some reason on my computer
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0); 
    glBindImageTexture(4, jumpdist_texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32I); //bind to channel 4

}
void AttractorRenderer::leap_update(const LEAP_TRACKING_EVENT& frame){

}
void AttractorRenderer::draw_ui(float& speed,glm::vec3& pos){
    ImGui::Begin("attractor");
    if(ImGui::CollapsingHeader("Debug & all", ImGuiTreeNodeFlags_DefaultOpen )){
        ImGui::SliderFloat("##lerpfactor", &lerp_factor, 0.0f, 1.0f, "lerp : %.3f");
            HelpMarker("Lerp between [0+x; 1-x] (stop before reaching 0 or 1)"
                "relevant when using more than 3 functions per attractors"
                "keep it 0 if you don't know what your doing");
        const char* items_cb2[] = { "per matrix", "per component"}; //MUST MATCH ENUM !
        if(ImGui::Combo("lerping mode", (int*)&lerp_mode, items_cb2, IM_ARRAYSIZE(items_cb2))){}
        
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
            ImGui::DragFloat2("max scaling",prm::scale, 0.005f, 0.2f, 1.5f, "%.3f");
            ImGui::DragInt("max rotation", &prm::angle, 1, 0, 180, "%d");
            ImGui::DragFloat("max shearing", &prm::shearing, 0.005f, 0.0f, 2.0f, "%.2f");
            ImGui::DragFloat("max translate", &prm::translation, 0.005f, 0.0f, 2.0f, "%.2f");
            ImGui::TreePop();
        }
    }
    if(ImGui::TreeNode("Other Utils")){
        if(ImGui::Button("pref Speed")) speed = 0.025f;
        if(ImGui::Button("reset cam")) pos = glm::vec3(0.0,0.0,-0.5);

        ImGui::TreePop();
    }

    if(ImGui::CollapsingHeader("Cool preset")){
        if(ImGui::Button("Sierpinski")) sierpinski();
        if(ImGui::Button("Sierpintagon")) sierpintagon();
        if(ImGui::Button("Sierpolygon")) sierpolygon(); 
        SL ImGui::SetNextItemWidth(60); ImGui::DragInt("side", &nb_cote, 1, 3, 10, "%d", ImGuiSliderFlags_AlwaysClamp);
        if(ImGui::Button("Barnsley Fern")) barsnley_fern(); 
    }


    if(ImGui::CollapsingHeader("Attractors")){
        ImGui::SliderInt("nb functions", &matrix_per_attractor, 3, 10);
            HelpMarker("The number of different random matrices per attractor. going above ten will crash the program");
        
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
        if(ImGui::TreeNode("Jump Distance MapRange")){
            ImGui::ColorEdit3("jd low color", glm::value_ptr(col_jd_low));
            ImGui::ColorEdit3("jd high color", glm::value_ptr(col_jd_high));
            ImGui::DragFloat("from min",&JD_FR_MIN, 0.005f, 0.0f, 2.0f, "%.2f");
            ImGui::DragFloat("from max",&JD_FR_MAX, 0.005f, 0.0f, 2.0f, "%.2f");
            //goes to 0-1 so useless
            //ImGui::DragFloat("to min",&JD_TO_MIN, 0.005f, 0.0f, 2.0f, "%.2f");
            //ImGui::DragFloat("to max",&JD_TO_MAX, 0.005f, 0.0f, 2.0f, "%.2f");

            ImGui::TreePop();
        }
        if(ImGui::TreeNode("Phong parameters")){
            ImGui::SliderFloat("k_a##attractor", &k_a, 0.0f, 1.0f);
            //ImGui::ColorEdit3("ambient##attractor", glm::value_ptr(col_ambient));
            ImGui::SliderFloat("k_d##attractor", &k_d, 0.0f, 1.0f);
            //ImGui::ColorEdit3("diffuse##attractor", glm::value_ptr(col_diffuse));
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

void AttractorRenderer::render(float width,float height,glm::vec3 pos,glm::mat4 inv_view, glm::mat4 proj, glm::vec3 light_pos, glm::vec3 fractal_position,glm::quat fractal_rotation){

    if(no_clear || inv_view!=old_view){
        int depth_clear = INT_MIN;
        glClearTexImage(depth_texture,0, GL_RED_INTEGER,GL_INT,&depth_clear);
        glClearTexImage(jumpdist_texture,0, GL_RED_INTEGER,GL_INT,&depth_clear);
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
    glUniform3fv(glGetUniformLocation(attractor_program, "light_pos"), 1, glm::value_ptr(light_pos));
    glUniform1f(glGetUniformLocation(attractor_program, "time"), glfwGetTime());
    glUniform1i(glGetUniformLocation(attractor_program, "matrixPerAttractor"),matrix_per_attractor);
    glUniform1i(glGetUniformLocation(attractor_program, "randInt_seed"),rand()%RAND_MAX);
    
    //send attractor data to compute shader
    update_ubo_matrices(lerp_mode);
    glBindBuffer(GL_UNIFORM_BUFFER, uboM4);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, matrix_per_attractor * sizeof(glm::mat4), ubo_matrices.data());
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    
    glDispatchCompute((NBPTS-1)/1024+1, 1, 1);

    // make sure writing to image has finished before read
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);


    glUseProgram(shading_program);
    glUniform2ui(glGetUniformLocation(shading_program, "screen_size"), width,height);
    glUniformMatrix4fv(glGetUniformLocation(shading_program, "inv_view"),1, GL_FALSE, glm::value_ptr(inv_view));
    glUniformMatrix4fv(glGetUniformLocation(shading_program, "inv_proj"),1, GL_FALSE, glm::value_ptr(glm::inverse(proj)));
    glUniform3fv(glGetUniformLocation(shading_program, "camera"), 1, glm::value_ptr(pos)); //here todo
    glUniform1f(glGetUniformLocation(shading_program, "JD_FR_MIN"), JD_FR_MIN);
    glUniform1f(glGetUniformLocation(shading_program, "JD_FR_MAX"), JD_FR_MAX);
    // glUniform1f(glGetUniformLocation(shading_program, "JD_TO_MIN"), JD_TO_MIN);
    // glUniform1f(glGetUniformLocation(shading_program, "JD_TO_MAX"), JD_TO_MAX);
    glUniform3fv(glGetUniformLocation(shading_program, "col_jd_low"), 1, glm::value_ptr(col_jd_low));
    glUniform3fv(glGetUniformLocation(shading_program, "col_jd_high"), 1, glm::value_ptr(col_jd_high));
    glUniform1f(glGetUniformLocation(shading_program, "k_a"), k_a);
    //glUniform3fv(glGetUniformLocation(shading_program, "col_ambient"), 1, glm::value_ptr(col_ambient));
    glUniform1f(glGetUniformLocation(shading_program, "k_d"), k_d);
    //glUniform3fv(glGetUniformLocation(shading_program, "col_diffuse"), 1, glm::value_ptr(col_diffuse));
    glUniform1f(glGetUniformLocation(shading_program, "k_s"), k_s);
    glUniform1f(glGetUniformLocation(shading_program, "alpha"), alpha);
    glUniform3fv(glGetUniformLocation(shading_program, "col_specular"), 1, glm::value_ptr(col_specular));
    glUniform1f(glGetUniformLocation(shading_program, "ao_fac"), ao_fac);
    glUniform1f(glGetUniformLocation(shading_program, "ao_size"), ao_size);
    glUniform3fv(glGetUniformLocation(shading_program, "col_ao"), 1, glm::value_ptr(col_ao));

    glDispatchCompute((width-1)/32+1, (height-1)/32+1, 1);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}