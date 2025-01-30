#include "attractor_renderer.h"
#include "glm/fwd.hpp"
#include "imgui/imgui.h"
#include "opengl_util.h"
#include <LeapC.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <GLFW/glfw3.h>

#include <imgui_util.hpp>

#define MAX_FUNC_PER_ATTRACTOR 10
#define RNDF (float)rand()/RAND_MAX
#define PI 3.14159265358979323f //duplicate, move it idk where



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



AttractorRenderer::AttractorRenderer(){
    defaultsValues();
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