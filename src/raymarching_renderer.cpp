#include "raymarching_renderer.h"
#include "app.h"
#include "glm/fwd.hpp"
#include "imgui/imgui.h"
#include "opengl_util.h"
#include <LeapC.h>
#include <cassert>
#include <iostream>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include "glm/gtx/string_cast.hpp"
#include <GLFW/glfw3.h>


RaymarchingRenderer::RaymarchingRenderer(){
    compute_program = glCreateProgram();
    GLint comp = loadshader("shaders/raymarching.comp", GL_COMPUTE_SHADER);
    assert(comp>0);
    glAttachShader(compute_program, comp);
    assert(linkProgram(compute_program));
    //ugly fix bc my compiler ignore aserts :(
    #ifdef FORCE_ASSERT
    linkProgram(compute_program);
    #endif
    glUseProgram(compute_program);
    color_freq = 1.0;
    rotation = glm::vec3( -3.0f, 1.0f, 0.55f );
    offset = glm::vec3( 1.0f, 1.1f, 2.2f );
    occlusion = 4.5;
    specular = glm::vec3(1.0,1.0,1.0);
    min_light = 0.32f;
    k_d = 1.0;
    k_s = 1.0;
    alpha = 3.0;
    scale = 1.5;
}
void RaymarchingRenderer::leap_update(const LEAP_TRACKING_EVENT& frame){

    for (int i = 0; i < frame.nHands; i++)
    {
        if (frame.pHands[i].type == eLeapHandType_Right && frame.pHands[i].confidence > 0.1)
        {
            LEAP_HAND& hand = frame.pHands[i];

            if (hand.pinch_distance < 25){
                if (!right_was_pinched)
                {
                    right_was_pinched = true;
                    start_rotation = glm::make_quat(hand.palm.orientation.v);
                    start_offset = glm::make_vec3(hand.palm.position.v);
                }
                rotation += glm::eulerAngles(glm::make_quat(hand.palm.orientation.v) * glm::inverse(start_rotation));
                offset += (start_offset - glm::make_vec3(hand.palm.position.v)) / 1e2f;
                start_rotation = glm::make_quat(hand.palm.orientation.v);
                start_offset = glm::normalize(glm::make_vec3(hand.palm.position.v));
            }
            else
            {
                right_was_pinched = false;
            }
        }

        if (frame.pHands[i].type == eLeapHandType_Left && frame.pHands[i].confidence > 0.1)
        {
            if (frame.pHands[i].pinch_distance < 25)
            {
                const glm::quat palm_orientation = glm::make_quat(frame.pHands[i].palm.orientation.v);
                if (!left_was_pinched)
                {
                    left_was_pinched = true;
                    left_start_rotation = palm_orientation;
                }
                fractal_position += glm::make_vec3(frame.pHands[i].palm.velocity.v) * 5e-5f;
                fractal_rotation = glm::mix(glm::identity<glm::quat>(), palm_orientation * glm::inverse(left_start_rotation) , 0.75f) * fractal_rotation;
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
}
void RaymarchingRenderer::draw_ui(){
    ImGui::Begin("Raymarcher");
    ImGui::Text("fractal at %.2fx, %.2fy, %.2fz. Move with arrows", fractal_position.x, fractal_position.y, fractal_position.z);
    ImGui::Text("fractal rotation at pitch: %.2f, yaw: %.2f, roll: %.2f. Change with numpad 1, 2 or 3", fractal_rotation.x, fractal_rotation.y, fractal_rotation.z);
    ImGui::SeparatorText("params");
    ImGui::SliderFloat3("rotation", glm::value_ptr(rotation), -4.0, 4.0);
    ImGui::SliderFloat3("offset", glm::value_ptr(offset), -4.0, 4.0);
    ImGui::SliderFloat("scale", &scale, 1.0f, 2.0f);
    ImGui::SliderFloat("color freq", &color_freq, 0.01f, 100.0f,"%.3f",ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("minimum light", &min_light, 0.0f, 2.0f);
    ImGui::SliderFloat("k_d", &k_d, 0.0f, 2.0f);
    ImGui::SliderFloat("k_s", &k_s, 0.0f, 2.0f);
    ImGui::SliderFloat("alpha", &alpha, 1.0f, 5.0f);
    ImGui::ColorEdit3("specular", glm::value_ptr(specular));
    ImGui::SliderFloat("occlusion", &occlusion, -10.0f, 10.0f);
    if(ImGui::Button("Hot reload shader")){
        
        GLuint new_program = glCreateProgram();
        GLint comp = loadshader("shaders/raymarching.comp", GL_COMPUTE_SHADER);
        if(comp>0){
            glAttachShader(new_program, comp);
            if(linkProgram(new_program)){
                compute_program=new_program;
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

void RaymarchingRenderer::render(float width,float height,glm::vec3 pos,glm::mat4 inv_view, glm::mat4 inv_proj, glm::vec3 light_pos){

    glm::mat4 fractal_transform =  glm::translate(glm::identity<glm::mat4>(),fractal_position) * glm::toMat4(fractal_rotation);
    glm::mat4 total_transform = glm::inverse(fractal_transform)*inv_view;

    glUseProgram(compute_program);

    glUniformMatrix4fv(glGetUniformLocation(compute_program, "inv_view"),1, GL_FALSE, glm::value_ptr(total_transform));
    glUniformMatrix4fv(glGetUniformLocation(compute_program, "inv_proj"),1, GL_FALSE, glm::value_ptr(inv_proj));
    glUniform2ui(glGetUniformLocation(compute_program, "screen_size"), width,height);
    glUniform3fv(glGetUniformLocation(compute_program, "camera"), 1, glm::value_ptr(pos));
    glUniform3fv(glGetUniformLocation(compute_program, "light_pos"), 1, glm::value_ptr(glm::inverse(fractal_transform)*glm::vec4(light_pos,1.0)));
    glUniform3fv(glGetUniformLocation(compute_program, "offset"), 1, glm::value_ptr(offset));
    glUniform3fv(glGetUniformLocation(compute_program, "rotation"), 1, glm::value_ptr(rotation));
    glUniform1f(glGetUniformLocation(compute_program, "scale"), scale);
    glUniform1f(glGetUniformLocation(compute_program, "color_freq"), color_freq);
    glUniform1f(glGetUniformLocation(compute_program, "min_light"), min_light);
    glUniform1f(glGetUniformLocation(compute_program, "k_d"), k_d);
    glUniform1f(glGetUniformLocation(compute_program, "k_s"), k_s);
    glUniform1f(glGetUniformLocation(compute_program, "alpha"), alpha);
    glUniform3fv(glGetUniformLocation(compute_program, "specular"), 1, glm::value_ptr(specular));
    glUniform1f(glGetUniformLocation(compute_program, "occlusion"), occlusion);
    glUniform1f(glGetUniformLocation(compute_program, "time"), glfwGetTime());
    glDispatchCompute((width-1)/8+1, (height-1)/8+1, 1);

    // make sure writing to image has finished before read
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}