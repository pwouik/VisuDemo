#include "app.h"
#include <GLFW/glfw3.h>
#include <LeapC.h>
#include <climits>
#include <cstdlib>
#include <glm/gtx/rotate_vector.hpp>
#include "attractor.h"
#include "opengl_util.h"


#include <iostream>
#include <memory>
#include <optional>
#include <vector>

#include "leap_connection.h"
#include "glm/gtc/type_ptr.hpp"
#include <mutex>

// #include <thread> //this therad
// #include <chrono> //sleep



#define BUFFER_OFFSET(offset) ((GLvoid*)(offset))

using namespace std;


App::App(int w,int h)
{
    pos = glm::vec3(0.0,0.0,0.0);
    light_pos = glm::vec3(0.0,0.0,0.0);
    speed = 1.0f;
    width = w;
    height = h;

    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(width, height, "[glad] GL with GLFW", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, onKey);
    glfwSetCursorPosCallback (window, onMouse);
    glfwSetScrollCallback(window, onScroll);
    glfwSetFramebufferSizeCallback(window, onResize);

    initIMGUI(window);
    DEBUG("ImGui initialized");

    int version = gladLoadGL(glfwGetProcAddress);
    printf("GL %d.%d\n", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
    
    //compute_program for ray marching

    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, 
                GL_FLOAT, nullptr);

    glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

    blit_program = glCreateProgram();
    GLint vert = loadshader("shaders/blit.vert", GL_VERTEX_SHADER);
    GLint frag = loadshader("shaders/blit.frag", GL_FRAGMENT_SHADER);
    glAttachShader(blit_program, vert);
    glAttachShader(blit_program, frag);
    glLinkProgram(blit_program);
    linkProgram(blit_program);

    glGenVertexArrays(1, &dummy_vao);
    glBindVertexArray(dummy_vao);
    glGenBuffers(1, &dummy_vbo);

    proj = glm::perspective(glm::radians(70.0f),(float)width/(float)height,0.01f,1000.0f);

    raymarching_renderer = new RaymarchingRenderer();
    attractor_renderer = new AttractorRenderer(w,h);
}

void App::updateFps(){
    frameAcc++;
    double timeCurr  = glfwGetTime();
    float elapsedTime = timeCurr-prevFpsUpdate;
    if(elapsedTime>FPS_UPDATE_DELAY){
        currentFPS = (float)frameAcc / elapsedTime ;
        frameAcc = 0;
        prevFpsUpdate = timeCurr;
    }
}

