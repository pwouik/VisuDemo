/**
 * Utility file containting function for representing attractor and lerping between them
 * 
 */
#pragma once

#include <imgui/imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>
#include <vector> //It may be possible to use array and template<int T>
#include <string>

#include <iostream> //TODO REMOVE

#include "app.h"

//magic value synced with render_attractor.comp
#define MAX_FUNC_PER_ATTRACTOR 10

//UI util
#define SL ImGui::SameLine();
#define CW ImGui::SetNextItemWidth(inputWidth);
#define UID(val) (std::string("##") + std::to_string(reinterpret_cast<uintptr_t>(&val))).c_str()
#define UIDT(txt, val) (std::string(txt) + std::string("##") + std::to_string(reinterpret_cast<uintptr_t>(&val))).c_str()
//#define UID(val) "##tantpis" // UID GENERATION ABOVE

#define RNDF (float)rand()/RAND_MAX

namespace prm{ //param for random generation
    static float scale[2]; //min, max
    static int angle;
    static float shearing;
    static float translation;

    void defaults(){
        scale[0] = 0.3f; scale[1] = 1.0f;
        angle = 180;
        shearing = 0.2f;
        translation = 0.8f;
    }

}

namespace ui{
    float inputWidth = 45.0f; //width for most input float

    //duplicate with the one imgui_util.hpp ...
    static void HelpMarker(const char* desc){
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::BeginItemTooltip())
        {
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    //editable field for float of float vector
    void valf(const char* desc, float& v, float speed = 0.005f, float minv = 0.0f, float maxv = 1.0f){
        ImGui::Text(desc);
        SL CW ImGui::DragFloat(UID(v), &v, speed, minv, maxv, "%.2f");
    }

    // void valf(const char* desc, glm::vec3& v, float minv = 0.0f, float maxv = 1.0f){
    //     ImGui::Text(desc);
    //     SL CW ImGui::InputFloat(UID(v.x), &v.x, minv, maxv, "%.2f");
    //     SL CW ImGui::InputFloat(UID(v.y), &v.y, minv, maxv, "%.2f");
    //     SL CW ImGui::InputFloat(UID(v.z), &v.z, minv, maxv, "%.2f");
    // }
    void valf(const char* desc, glm::vec3& v, float minv = -1.0f, float maxv = 1.0f){
        ImGui::Text(desc);
        SL ImGui::DragFloat3(UID(v),&v.x, 0.005f, minv, maxv);
    }
    
    void valf(const char* desc, glm::mat4& mat, float minv = -1.0f, float maxv = 1.0f){
        ImGui::Text(desc);
        for (int row = 0; row < 4; ++row) {
               CW ImGui::InputFloat(UID(mat[0][row]), &mat[0][row], minv, maxv,"%.2f");
            SL CW ImGui::InputFloat(UID(mat[1][row]), &mat[1][row], minv, maxv, "%.2f");
            SL CW ImGui::InputFloat(UID(mat[2][row]), &mat[2][row], minv, maxv, "%.2f");
            SL CW ImGui::InputFloat(UID(mat[3][row]), &mat[3][row], minv, maxv, "%.2f");
        }
    }

    void param_settings(){
        if(ImGui::TreeNode("random range")){
            HelpMarker("The lower / upper born at range random parameter will be generated");
            ImGui::DragFloat2("max scaling",prm::scale, 0.005f, 0.2f, 1.5f, "%.3f");
            ImGui::DragInt("max rotation", &prm::angle, 1, 0, 180, "%d");
            ImGui::DragFloat("max shearing", &prm::shearing, 0.005f, 0.0f, 2.0f, "%.2f");
            ImGui::DragFloat("max translate", &prm::translation, 0.005f, 0.0f, 2.0f, "%.2f");

            ImGui::TreePop();
        }
    }
} //end namespace UI

namespace mtl{   //namespace matrices utiles or whatever idk how to name it

    //fixedProcess is a legacy name corresponding to previous architecture. We used to have an abstract class MatricesGenerator and fixedProcess inherited from it alongsite rawMatrix, and we planned to implement other subclass such as Conpound which would've take an arbitrary number or scale, rotation, translation and shear in any given order.
    //However this just complexifies code for no reason, as a fixed process is enought for everything we could possibly want to do for now
    class FixedProcess{
    private:
        glm::mat4 matrix;
    public:
        glm::vec3 scale_factors;
        glm::vec3 rot_axis;
        float rot_angle;
        glm::vec3 Sh_xy_xz_yx;
        glm::vec3 Sh_yz_zx_zy;
        glm::vec3 translation_vector;

        

