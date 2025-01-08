/**
 * Utility file containting function for representing attractor and lerping between them
 * 
 */
#pragma once

#include <imgui/imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector> //It may be possible to use array and template<int T>
#include <string>

#include <iostream> //TODO REMOVE
// 

//UI util
#define SL ImGui::SameLine();
#define CW ImGui::SetNextItemWidth(inputWidth);
#define UID(val) (std::string("##Z") + std::to_string(reinterpret_cast<uintptr_t>(&val))).c_str()
//#define UID(val) "##tantpis" //TODO USE UID GENERATION ABOVE

namespace ui{
    float inputWidth = 45.0f; //width for most input float

    //editable field for float of float vector
    void valf(const char* desc, float& v, float minv = 0.0f, float maxv = 1.0f){
        ImGui::Text(desc);
        SL CW ImGui::InputFloat(UID(v), &v, minv, maxv, "%.2f");
    }
    void valf(const char* desc, glm::vec3& v, float minv = 0.0f, float maxv = 1.0f){
        ImGui::Text(desc);
        SL CW ImGui::InputFloat(UID(v.x), &v.x, minv, maxv, "%.2f");
        SL CW ImGui::InputFloat(UID(v.y), &v.y, minv, maxv, "%.2f");
        SL CW ImGui::InputFloat(UID(v.z), &v.z, minv, maxv, "%.2f");
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
} //end namespace UI

namespace mtl{   //namespace matrices utiles or whatever idk how to name it
    class MatricesGenerator {
    protected:
        glm::mat4 matrix;

    public:
        virtual const glm::mat4& getMatrix() const = 0;
        virtual void ui() = 0;
        virtual ~MatricesGenerator() = default;
    };

    class RawMatrix : public MatricesGenerator {
    public:
        RawMatrix(const glm::mat4& mat) {
            matrix = mat;
        }
        void ui() override {
            ui::valf("raw matrix : ", matrix);
        }
        const glm::mat4& getMatrix() const override {
            return matrix; 
        }

    };

    class FixedProcess : public MatricesGenerator{
        void ui(){
            ui::valf("raw matrix : ", matrix);
        }
        const glm::mat4& getMatrix() const override {
            return matrix;
        }
    };

    //class compound : MatrixGenerator ...


    class Attractor{
    public:
        MatricesGenerator* generators[8];
        size_t count;

        Attractor() : count(0) {}

        void ui() {
            for (size_t i = 0; i < count; i++) {
                ImGui::Text("Generator %d",i);
                generators[i]->ui();
            }
        }

        void addGenerator(MatricesGenerator* generator) {
            if (count >= 8) {
                throw std::out_of_range("Cannot add more than 6 MatricesGenerator.");
            }
            generators[count++] = generator;
        }

        const glm::mat4& getMatrix(size_t index) const {
            if (index >= count) {
                throw std::out_of_range("Index out of range.");
            }
            return generators[index]->getMatrix();
        }

        void freeAll(){
            for(size_t i=0; i < count; i++){
                delete generators[i];
            }
        }
        
    };

    //lerp - per matrix coefficient on 2 matrix genned by attractors A and B
    glm::mat4 matrixLerp(const Attractor& attractorA, const Attractor& attractorB, int index, float t) {
        glm::mat4 matA = attractorA.generators[index]->getMatrix();
        glm::mat4 matB = attractorB.generators[index]->getMatrix();
        return (1.0f - t) * matA + t * matB;
    }
    //todo lerp by component for fixed process

} // namespace mtl



namespace uvl{ //utility values (also out of inspiration for naming namespace)
    int matrixPerAttractor = 0; //must match the size of Attractor
    float lerpFactor = 0.0f;
    mtl::Attractor A_tractor;
    mtl::Attractor B_tractor;
    std::vector<glm::mat4> ubo_matrices;

    void update_ubo_matrices(){
        for(int i=0; i < matrixPerAttractor; i++){
            ubo_matrices[i] = (mtl::matrixLerp(A_tractor, B_tractor, i,  lerpFactor));
        }
        
        //debug todo remove
        // if(matrixPerAttractor == 0) return;
        // ubo_matrices[0] = A_tractor.generators[0]->getMatrix();
        // ubo_matrices[1] = A_tractor.generators[1]->getMatrix();
        // ubo_matrices[2] = A_tractor.generators[2]->getMatrix();
        
        //debug todo remove
        // glm::mat4 mat = A_tractor.generators[0]->getMatrix();
        // std::cout << "\n\nfoo\n\n";
        // for (int row = 0; row < 4; ++row) {
        //     std::cout << "\t" << mat[row][0]<< "\t" << mat[row][1]<< "\t" << mat[row][2]<< "\t" << mat[row][3] << "\n";
        // }
    
    }

    void set_sierpinski(){
        mtl::MatricesGenerator* mga1, *mga2, *mga3;
        mtl::MatricesGenerator* mgb1, *mgb2, *mgb3;
        {//Sierpinski and reverse Sierpinski attractor
            mga1 = new mtl::RawMatrix(glm::transpose(glm::mat4(
                    0.5f, 0.0f,  0.0f, 0.0f,
                    0.0f, 0.5f,  0.0f, 0.36f,
                    0.0f, 0.0f,  0.0f, 0.0f,
                    0.0f, 0.0f,  0.0f, 1.0f)));
            mga2 = new mtl::RawMatrix(glm::transpose(glm::mat4(
                    0.5f, 0.0f,  0.0f, -0.5f,
                    0.0f, 0.5f,  0.0f, -0.5f,
                    0.0f, 0.0f,  0.0f, 0.0f,
                    0.0f, 0.0f,  0.0f, 1.0f)));
            mga3 = new mtl::RawMatrix(glm::transpose(glm::mat4(
                    0.5f, 0.0f,  0.0f, 0.5f,
                    0.0f, 0.5f,  0.0f, -0.5f,
                    0.0f, 0.0f,  0.0f, 0.0f,
                    0.0f, 0.0f,  0.0f, 1.0f)));
            mgb1 = new mtl::RawMatrix(glm::transpose(glm::mat4(
                    0.5f, 0.0f,  0.0f, 0.0f,
                    0.0f, 0.5f,  0.0f, -0.36f,
                    0.0f, 0.0f,  0.0f, 0.0f,
                    0.0f, 0.0f,  0.0f, 1.0f)));
            mgb2 = new mtl::RawMatrix(glm::transpose(glm::mat4(
                    0.5f, 0.0f,  0.0f, 0.5f,
                    0.0f, 0.5f,  0.0f, 0.5f,
                    0.0f, 0.0f,  0.0f, 0.0f,
                    0.0f, 0.0f,  0.0f, 1.0f)));
            mgb3 = new mtl::RawMatrix(glm::transpose(glm::mat4(
                    0.5f, 0.0f,  0.0f, -0.5f,
                    0.0f, 0.5f,  0.0f, 0.5f,
                    0.0f, 0.0f,  0.0f, 0.0f,
                    0.0f, 0.0f,  0.0f, 1.0f)));
        }
        
        A_tractor.addGenerator(mga1); A_tractor.addGenerator(mga2); A_tractor.addGenerator(mga3);
        B_tractor.addGenerator(mgb1); B_tractor.addGenerator(mgb2); B_tractor.addGenerator(mgb3);

        ubo_matrices.push_back(glm::mat4(1.0f));
        ubo_matrices.push_back(glm::mat4(1.0f));
        ubo_matrices.push_back(glm::mat4(1.0f));
        matrixPerAttractor = 3;
    }

    

} //end namespace uvl


