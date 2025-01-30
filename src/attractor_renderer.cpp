#include "attractor_renderer.h"
#include "glm/fwd.hpp"
#include "imgui/imgui.h"
#include "opengl_util.h"
#include <LeapC.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <GLFW/glfw3.h>

//#include "imgui_util.hpp"

#include "app.h" //todo must be removed when refactoring done ! Also uncomment above !

#define MAX_FUNC_PER_ATTRACTOR 10
#define RNDF (float)rand()/RAND_MAX
#define PI 3.14159265358979323f //duplicate, move it idk where
#define NBPTS 20'000'000



AffineTransfo::AffineTransfo(){
    setRandom();
}

void AffineTransfo::updt_matrix(){
    matrix =
        glm::scale(glm::mat4(1.0f), scale_factors) *
        glm::rotate(glm::mat4(1.0f), rot_angle, rot_axis) *
        glm::transpose(glm::mat4(
            1.0f, Sh_xy_xz_yx.x,  Sh_xy_xz_yx.y, 0.0f,  //     1.0f, Shxy,  Shxz, 0.0f,
            Sh_xy_xz_yx.z, 1.0f,  Sh_yz_zx_zy.x, 0.0f,  //     Shyx, 1.0f,  Shyz, 0.0f,
            Sh_yz_zx_zy.y, Sh_yz_zx_zy.z,  1.0f, 0.0f,  //     Shzx, Shzy,  1.0f, 0.0f,
            0.0f, 0.0f,  0.0f, 1.0f)) *                 //     0.0f, 0.0f,  0.0f, 1.0f))
        glm::translate(glm::mat4(1.0f), translation_vector);
}

void AffineTransfo::setIdentity(){
    scale_factors = glm::vec3(1.0f);
    rot_axis = glm::vec3(1.0f,0.0f,0.0f);
    rot_angle = 0.0f;
    Sh_xy_xz_yx = glm::vec3(0.0f);
    Sh_yz_zx_zy = glm::vec3(0.0f);
    translation_vector = glm::vec3(0.0f);
    updt_matrix();
}

void AffineTransfo::setRandom(){
    scale_factors = glm::vec3(
        RNDF*(prm.scale[1]-prm.scale[0])+prm.scale[0],
        RNDF*(prm.scale[1]-prm.scale[0])+prm.scale[0],
        RNDF*(prm.scale[1]-prm.scale[0])+prm.scale[0]
    );
    glm::vec3 randomVec = glm::ballRand(1.0f);
    rot_axis = glm::normalize(randomVec);
    rot_angle = glm::radians((RNDF*2-1) * (float) prm.angle);
    Sh_xy_xz_yx = glm::vec3(
        (RNDF*2-1)*prm.shearing,
        (RNDF*2-1)*prm.shearing,
        (RNDF*2-1)*prm.shearing
    );
    Sh_yz_zx_zy = glm::vec3(
        (RNDF*2-1)*prm.shearing,
        (RNDF*2-1)*prm.shearing,
        (RNDF*2-1)*prm.shearing
    );
    translation_vector = glm::vec3(
        (RNDF*2-1)*prm.translation,
        (RNDF*2-1)*prm.translation,
        (RNDF*2-1)*prm.translation
    );
    
    updt_matrix();
}

void AffineTransfo::ui(){
    ImGui::Checkbox(UIDT("overwrite matrix", *this), &overwriteMatrix);
    if(ImGui::Button(UIDT("randomize func", *this))) setRandom();
    SL if(ImGui::Button(UIDT("set identity", *this))) setIdentity();
    if(!overwriteMatrix){
        ui::valf("scale : ", scale_factors);
        ui::valf("rot axis", rot_axis);
        ui::valf("rota angle", rot_angle, 0.005f, -PI, PI);
        ui::valf("shear xy xz yx", Sh_xy_xz_yx);
        ui::valf("shear yz_zx_zy", Sh_yz_zx_zy);
        ui::valf("translation", translation_vector);
    }
    else{
        ui::valf("raw matrix : ", matrix);
    }

}

const glm::mat4& AffineTransfo::getMatrix(){
    if(!overwriteMatrix)
        updt_matrix();
    
    return matrix;
}



Attractor::Attractor(){}

void Attractor::setRandom(size_t nb_func){
    for (size_t i = 0; i < nb_func; i++){
        attr_funcs[i]->setRandom();
    }
}