        void setRandom(){
            scale_factors = glm::vec3(
                RNDF*(prm::scale[1]-prm::scale[0])+prm::scale[0],
                RNDF*(prm::scale[1]-prm::scale[0])+prm::scale[0],
                RNDF*(prm::scale[1]-prm::scale[0])+prm::scale[0]
            );
            glm::vec3 randomVec = glm::ballRand(1.0f);
            rot_axis = glm::normalize(randomVec);
            rot_angle = glm::radians((RNDF*2-1) * (float) prm::angle);
            Sh_xy_xz_yx = glm::vec3(
                (RNDF*2-1)*prm::shearing,
                (RNDF*2-1)*prm::shearing,
                (RNDF*2-1)*prm::shearing
            );
            Sh_yz_zx_zy = glm::vec3(
                (RNDF*2-1)*prm::shearing,
                (RNDF*2-1)*prm::shearing,
                (RNDF*2-1)*prm::shearing
            );
            translation_vector = glm::vec3(
                (RNDF*2-1)*prm::translation,
                (RNDF*2-1)*prm::translation,
                (RNDF*2-1)*prm::translation
            );

        }

        const char* getName(){
            return "Fixed process matrix";
        }
        void ui(){
            if(ImGui::Button(UIDT("randomize func", *this))) setRandom();

            ui::valf("scale : ", scale_factors);
            ui::valf("rot axis", rot_axis);
            ui::valf("rota angle", rot_angle, 0.005f, -PI, PI);
            ui::valf("shear xy xz yx", Sh_xy_xz_yx);
            ui::valf("shear yz_zx_zy", Sh_yz_zx_zy);
            ui::valf("translation", translation_vector);

        }

        const glm::mat4& getMatrix(){
            matrix =
                glm::scale(glm::mat4(1.0f), scale_factors) *
                glm::rotate(glm::mat4(1.0f), rot_angle, rot_axis) *
                glm::transpose(glm::mat4(
                    1.0f, Sh_xy_xz_yx.x,  Sh_xy_xz_yx.y, 0.0f,  //     1.0f, Shxy,  Shxz, 0.0f,
                    Sh_xy_xz_yx.z, 1.0f,  Sh_yz_zx_zy.x, 0.0f,  //     Shyx, 1.0f,  Shyz, 0.0f,
                    Sh_yz_zx_zy.y, Sh_yz_zx_zy.z,  1.0f, 0.0f,  //     Shzx, Shzy,  1.0f, 0.0f,
                    0.0f, 0.0f,  0.0f, 1.0f)) *                 //     0.0f, 0.0f,  0.0f, 1.0f))
                glm::translate(glm::mat4(1.0f), translation_vector);
            return matrix;
        }

        FixedProcess(){
            setRandom();
        }
    };

    //class compound : MatrixGenerator ...


    class Attractor{
    public:
        FixedProcess* attr_funcs[10];

        Attractor() {}

        void setRandom(size_t nb_func){
             for (size_t i = 0; i < nb_func; i++) {
                attr_funcs[i]->setRandom();
             }
        }

