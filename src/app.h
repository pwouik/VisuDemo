#pragma once

#include "glad/gl.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glad/gl.h>
#include <cmath>
#include <cstdio>
#include <iostream>

#include "imgui_util.hpp" 
#include "leap_connection.h"
#define GLM_ENABLE_EXPERIMENTAL

#define PI 3.14159265358979323f
#define FPS_UPDATE_DELAY 1

enum Mode{
    Raymarching,
    Attractor
};

enum LerpMode{
    lerp_Matrix,
    lerp_PerComponent
};

#define DEBUG(x) std::cout << x << std::endl;
//#define DEBUG

#define DEBUG(x) std::cout << x << std::endl;
//#define DEBUG

class App
{
public:
    App(int w,int h);
	~App()
	{
        if (leap_connection) leap_connection.reset();
        utl::shutdownIMGUI();
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
        glViewport(0, 0, width, height);
        width=w;
        height=h;
        
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, 
                    GL_FLOAT, NULL);

        glBindTexture(GL_TEXTURE_2D, depth_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32I, w, h, 0, GL_RED_INTEGER, 
                    GL_INT, nullptr);

    glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
        proj = glm::perspective(70.0f * PI / 180.0f,(float)width/(float)height,0.1f,10000.0f);
        glUniformMatrix4fv(glGetUniformLocation(compute_program, "persp"),1, GL_FALSE, glm::value_ptr(proj));
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
    int width;
    int height;
    float pitch;
    float yaw;
    glm::vec3 pos;
    glm::vec3 light_pos;
    glm::vec3 param1;
    glm::vec3 param2;
    float k_a;
    float k_d;
    float k_s;
    float alpha;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    float occlusion;
    glm::vec3 fractal_position{0.0f, 0.0f, 0.0f};
    glm::vec3 fractal_rotation{0.0f, 0.0f, 0.0f};
    float speed;
    GLuint compute_program;
    GLuint compute_program_attractor;
    GLuint ssao_attractor;
    GLuint blit_program;
    GLuint texture;
    GLuint depth_texture;
    GLuint dummy_vbo;
    GLuint dummy_vao;
    glm::mat4 proj;
    bool mouse_lock = false;
    glm::mat4 old_view;

    Mode curr_mode = Raymarching;
    LerpMode lerpmode = lerp_Matrix;

    int frameAcc = 0;
    float prevFpsUpdate = 0 ;
    float currentFPS;

    std::unique_ptr<leap_connection_class> leap_connection;
};