void App::draw_ui(){
    ImGui::Begin("Base info");
        ImGui::SeparatorText("current rendering");
            ImGui::Text("FPS : %.2f", currentFPS);
            const char* items_cb1[] = { "Raymarching", "Attractor"}; //MUST MATCH ENUM IN app.h !
            if(ImGui::Combo("Display type", (int*)&curr_mode, items_cb1, IM_ARRAYSIZE(items_cb1))){
            }
            
        ImGui::SeparatorText("generic debug info");
        ImGui::Text("camera in %.2fx, %.2fy, %.2fz", pos.x, pos.y, pos.z);
        ImGui::Text("light in %.2fx, %.2fy, %.2fz . Set with L", light_pos.x, light_pos.y, light_pos.z);
        ImGui::Text("fractal at %.2fx, %.2fy, %.2fz. Move with arrows", fractal_position.x, fractal_position.y, fractal_position.z);
        ImGui::Text("fractal rotation at pitch: %.2f, yaw: %.2f, roll: %.2f. Change with numpad 1, 2 or 3", fractal_rotation.x, fractal_rotation.y, fractal_rotation.z);
        ImGui::Text("speed %.2e. Scroll to change", speed);
        ImGui::SeparatorText("Leap motion");
        if (!leap_connection->is_service_running())
        {
            static int current = 0;
            ImGui::Combo("Select recording", &current, recordings.data(), recordings.size());
            if(ImGui::Button("Start record playback") && current < recordings.size() && current >= 0)
            {
                leap_connection->start_playback(recordings[current]);
            }
            else if(ImGui::Button("Start leap capture"))
            {
                leap_connection->start_service();
            }
        }
        else
        {
            ImGui::TextColored(hasLeftHand ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Left hand");
            ImGui::SameLine();
            ImGui::Text(" | ");
            ImGui::SameLine();
            ImGui::TextColored(hasRightHand ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Right hand");
            if (ImGui::Button("Stop leap service"))
            {
                leap_connection->terminate_service();
            }
        }
    ImGui::End();
}

void App::run(){
    while (!glfwWindowShouldClose(window)) {
        double current_tick = glfwGetTime();
        float motion = speed*(current_tick-last_tick);
        last_tick=current_tick;
        glfwPollEvents();
        updateFps();

        
        if(glfwGetKey(window, GLFW_KEY_W)==GLFW_PRESS){
            pos+= glm::rotateY(glm::vec3(0.0,0.0,-1.0),yaw * PI / 180.0f) * motion;
        }
        if(glfwGetKey(window, GLFW_KEY_S)==GLFW_PRESS){
            pos+= glm::rotateY(glm::vec3(0.0,0.0,1.0),yaw * PI / 180.0f) * motion;
        }
        if(glfwGetKey(window, GLFW_KEY_A)==GLFW_PRESS){
            pos+= glm::rotateY(glm::vec3(-1.0,0.0,0.0),yaw * PI / 180.0f) * motion;
        }
        if(glfwGetKey(window, GLFW_KEY_D)==GLFW_PRESS){
            pos+= glm::rotateY(glm::vec3(1.0,0.0,0.0),yaw * PI / 180.0f) * motion;
        }
        if(glfwGetKey(window, GLFW_KEY_SPACE)==GLFW_PRESS){
            pos.y += motion;
        }
        if(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS){
            pos.y -= motion;
        }
        if(glfwGetKey(window, GLFW_KEY_L)==GLFW_PRESS){
            light_pos = pos;
        }
        if(glfwGetKey(window, GLFW_KEY_UP)==GLFW_PRESS){
            fractal_position += glm::vec3(-1.0f ,0.0f, 0.0f) * motion;
        }
        if(glfwGetKey(window, GLFW_KEY_DOWN)==GLFW_PRESS){
            fractal_position += glm::vec3(1.0f ,0.0f, 0.0f) * motion;
        }
        if(glfwGetKey(window, GLFW_KEY_LEFT)==GLFW_PRESS){
            fractal_position += glm::vec3(0.0f ,0.0f, 1.0f) * motion;
        }
        if(glfwGetKey(window, GLFW_KEY_RIGHT)==GLFW_PRESS){
            fractal_position += glm::vec3(0.0f ,0.0f, -1.0f) * motion;
        }
        if(glfwGetKey(window, GLFW_KEY_PAGE_UP)==GLFW_PRESS){
            fractal_position += glm::vec3(0.0f ,1.0f, 0.0f) * motion;
        }
        if(glfwGetKey(window, GLFW_KEY_PAGE_DOWN)==GLFW_PRESS){
            fractal_position += glm::vec3(0.0f ,-1.0f, 0.0f) * motion;
        }

        glm::mat4 inv_camera_view = glm::inverse(glm::translate(glm::rotate(
                glm::rotate(glm::identity<glm::mat4>(),
                    -glm::radians(pitch),
                    glm::vec3(1.0, 0.0, 0.0)),
                -glm::radians(yaw),
                glm::vec3(0.0, 1.0, 0.0)),
            glm::vec3(-pos.x, -pos.y, -pos.z)));

        newframeIMGUI();
        draw_ui();
        switch (curr_mode) {
        case Raymarching:{
            std::lock_guard<std::mutex> lock(leapmotion_mutex);
            raymarching_renderer->draw_ui();
            raymarching_renderer->render(width, height, pos, inv_camera_view, glm::inverse(proj), light_pos, fractal_position, fractal_rotation);
            break;
        }
        case Attractor:{
            attractor_renderer->draw_ui(speed, pos);
            attractor_renderer->render(width, height, pos, inv_camera_view, proj, light_pos);
            break;
        }
        }

        glUseProgram(blit_program);
        glBindVertexArray(dummy_vao);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glDrawArrays(GL_TRIANGLES,0,3);

        enframeIMGUI();
        multiViewportIMGUI(window);
        glfwSwapBuffers(window);
    }
}

void App::setupLeapMotion()
{
    constexpr LEAP_ALLOCATOR allocator
    {
        [](const uint32_t size, eLeapAllocatorType, void*){void* ptr = malloc(size); return ptr;},
        [](void* ptr, void*){if(!ptr) return; free(ptr);},
        nullptr
    };
    leap_connection = std::make_unique<leap::leap_connection>(allocator, eLeapPolicyFlag_Images | eLeapPolicyFlag_MapPoints, 0);
    leap_connection->on_connection = []{std::cout << "Leap device connected.\n";};
    leap_connection->on_device_found = [](const LEAP_DEVICE_INFO& device_info){std::cout << "Found device " << device_info.serial << ".\n";};

    leap_connection->on_frame = [this](const LEAP_TRACKING_EVENT& frame){onFrame(frame);};
}

void App::onFrame(const LEAP_TRACKING_EVENT& frame)
    {
        raymarching_renderer->leap_update(frame);
        std::cout << "Frame " << frame.info.frame_id << " with " << frame.nHands << " hands. ";
        std::optional<LEAP_HAND> left = std::nullopt;
        for (int i = 0; i < frame.nHands; i++)
        {
            if (frame.pHands[i].type == eLeapHandType_Left && frame.pHands[i].confidence > 0.1)
            {
                hasRightHand = true;
            }
            if (frame.pHands[i].type == eLeapHandType_Right && frame.pHands[i].confidence > 0.1)
            {
                hasRightHand = true;
            }
        }
        static bool left_was_pinched = false;
        if (left.has_value())
        {
            hasLeftHand = true;
            std::cout << "Left hand velocity: " << left.value().palm.velocity.x << ", " << left.value().palm.velocity.y << ", " << left.value().palm.velocity.z << " with " << left.value().confidence << " confidence. Pinch distance: " << std::floor(left.value().pinch_distance);
            static glm::quat left_start_rotation;
            if (left.value().pinch_distance < 25)
            {
                const glm::quat palm_orientation = glm::make_quat(left.value().palm.orientation.v);
                if (!left_was_pinched)
                {
                    left_was_pinched = true;
                    left_start_rotation = palm_orientation;
                }
                fractal_position += glm::make_vec3(left.value().palm.velocity.v) * 5e-5f;
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
        hasLeftHand = false;
        hasRightHand = false;
        std::cout << "\n";
    }