        void ui(size_t nb_func) {
            if(ImGui::Button(UIDT("randomize all", *this))) setRandom(nb_func);
            ui::HelpMarker("Apllyable only if fixed process for now");
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

    //lerp - per matrix coefficient on 2 matrix genned by attractors A and B
    glm::mat4 matrixLerp(const Attractor& attractorA, const Attractor& attractorB, int index, float t) {
        glm::mat4 matA = attractorA.attr_funcs[index]->getMatrix();
        glm::mat4 matB = attractorB.attr_funcs[index]->getMatrix();
        return (1.0f - t) * matA + t * matB;
    }

    //fucked up if attractorfucntion aren't all fixed process. I plan on just deleting rawmatrix anyways
    glm::mat4 componentLerp(const Attractor& attractorA, const Attractor& attractorB, int index, float t) {
        FixedProcess* genA = attractorA.attr_funcs[index];
        FixedProcess* genB = attractorB.attr_funcs[index];

        
        glm::vec3 l_scale_factors = (1.0-t)*genA->scale_factors + t * genB->scale_factors;
        glm::vec3 l_rot_axis = glm::normalize((1.0-t)*genA->rot_axis + t *genB->rot_axis);
        float l_rot_angle = (1.0-t)*genA->rot_angle + t * genB->rot_angle;
        glm::vec3 l_Sh_xy_xz_yx = (1.0-t)*genA->Sh_xy_xz_yx + t * genB->Sh_xy_xz_yx;
        glm::vec3 l_Sh_yz_zx_zy = (1.0-t)*genA->Sh_yz_zx_zy + t * genB->Sh_yz_zx_zy;
        glm::vec3 l_translation_vector = (1.0-t)*genA->translation_vector + t * genB->translation_vector;

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
    //todo lerp by component for fixed process

} // namespace mtl



namespace uvl{ //utility values (also out of inspiration for naming namespace)
    int matrixPerAttractor = 0;
    int immobileCount = 1;
    float lerpFactor = 0.0f;
    mtl::Attractor A_tractor;
    mtl::Attractor B_tractor;
    std::vector<glm::mat4> ubo_matrices;

    void reset(){
        A_tractor.freeAll();
        B_tractor.freeAll();
        ubo_matrices.clear();
        ubo_matrices.reserve(10);
    }

    void update_ubo_matrices(LerpMode mode){
        switch (mode)
        {
        case lerp_Matrix:
            for(int i=0; i < matrixPerAttractor; i++){
                ubo_matrices[i] = (mtl::matrixLerp(A_tractor, B_tractor, i,  lerpFactor));
            }
            break;
        case lerp_PerComponent:    
            for(int i=0; i < matrixPerAttractor; i++){
                ubo_matrices[i] = (mtl::componentLerp(A_tractor, B_tractor, i,  lerpFactor));
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

    void set_sierpinski(){
        //todo!

        // reset();
        // mtl::MatricesGenerator* mga1, *mga2, *mga3;
        // mtl::MatricesGenerator* mgb1, *mgb2, *mgb3;
        // {//Sierpinski and reverse Sierpinski attractor
        //     mga1 = new mtl::RawMatrix(glm::transpose(glm::mat4(
        //             0.5f, 0.0f,  0.0f, 0.0f,
        //             0.0f, 0.5f,  0.0f, 0.36f,
        //             0.0f, 0.0f,  0.0f, 0.0f,
        //             0.0f, 0.0f,  0.0f, 1.0f)));
        //     mga2 = new mtl::RawMatrix(glm::transpose(glm::mat4(
        //             0.5f, 0.0f,  0.0f, -0.5f,
        //             0.0f, 0.5f,  0.0f, -0.5f,
        //             0.0f, 0.0f,  0.0f, 0.0f,
        //             0.0f, 0.0f,  0.0f, 1.0f)));
        //     mga3 = new mtl::RawMatrix(glm::transpose(glm::mat4(
        //             0.5f, 0.0f,  0.0f, 0.5f,
        //             0.0f, 0.5f,  0.0f, -0.5f,
        //             0.0f, 0.0f,  0.0f, 0.0f,
        //             0.0f, 0.0f,  0.0f, 1.0f)));
        //     mgb1 = new mtl::RawMatrix(glm::transpose(glm::mat4(
        //             0.5f, 0.0f,  0.0f, 0.0f,
        //             0.0f, 0.5f,  0.0f, -0.36f,
        //             0.0f, 0.0f,  0.0f, 0.0f,
        //             0.0f, 0.0f,  0.0f, 1.0f)));
        //     mgb2 = new mtl::RawMatrix(glm::transpose(glm::mat4(
        //             0.5f, 0.0f,  0.0f, 0.5f,
        //             0.0f, 0.5f,  0.0f, 0.5f,
        //             0.0f, 0.0f,  0.0f, 0.0f,
        //             0.0f, 0.0f,  0.0f, 1.0f)));
        //     mgb3 = new mtl::RawMatrix(glm::transpose(glm::mat4(
        //             0.5f, 0.0f,  0.0f, -0.5f,
        //             0.0f, 0.5f,  0.0f, 0.5f,
        //             0.0f, 0.0f,  0.0f, 0.0f,
        //             0.0f, 0.0f,  0.0f, 1.0f)));
        // }
        
        // A_tractor.addGenerator(mga1); A_tractor.addGenerator(mga2); A_tractor.addGenerator(mga3);
        // B_tractor.addGenerator(mgb1); B_tractor.addGenerator(mgb2); B_tractor.addGenerator(mgb3);

        // ubo_matrices.push_back(glm::mat4(1.0f));
        // ubo_matrices.push_back(glm::mat4(1.0f));
        // ubo_matrices.push_back(glm::mat4(1.0f));
        // matrixPerAttractor = 3;
    }
    void set_fixedProcess(){
        DEBUG("starting assign");
        reset();
        mtl::FixedProcess* mga[MAX_FUNC_PER_ATTRACTOR];
        mtl::FixedProcess* mgb[MAX_FUNC_PER_ATTRACTOR];

        for(int i=0; i<MAX_FUNC_PER_ATTRACTOR; i++){
            mga[i] = new mtl::FixedProcess();
            mgb[i] = new mtl::FixedProcess();
            A_tractor.attr_funcs[i] = mga[i];
            B_tractor.attr_funcs[i] = mgb[i];
            ubo_matrices.push_back(glm::mat4(1.0f));
        }
        
        matrixPerAttractor = 3; //can be any value between 3 or 10

        DEBUG("done");
    }

    

} //end namespace uvl


