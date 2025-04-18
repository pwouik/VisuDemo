#include "raymarching_renderer.h"
#include "app.h"
#include "glm/ext/quaternion_common.hpp"
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
    reset();    
    // Initialize time for frame-rate independent smoothing
    last_update_time = glfwGetTime();
}
void RaymarchingRenderer::reset(){
    rotation = glm::vec3( 0.603f, -0.275f, -0.314f );
    offset = glm::vec3( 0.822f, 1.261f, 0.109f );
    scale = 1.5;
    color_freq = 0.631;
    hue = 1.931;
    min_light = 1.0f;
    occlusion = 2.5;
    specular = glm::vec3(1.0,1.0,1.0);
    k_d = 1.0;
    k_s = 0.8;
    alpha = 4.0;
    aocolor = glm::vec3(0.1f,0.2f,0.0f);
    bloomcolor = glm::vec3(1.0f,0.7f,0.0f);
    bgcolor = glm::vec3(0.8f,0.0f,1.0f);
    
    // Initialize smoothing variables
    smoothing_factor = 0.1f;
    position_smoothing_factor = 0.08f;
    rotation_smoothing_factor = 0.15f;
    target_fractal_position = fractal_position;
    target_fractal_rotation = fractal_rotation;

}

void RaymarchingRenderer::leap_update(const LEAP_TRACKING_EVENT& frame){
    // Calculate delta time for frame-rate independent smoothing
    double current_time = glfwGetTime();
    float delta_time = static_cast<float>(current_time - last_update_time);
    last_update_time = current_time;
    
    // Adjust smoothing factors based on delta time to ensure consistent smoothing
    // regardless of frame rate (clamped to avoid issues with very low framerates)
    float time_adjusted_pos_smoothing = std::min(position_smoothing_factor * delta_time * 60.0f, 1.0f);
    float time_adjusted_rot_smoothing = std::min(rotation_smoothing_factor * delta_time * 60.0f, 1.0f);

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
                
                // Calculate rotation changes
                glm::quat current_rotation = glm::make_quat(hand.palm.orientation.v);
                glm::vec3 rotation_delta = glm::eulerAngles(current_rotation * glm::inverse(start_rotation));
                
                // Apply gradual rotation change
                rotation += rotation_delta * time_adjusted_rot_smoothing;
                
                // Calculate position changes with smoothing
                glm::vec3 current_offset = glm::make_vec3(hand.palm.position.v);
                glm::vec3 offset_delta = (start_offset - current_offset);
                
                // Apply gradual position change
                offset += offset_delta * 1e-2f;
                
                // Update starting points for next frame
                start_rotation = current_rotation;
                start_offset = current_offset;
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
                
                // Set target position with velocity
                glm::vec3 velocity = glm::make_vec3(frame.pHands[i].palm.velocity.v) * 2e-4f;
                target_fractal_position += velocity;
                
                // Set target rotation
                target_fractal_rotation = glm::mix(
                    glm::identity<glm::quat>(), 
                    palm_orientation * glm::inverse(left_start_rotation), 
                    0.75f
                ) * target_fractal_rotation;
                
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
    
    // Apply smoothing between current and target values
    fractal_position = glm::mix(fractal_position, target_fractal_position, time_adjusted_pos_smoothing);
    
    // Quaternion interpolation for smooth rotation
    fractal_rotation = glm::slerp(fractal_rotation, target_fractal_rotation, time_adjusted_rot_smoothing);
}

void RaymarchingRenderer::draw_ui(){
    ImGui::Begin("Raymarcher");
    ImGui::Text("fractal at %.2fx, %.2fy, %.2fz. Move with arrows", fractal_position.x, fractal_position.y, fractal_position.z);
    ImGui::Text("fractal rotation at pitch: %.2f, yaw: %.2f, roll: %.2f. Change with numpad 1, 2 or 3", fractal_rotation.x, fractal_rotation.y, fractal_rotation.z);
    
    ImGui::SeparatorText("Movement Settings");
    ImGui::SliderFloat("Position Smoothing", &position_smoothing_factor, 0.01f, 1.0f);
    ImGui::SliderFloat("Rotation Smoothing", &rotation_smoothing_factor, 0.01f, 1.0f);
    
    ImGui::SeparatorText("params");
    ImGui::SliderFloat3("rotation", glm::value_ptr(rotation), -4.0, 4.0);
    ImGui::SliderFloat3("offset", glm::value_ptr(offset), -4.0, 4.0);
    ImGui::SliderFloat("scale", &scale, 1.0f, 2.0f);
    ImGui::SliderFloat("color freq", &color_freq, 0.01f, 100.0f,"%.3f",ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("hue", &hue, 0.0f, 2.0f);
    ImGui::SliderFloat("minimum light", &min_light, 0.0f, 2.0f);
    ImGui::SliderFloat("k_d", &k_d, 0.0f, 2.0f);
    ImGui::SliderFloat("k_s", &k_s, 0.0f, 2.0f);
    ImGui::SliderFloat("alpha", &alpha, 1.0f, 5.0f);
    ImGui::ColorEdit3("specular", glm::value_ptr(specular));
    ImGui::SliderFloat("occlusion", &occlusion, -10.0f, 10.0f);
    ImGui::ColorEdit3("occlusion color", glm::value_ptr(aocolor));
    ImGui::ColorEdit3("bloom color", glm::value_ptr(bloomcolor));
    ImGui::ColorEdit3("background color", glm::value_ptr(bgcolor));
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

    glm::mat4 fractal_transform =  glm::translate(glm::identity<glm::mat4>(), fractal_position) * glm::toMat4(fractal_rotation);
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
    glUniform1f(glGetUniformLocation(compute_program, "hue"), hue);
    glUniform3fv(glGetUniformLocation(compute_program, "aocolor"), 1, glm::value_ptr(aocolor));
    glUniform3fv(glGetUniformLocation(compute_program, "bloomcolor"), 1, glm::value_ptr(bloomcolor));
    glUniform3fv(glGetUniformLocation(compute_program, "bgcolor"), 1, glm::value_ptr(bgcolor));
    glDispatchCompute((width-1)/8+1, (height-1)/8+1, 1);

    // make sure writing to image has finished before read
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}