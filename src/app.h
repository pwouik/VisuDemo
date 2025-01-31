#pragma once

#include "glad/gl.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glad/gl.h>
#include <cmath>
#include <cstdio>
#include <vector>
#include "raymarching_renderer.h"
#include "attractor_renderer.h"
#include <mutex>

#include "glm/ext/matrix_transform.hpp"
#include "glm/fwd.hpp"
#include "imgui_util.hpp" 
#include "leap_connection.h"
#define GLM_ENABLE_EXPERIMENTAL

#define FPS_UPDATE_DELAY 1

enum Mode{
    Raymarching,
    Attractor
};


#define DEBUG(x) std::cout << x << std::endl;
//#define DEBUG

class App
{
public:
    App(int w,int h);
	~App()
	{
        if (leap_connection) leap_connection.reset();
        shutdownIMGUI();
        delete raymarching_renderer;
        glfwTerminate();
	}
    void run();

    void setupLeapMotion();

    // virtual in case you want to override it in child class
    void onKey(int key, int scancode, int actions, int mods)
    {
        if(key == GLFW_KEY_M && actions == GLFW_PRESS)
            mouse_lock=!mouse_lock;
    }
    // virtual in case you want to override it in child class
    void onMouse(double xpos, double ypos)
    {
        if(!mouse_lock){
            pitch = -(ypos*180.0/height-90.0);
            yaw = -(xpos*2.0*360.0/width);
        }
    }
    void onScroll(double xoffset, double yoffset)
    {
        speed *= exp(yoffset/10.0);
    }

    void onResize(int w, int h){
        width=w;
        height=h;
        glViewport(0, 0, width, height);
        
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, 
                    GL_FLOAT, NULL);

        

        attractor_renderer->resize(w, h);

        //not requiered but maybe according to chat gpt yapping
        //glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
        //glBindImageTexture(1, depth_texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32I);
        //glBindImageTexture(4, jumpdist_texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32I);


        glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
        proj = glm::perspective(70.0f * PI / 180.0f,(float)width/(float)height,0.01f,1000.0f);
    }

    //getter. Maybe put everything public so we don't have those ?
    
    glm::vec3 getPos() const {
        return pos;
    }

    void draw_ui();
    void draw_ui_attractor();
    void updateFps();

public:
    GLFWwindow* window;

    static void onKey(GLFWwindow* window, int key, int scancode, int actions, int mods)
    {
        App* obj = (App*)glfwGetWindowUserPointer(window);
        obj->onKey(key, scancode, actions, mods);
    }
    static void onMouse(GLFWwindow* window, double xpos, double ypos)
    {
        App* obj = (App*)glfwGetWindowUserPointer(window);
        obj->onMouse(xpos,ypos);
    }
    static void onScroll(GLFWwindow* window, double xoffset, double yoffset)
    {
        App* obj = (App*)glfwGetWindowUserPointer(window);
        obj->onScroll(xoffset,yoffset);
    }
    static void onResize(GLFWwindow* window, int w, int h)
    {
        App* obj = (App*)glfwGetWindowUserPointer(window);
        obj->onResize(w,h);
    }

    void onFrame(const LEAP_TRACKING_EVENT& frame);
    
    const std::vector<const char*> recordings{"leap_recordings/leapRecording2.lmt", "leap_recordings/leapRecording3.lmt"};
    int width;
    int height;
    float pitch;
    float yaw;
    glm::vec3 pos;
    glm::vec3 light_pos;
    glm::vec3 fractal_position{0.0f, 0.0f, 0.0f};
    std::mutex leapmotion_mutex;
    glm::quat fractal_rotation = glm::identity<glm::quat>();
    float speed;
    double last_tick = 0;
    //pretty sure this could (and should) be stack allocated
    RaymarchingRenderer* raymarching_renderer;
    AttractorRenderer* attractor_renderer;

    GLuint blit_program;
    GLuint texture;
    GLuint dummy_vbo;
    GLuint dummy_vao;
    glm::mat4 proj;
    bool mouse_lock = false;
    glm::mat4 old_view;

    Mode curr_mode = Raymarching;

    int frameAcc = 0;
    float prevFpsUpdate = 0 ;
    float currentFPS;
    bool hasLeftHand = false;
    bool hasRightHand = false;
    std::unique_ptr<leap::leap_connection> leap_connection;
};