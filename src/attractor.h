#pragma once
#include "glm/glm.hpp"
#include "glm/ext.hpp"
#include "imgui/imgui.h"
#include "imgui_util.hpp"


#define PI 3.14159265358979323846264338327950f
#define MAX_FUNC_PER_ATTRACTOR 10

static float randf(){
    return (float)rand()/RAND_MAX;
}

static struct RAND_RANGE{
    float scale[2] = {0.4f, 0.9f}; //min, max
    int angle = 180;
    float shearing = 0.2;
    float translation = 0.8;
} prm;

class Transform{
public:
    glm::mat4 matrix;
    bool overwriteMatrix = false;

    glm::vec3 scale_factors;
    glm::vec3 rot_axis;
    float rot_angle;
    glm::vec3 Sh_xy_xz_yx;
    glm::vec3 Sh_yz_zx_zy;
    glm::vec3 translation_vector;

    void updt_matrix(){
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
    void setIdentity(){
        scale_factors = glm::vec3(1.0f);
        rot_axis = glm::vec3(1.0f,0.0f,0.0f);
        rot_angle = 0.0f;
        Sh_xy_xz_yx = glm::vec3(0.0f);
        Sh_yz_zx_zy = glm::vec3(0.0f);
        translation_vector = glm::vec3(0.0f);
        updt_matrix();
    }
    void setRandom(){
        scale_factors = glm::vec3(
            randf()*(prm.scale[1]-prm.scale[0])+prm.scale[0],
            randf()*(prm.scale[1]-prm.scale[0])+prm.scale[0],
            randf()*(prm.scale[1]-prm.scale[0])+prm.scale[0]
        );
        glm::vec3 randomVec = glm::ballRand(1.0f);
        rot_axis = glm::normalize(randomVec);
        rot_angle = glm::radians((randf()*2-1) * (float) prm.angle);
        Sh_xy_xz_yx = glm::vec3(
            (randf()*2-1)*prm.shearing,
            (randf()*2-1)*prm.shearing,
            (randf()*2-1)*prm.shearing
        );
        Sh_yz_zx_zy = glm::vec3(
            (randf()*2-1)*prm.shearing,
            (randf()*2-1)*prm.shearing,
            (randf()*2-1)*prm.shearing
        );
        translation_vector = glm::vec3(
            (randf()*2-1)*prm.translation,
            (randf()*2-1)*prm.translation,
            (randf()*2-1)*prm.translation
        );
        
        updt_matrix();
    }

    const char* getName(){
        return "Fixed process matrix";
    }
    void ui(){
        ImGui::Checkbox(UIDT("overwrite matrix", *this), &overwriteMatrix);
        if(ImGui::Button(UIDT("randomize func", *this))) setRandom();
        SL if(ImGui::Button(UIDT("set identity", *this))) setIdentity();
        if(!overwriteMatrix){
            valf("scale : ", scale_factors);
            valf("rot axis", rot_axis);
            valf("rota angle", rot_angle, 0.005f, -PI, PI);
            valf("shear xy xz yx", Sh_xy_xz_yx);
            valf("shear yz_zx_zy", Sh_yz_zx_zy);
            valf("translation", translation_vector);
        }
        else{
            valf("raw matrix : ", matrix);
        }

    }

    const glm::mat4& getMatrix(){
        if(!overwriteMatrix)
            updt_matrix();
        
        return matrix;
    }

    Transform(){
        setRandom();
    }
};

class Attractor{
public:
    Transform* attr_funcs[10];

    Attractor() {}

    void setRandom(size_t nb_func){
        for (size_t i = 0; i < nb_func; i++) {
            attr_funcs[i]->setRandom();
        }
    }

    void ui(size_t nb_func) {
        if(ImGui::Button(UIDT("randomize all", *this))) setRandom(nb_func);
        HelpMarker("Apllyable only if fixed process for now");
        for (size_t i = 0; i < nb_func; i++) {
            char buffer[32];  // Allocate a buffer for the formatted string
            std::snprintf(buffer, sizeof(buffer), "function %zu", i);
            if (ImGui::TreeNode(buffer)){
                //ImGui::SeparatorText(attr_funcs[i]->getName());
                //todo pop tree in ui below
                attr_funcs[i]->ui();

                ImGui::TreePop();
            }
        }
    }


    const glm::mat4& getMatrix(size_t index) const {            
        return attr_funcs[index]->getMatrix();
    }

    void freeAll(){
        for(size_t i=0; i < MAX_FUNC_PER_ATTRACTOR; i++){
            delete attr_funcs[i];
        }
    }
    
};

