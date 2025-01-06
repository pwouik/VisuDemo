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
#include <vector>
#include <sstream>
#include <string>
#include <fstream>
#define GLM_ENABLE_EXPERIMENTAL

#define PI 3.14159265358979323f


class App
{
public:
    App(int w,int h);
	~App()
	{
        glfwTerminate();
	}
    void run();

    // virtual in case you want to override it in child class
    void onKey(int key, int scancode, int actions, int mods)
    {
    }
    // virtual in case you want to override it in child class
    void onMouse(double xpos, double ypos)
    {
        pitch = -(ypos*180.0/height-90.0);
        yaw = -(xpos*2.0*360.0/width);
    }
    void onScroll(double xoffset, double yoffset)
    {
        speed *= exp(yoffset/10.0);
    }

    void onResize(int w, int h){
        glViewport(0, 0, width, height);
        width=w;
        height=h;
        proj = glm::perspective(70.0f * PI / 180.0f,(float)width/(float)height,0.1f,10000.0f);
        glUniformMatrix4fv(glGetUniformLocation(compute_program, "persp"),1, GL_FALSE, glm::value_ptr(proj));
    }

private:
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
    float speed;
    GLuint compute_program;
    GLuint blit_program;
    GLuint texture;
    glm::mat4 proj;
    glm::mat4 view;

};