void Attractor::ui(size_t nb_func) {
    if(ImGui::Button(UIDT("randomize all", *this))) setRandom(nb_func);
    ui::HelpMarker("Apllyable only if fixed process for now");
    for (size_t i = 0; i < nb_func; i++) {
        char buffer[32];  // Allocate a buffer for the formatted string
        std::snprintf(buffer, sizeof(buffer), "function %zu", i);
        if (ImGui::TreeNode(buffer)){
            attr_funcs[i]->ui();
            ImGui::TreePop();
        }
    }
}

const glm::mat4& Attractor::getMatrix(size_t index) const {            
    return attr_funcs[index]->getMatrix();
}

void Attractor::freeAll(){
    for(size_t i=0; i < MAX_FUNC_PER_ATTRACTOR; i++){
        delete attr_funcs[i];
    }
}



glm::mat4 AttractorRenderer::ANIM::getIdleView(float time){
    float angle = 2*PI*time / spin_Period;
    float intpart;
    parent->atr.lerpFactor = smooth_curve(std::modf(time/lerp_period, &intpart));
    if(intpart > (float)iter){ //a little messy but modf takes a pointer to float so ....
        iter = intpart;
        if(iter%2) parent->atr.B_tractor.setRandom(parent->atr.matrixPerAttractor);
        else parent->atr.A_tractor.setRandom(parent->atr.matrixPerAttractor);
    }
    parent->atr.lerpFactor = iter%2 ? parent->atr.lerpFactor : 1.0f - parent->atr.lerpFactor;

    glm::vec3 eye = glm::vec3(
        height_and_distance[1] * cos(angle),
        height_and_distance[0],
        height_and_distance[1] * sin(angle)
    );
    return glm::lookAt(eye,glm::vec3(0.0f),glm::vec3(0.0f,1.0f,0.0f));
}
inline float AttractorRenderer::ANIM::smooth_curve(float v){
        const float k = lerp_stiffness;
        const float os = parent->atr.lerpEdgeClamp;
        const float mv = 1.0f/(1+exp(k));
        //return (1.0f/(1+exp(-k*(2*v-1)) ) - mv) * (1.0f/(1-2*mv));
        //trust the math I swear this works
        return 0.5 + (1.0f/(1+exp(-k*(2*v-1)) ) - 0.5) * ( (0.5f-os)/(0.5-mv));
        
    }

void AttractorRenderer::ATR::update_ubo_matrices(){
    switch (lerpmode)
    {
    case lerp_Matrix:
        for(int i=0; i < matrixPerAttractor; i++){
            ubo_matrices[i] = matrixLerp(i);
        }
        break;
    case lerp_PerComponent:    
        for(int i=0; i < matrixPerAttractor; i++){
            ubo_matrices[i] = componentLerp(i);
        }
        break;     
    default:
        throw std::runtime_error("Unhandled lerping mode");
        break;
    }
}
glm::mat4 AttractorRenderer::ATR::matrixLerp(int index) {
    float t = lerpFactor;
    glm::mat4 matA = A_tractor.attr_funcs[index]->getMatrix();
    glm::mat4 matB = B_tractor.attr_funcs[index]->getMatrix();
    return (1.0f - t) * matA + t * matB;
}
glm::mat4 AttractorRenderer::ATR::componentLerp(int index) {
    float t = lerpFactor;
    AffineTransfo* genA = A_tractor.attr_funcs[index];
    AffineTransfo* genB = B_tractor.attr_funcs[index];

    
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

AttractorRenderer::AttractorRenderer(){}
AttractorRenderer::AttractorRenderer(void* temp){
    {//unholy hack
        appptr = temp;
        anim.parent = this; //keep
    }
    
    defaultsValues();

    {//compute programs
        compute_program_positions = glCreateProgram();
        GLint comp_attractor = loadshader("shaders/render_attractor.comp", GL_COMPUTE_SHADER);
        glAttachShader(compute_program_positions, comp_attractor);
        glLinkProgram(compute_program_positions);
        linkProgram(compute_program_positions);

        compute_program_shading = glCreateProgram();
        GLint ssao_shader = loadshader("shaders/ssao_attractor.comp", GL_COMPUTE_SHADER);
        glAttachShader(compute_program_shading, ssao_shader);
        glLinkProgram(compute_program_shading);
        linkProgram(compute_program_shading);
    }


    

    {//ssbo of points
        float* data = new float[NBPTS*4];
        randArray(data, NBPTS, 1);
        glGenBuffers(1, &ssbo_pts);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_pts);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * 4 * NBPTS, data, GL_DYNAMIC_DRAW); //GL_DYNAMIC_DRAW update occasionel et lecture frequente
        //glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data); //to update partially
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssbo_pts);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // unbind

        delete[] data; //points memory only needed on GPU
    }

    {//attractors (a max of 10 matrices stored as UBO)
        glGenBuffers(1, &uboM4);
        glBindBuffer(GL_UNIFORM_BUFFER, uboM4);
        glBufferData(GL_UNIFORM_BUFFER, MAX_FUNC_PER_ATTRACTOR * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW); // Allocate space for up to 10 matrices
        glBindBufferBase(GL_UNIFORM_BUFFER, 1, uboM4); // Binding point 0
        glBindBuffer(GL_UNIFORM_BUFFER, 0); // Unbind
    }

    { //assign value to UBO matrics
        DEBUG("starting Attractor assingment ...");
        atr.ubo_matrices.reserve(MAX_FUNC_PER_ATTRACTOR);
        AffineTransfo* mga[MAX_FUNC_PER_ATTRACTOR];
        AffineTransfo* mgb[MAX_FUNC_PER_ATTRACTOR];

        for(int i=0; i<MAX_FUNC_PER_ATTRACTOR; i++){
            mga[i] = new AffineTransfo();
            mgb[i] = new AffineTransfo();
            atr.A_tractor.attr_funcs[i] = mga[i];
            atr.B_tractor.attr_funcs[i] = mgb[i];
            atr.ubo_matrices.push_back(glm::mat4(1.0f));
        }
        
        atr.matrixPerAttractor = 3; //can be any value between 3 or 10
        DEBUG("done");
    }

}

void AttractorRenderer::draw_ui(){
    ImGui::Begin("attractor");
        if(ImGui::CollapsingHeader("Debug & all", ImGuiTreeNodeFlags_DefaultOpen )){
            ImGui::SliderFloat("##lerpfactor", &atr.lerpFactor, 0.0f, 1.0f, "lerp : %.3f");
            ImGui::SliderFloat("##SliderLerpToMin", &atr.lerpEdgeClamp, 0.0f, 0.5f, "lerp edge clamp: %.2f");
                ui::HelpMarker("Lerp between [0+x; 1-x] (stop before reaching 0 or 1)"
                    "relevant when using more than 3 functions per attractors"
                    "keep it 0 if you don't know what your doing");
            const char* items_cb2[] = { "per matrix", "per component"}; //MUST MATCH ENUM IN app.h !
            if(ImGui::Combo("lerping mode", (int*)&atr.lerpmode, items_cb2, IM_ARRAYSIZE(items_cb2))){}
            
            ImGui::Checkbox("no clear", &anim.no_clear);
                ui::HelpMarker("Do not clear previous frame if view didn't changed");
            ImGui::Checkbox("Idle animation", &anim.idle);
                ui::HelpMarker("camera enter an idle spinning animation if checked");

            if(ImGui::TreeNode("Idle params")){
                ImGui::DragFloat("Spin period", &anim.spin_Period, 0.01f, 3.0f,10.0f,"%.2f");
                ImGui::DragFloat("Lerp period", &anim.lerp_period, 0.01f, 3.0f,10.0f,"%.2f");
                ImGui::DragFloat2("heigh & distance", anim.height_and_distance, 0.01f,-3.0f,3.0f,"%.2f");
                ImGui::DragFloat("Lerp stiffness", &anim.lerp_stiffness, 0.01f, 0.5f,20.0f,"%.2f");
                ui::HelpMarker("The stifness of the smoothing curve"
                    "The function is a sigmoid mapranged to range 0-1, ie :"
                    "(1/(1+exp(-k(2x-1))) - mv) * 1/(1-2mv)"
                    "where mv is the value at 0,  1+(1+exp(k))");
                ImGui::TreePop();
            }
            //ui::param_settings(); TODO param settings ! must be a property of attractor_renderer


            //TODO other util : move into app::draw_ui
            // if(ImGui::TreeNode("Other Utils")){
            //     if(ImGui::Button("pref Speed")) speed = 0.025f;
            //     if(ImGui::Button("reset cam")) pos = glm::vec3(0.0,0.0,-0.5);

            //     ImGui::TreePop();
            // }
        }

        //TODO support presets
        if(ImGui::CollapsingHeader("Cool preset")){
            // if(ImGui::Button("Sierpinski")) preset::sierpinski();
            // if(ImGui::Button("Sierpintagon")) preset::sierpintagon();
            // if(ImGui::Button("Sierpolygon")) preset::sierpolygon(); 
            // SL ImGui::SetNextItemWidth(60); ImGui::DragInt("side", &preset::nb_cote, 1, 3, 10, "%d", ImGuiSliderFlags_AlwaysClamp);
            // if(ImGui::Button("Barnsley Fern")) preset::barsnley_fern(); 
        }


        if(ImGui::CollapsingHeader("Attractors")){
            ImGui::SliderInt("nb functions", &atr.matrixPerAttractor, 3, 10);
                ui::HelpMarker("The number of different random matrices per attractor. going above ten will crash the program");
            
            if (ImGui::TreeNode("Attractor A")){
                atr.A_tractor.ui(atr.matrixPerAttractor);

                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Attractor B")){
                atr.B_tractor.ui(atr.matrixPerAttractor);

                ImGui::TreePop();
            }
        }
        if(ImGui::CollapsingHeader("Coloring", ImGuiTreeNodeFlags_DefaultOpen )){
            ImGui::Text("temporary, must be hardcoded when it'll look nice");
            if(ImGui::TreeNode("Jump Distance MapRange")){
                ImGui::ColorEdit3("jd low color", glm::value_ptr(clr.col_jd_low));
                ImGui::ColorEdit3("jd high color", glm::value_ptr(clr.col_jd_high));
                ImGui::DragFloat("from min",&clr.JD_FR_MIN, 0.005f, 0.0f, 2.0f, "%.2f");
                ImGui::DragFloat("from max",&clr.JD_FR_MAX, 0.005f, 0.0f, 2.0f, "%.2f");

                ImGui::TreePop();
            }
            if(ImGui::TreeNode("Phong parameters")){
                ImGui::SliderFloat("k_a##attractor", &clr.k_a, 0.0f, 1.0f);
                ImGui::SliderFloat("k_d##attractor", &clr.k_d, 0.0f, 1.0f);
                ImGui::SliderFloat("k_s##attractor", &clr.k_s, 0.0f, 1.0f);
                ImGui::SliderFloat("alpha##attractor", &clr.alpha, 0.1f, 20.0f);
                ImGui::ColorEdit3("specular##attractor", glm::value_ptr(clr.col_specular));

                ImGui::TreePop();
            }
            if(ImGui::TreeNode("Ambient Occlusions")){
                ImGui::SliderFloat("AO size##attractor", &clr.ao_size, 0.0f, 1.0f);
                    ui::HelpMarker("The size of the square in witch AO will be sampled");
                ImGui::SliderFloat("AO factor##attractor", &clr.ao_fac, -0.025f, 0.025f);
                    ui::HelpMarker("A multiplicative factor applied to ambient occlusion"
                        "to have darkenning AO, just set color to white and ao_fac to negative");
                ImGui::ColorEdit3("ao colors##attractor", glm::value_ptr(clr.col_ao));

                ImGui::TreePop();
            }
        }

    ImGui::End();
}

void AttractorRenderer::render(float width,float height,glm::vec3& pos,glm::mat4& inv_camera_view, glm::mat4& old_view, glm::mat4& proj, glm::vec3& light_pos){
    //if(!anim.no_clear || inv_camera_view!=old_view){
    // bool areEqual = 
    //     glm::all(glm::epsilonEqual(inv_camera_view[0], old_view[0], 0.1f)) &&
    //     glm::all(glm::epsilonEqual(inv_camera_view[1], old_view[1], 0.1f)) &&
    //     glm::all(glm::epsilonEqual(inv_camera_view[2], old_view[2], 0.1f)) &&
    //     glm::all(glm::epsilonEqual(inv_camera_view[3], old_view[3], 0.1f));
    if(!anim.no_clear || inv_camera_view!=old_view){
        int depth_clear = INT_MIN;
        glClearTexImage(((App*)appptr)->depth_texture,0, GL_RED_INTEGER,GL_INT,&depth_clear);
        glClearTexImage(((App*)appptr)->jumpdist_texture,0, GL_RED_INTEGER,GL_INT,&depth_clear);
    }

    //overwrite camera view if idling
    if(anim.idle){
        //some optimizing could be done about inversing multiples times camera view
        inv_camera_view = glm::inverse(anim.getIdleView((float)glfwGetTime()));
    }
    
    old_view=inv_camera_view;
    glUseProgram(compute_program_positions);

    glUniformMatrix4fv(glGetUniformLocation(compute_program_positions, "view"),1, GL_FALSE, glm::value_ptr(glm::inverse(inv_camera_view)));
    glUniformMatrix4fv(glGetUniformLocation(compute_program_positions, "proj"),1, GL_FALSE, glm::value_ptr(proj));
    glUniform2ui(glGetUniformLocation(compute_program_positions, "screen_size"), width,height);
    glUniform3fv(glGetUniformLocation(compute_program_positions, "camera"), 1, glm::value_ptr(pos));
    glUniform3fv(glGetUniformLocation(compute_program_positions, "light_pos"), 1, glm::value_ptr(light_pos));
    glUniform1f(glGetUniformLocation(compute_program_positions, "time"), glfwGetTime());
    glUniform1i(glGetUniformLocation(compute_program_positions, "matrixPerAttractor"),atr.matrixPerAttractor);
    glUniform1i(glGetUniformLocation(compute_program_positions, "randInt_seed"),rand()%RAND_MAX);
    
    //send attractor data to compute shader
    atr.update_ubo_matrices();
    glBindBuffer(GL_UNIFORM_BUFFER, uboM4);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, atr.matrixPerAttractor * sizeof(glm::mat4), atr.ubo_matrices.data());
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    
    glDispatchCompute((NBPTS-1)/1024+1, 1, 1);

    // make sure writing to image has finished before read
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);


    glUseProgram(compute_program_shading);
    glUniform2ui(glGetUniformLocation(compute_program_shading, "screen_size"), width,height);
    glUniformMatrix4fv(glGetUniformLocation(compute_program_shading, "inv_view"),1, GL_FALSE, glm::value_ptr(inv_camera_view));
    glUniformMatrix4fv(glGetUniformLocation(compute_program_shading, "inv_proj"),1, GL_FALSE, glm::value_ptr(glm::inverse(proj)));
    glUniform3fv(glGetUniformLocation(compute_program_shading, "camera"), 1, glm::value_ptr(pos));
    glUniform1f(glGetUniformLocation(compute_program_shading, "JD_FR_MIN"), clr.JD_FR_MIN);
    glUniform1f(glGetUniformLocation(compute_program_shading, "JD_FR_MAX"), clr.JD_FR_MAX);
    glUniform3fv(glGetUniformLocation(compute_program_shading, "col_jd_low"), 1, glm::value_ptr(clr.col_jd_low));
    glUniform3fv(glGetUniformLocation(compute_program_shading, "col_jd_high"), 1, glm::value_ptr(clr.col_jd_high));
    glUniform1f(glGetUniformLocation(compute_program_shading, "k_a"), clr.k_a);
    glUniform1f(glGetUniformLocation(compute_program_shading, "k_d"), clr.k_d);
    glUniform1f(glGetUniformLocation(compute_program_shading, "k_s"), clr.k_s);
    glUniform1f(glGetUniformLocation(compute_program_shading, "alpha"), clr.alpha);
    glUniform3fv(glGetUniformLocation(compute_program_shading, "col_specular"), 1, glm::value_ptr(clr.col_specular));
    glUniform1f(glGetUniformLocation(compute_program_shading, "ao_fac"), clr.ao_fac);
    glUniform1f(glGetUniformLocation(compute_program_shading, "ao_size"), clr.ao_size);
    glUniform3fv(glGetUniformLocation(compute_program_shading, "col_ao"), 1, glm::value_ptr(clr.col_ao));

    glDispatchCompute((width-1)/32+1, (height-1)/32+1, 1);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void AttractorRenderer::defaultsValues(){
    anim.no_clear = true;
    anim.idle = false;
    anim.iter = 0;
    anim.spin_Period = 30.0f;
    anim.height_and_distance[0] = 2.0f;
    anim.height_and_distance[1] = 1.75f;
    anim.lerp_period = 3.0f;
    anim.lerp_stiffness = 3.5f;

    atr.matrixPerAttractor = 0;
    atr.lerpFactor = 0.0f;
    atr.lerpEdgeClamp = 0.0f;

    clr.JD_FR_MIN = 0.1f;
    clr.JD_FR_MAX = 0.9f;
    clr.col_jd_low = glm::vec3(0.9,0.8,0.15);
    clr.col_jd_high = glm::vec3(1.0,0.0,0.3);

    clr.k_a = 0.2f;
    clr.k_d = 1.0f;
    clr.k_s = 1.0f;
    clr.alpha = 10.0f;
    clr.col_specular = glm::vec3(0.7,0.7,0.7);

    clr.ao_fac = 0.008f;
    clr.ao_size = 0.2f;
    clr.col_ao = glm::vec3(0.1,0.2,0.8);
}

void AttractorRenderer::randArray(float* array, int size, float range){
    for(int i=0; i<size; i+=4){
        array[i] = range * (((float)rand()/RAND_MAX) *2 -1);
        array[i+1] = range * (((float)rand()/RAND_MAX) *2 -1);
        array[i+2] = range * (((float)rand()/RAND_MAX) *2 -1);
        array[i+3] = 1;
    }
}

