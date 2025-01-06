#include "app.h"
#include <GLFW/glfw3.h>
#include <cstdlib>
#include <glm/ext/matrix_transform.hpp>
#include <glm/fwd.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <iostream>
#include "glm/gtc/type_ptr.hpp"
#include "glm/matrix.hpp"
#include "glm/ext.hpp"
#define BUFFER_OFFSET(offset) ((GLvoid*)(offset))

using namespace std;


namespace ui{
    bool displayUI = true;
    void baseInfo(App& app){
        if(!displayUI) return;
        ImGui::Begin("Base info");
            ImGui::SeparatorText("generic debug info");
            glm::vec3 pos = app.getPos();
            ImGui::Text("camera in %.2fx, %.2fy, %.2fz", pos.x, pos.y, pos.z);

        ImGui::End();
    }


}//end namespace ui

GLchar* readShaderSource(const char * shaderFile)
{
    FILE* fp = fopen(shaderFile, "r");
    GLchar* buf;
    long size;
    if (fp == NULL) return NULL;
    fseek(fp, 0L, SEEK_END);//go to end
    size = ftell(fp); //get size
    fseek(fp, 0L, SEEK_SET);//go to beginning
    buf = (GLchar*)malloc((size+1)*sizeof(GLchar));
    fread(buf, 1, size, fp);
    buf[size] = 0;
    fclose(fp);
    return buf;
}

GLuint loadshader(const char* file,GLuint type)
{
    GLchar* source = readShaderSource(file);
    if (source == NULL) {
        printf("Failed to read %s\n", file);
        exit(EXIT_FAILURE);
    }
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, (const GLchar**)&source, NULL);
    glCompileShader(shader);
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        printf("%s failed to compile:\n", file);
        GLint logSize;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logSize);
        char* logMsg = new char[logSize];
        glGetShaderInfoLog(shader, logSize, NULL, logMsg);
        printf("%s\n", logMsg);
        delete[] logMsg;
        exit(EXIT_FAILURE);
    }
    delete[] source;
    return shader;
}
App::App(int w,int h)
{
    pos = glm::vec3(0.0,0.0,0.0);
    light_pos = glm::vec3(0.0,0.0,0.0);
    width = w;
    height = h;
    speed = 1.0;

    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(width, height, "[glad] GL with GLFW", NULL, NULL);
    glfwMakeContextCurrent(window);

    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, onKey);
    glfwSetCursorPosCallback (window, onMouse);
    glfwSetScrollCallback(window, onScroll);
    glfwSetFramebufferSizeCallback(window, onResize);

    utl::initIMGUI(window);

    int version = gladLoadGL(glfwGetProcAddress);
    printf("GL %d.%d\n", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
    
    compute_program = glCreateProgram();
    GLint comp = loadshader("shaders/render.comp", GL_COMPUTE_SHADER);
    glAttachShader(compute_program, comp);
    glLinkProgram(compute_program);
    GLint linked;
    glGetProgramiv(compute_program, GL_LINK_STATUS, &linked);
    if (!linked) {
        printf("Shader program failed to link:\n");
        GLint logSize;
        glGetProgramiv(compute_program, GL_INFO_LOG_LENGTH, &logSize);
        char* logMsg = new char[logSize];
        glGetProgramInfoLog(compute_program, logSize, NULL, logMsg);
        printf("%s\n", logMsg);
        delete[] logMsg;
        exit(EXIT_FAILURE);
    }
    glUseProgram(compute_program);

    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, 
                GL_FLOAT, NULL);

    glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);



    blit_program = glCreateProgram();
    GLint vert = loadshader("shaders/blit.vert", GL_VERTEX_SHADER);
    GLint frag = loadshader("shaders/blit.frag", GL_FRAGMENT_SHADER);
    glAttachShader(blit_program, vert);
    glAttachShader(blit_program, frag);
    glLinkProgram(blit_program);

    glGetProgramiv(blit_program, GL_LINK_STATUS, &linked);
    if (!linked) {
        printf("Shader program failed to link:\n");
        GLint logSize;
        glGetProgramiv(blit_program, GL_INFO_LOG_LENGTH, &logSize);
        char* logMsg = new char[logSize];
        glGetProgramInfoLog(blit_program, logSize, NULL, logMsg);
        printf("%s\n", logMsg);
        delete[] logMsg;
        exit(EXIT_FAILURE);
    }

    glGenVertexArrays(1, &dummy_vao);
    glBindVertexArray(dummy_vao);
    glGenBuffers(1, &dummy_vbo);

    proj = glm::perspective(glm::radians(70.0f),(float)width/(float)height,0.1f,10000.0f);
    glUniformMatrix4fv(glGetUniformLocation(compute_program, "persp"),1, GL_FALSE, glm::value_ptr(proj));
}

void App::run(){
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        

        if(glfwGetKey(window, GLFW_KEY_W)==GLFW_PRESS){
            pos+= glm::rotateY(glm::vec3(0.0,0.0,-1.0),yaw * PI / 180.0f) * speed;
        }
        if(glfwGetKey(window, GLFW_KEY_S)==GLFW_PRESS){
            pos+= glm::rotateY(glm::vec3(0.0,0.0,1.0),yaw * PI / 180.0f) * speed;
        }

        if(glfwGetKey(window, GLFW_KEY_A)==GLFW_PRESS){
            pos+= glm::rotateY(glm::vec3(-1.0,0.0,0.0),yaw * PI / 180.0f) * speed;
        }

        if(glfwGetKey(window, GLFW_KEY_D)==GLFW_PRESS){
            pos+= glm::rotateY(glm::vec3(1.0,0.0,0.0),yaw * PI / 180.0f) * speed;
        }
        if(glfwGetKey(window, GLFW_KEY_L)==GLFW_PRESS){
            light_pos = pos;
        }

        if(glfwGetKey(window, GLFW_KEY_SPACE)==GLFW_PRESS){
            pos.y += speed;
        }

        if(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS){
            pos.y -= speed;
        }

        view = glm::translate(glm::rotate(
                glm::rotate(glm::identity<glm::mat4>(),
                    -glm::radians(pitch),
                    glm::vec3(1.0, 0.0, 0.0)),
                -glm::radians(yaw),
                glm::vec3(0.0, 1.0, 0.0)),
            glm::vec3(-pos.x, -pos.y, -pos.z));
        
        utl::newframeIMGUI();
        ui::baseInfo(*this);

        glClearColor(0.0f, 0.0f, 0.1f, 1.0f);

        glUseProgram(compute_program);
        glActiveTexture(GL_TEXTURE0);

        glUniformMatrix4fv(glGetUniformLocation(compute_program, "inv_view"),1, GL_FALSE, glm::value_ptr(glm::inverse(view)));
        glUniformMatrix4fv(glGetUniformLocation(compute_program, "inv_proj"),1, GL_FALSE, glm::value_ptr(glm::inverse(proj)));
        glUniform2ui(glGetUniformLocation(compute_program, "screen_size"), width,height);
        glUniform3fv(glGetUniformLocation(compute_program, "camera"), 1, glm::value_ptr(pos));
        glUniform3fv(glGetUniformLocation(compute_program, "light_pos"), 1, glm::value_ptr(light_pos));
        glUniform1f(glGetUniformLocation(compute_program, "time"), glfwGetTime());
        glDispatchCompute((width-1)/32+1, (height-1)/32+1, 1);

        // make sure writing to image has finished before read
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(blit_program);
        glBindVertexArray(dummy_vao);
        glBindTexture(GL_TEXTURE_2D, texture);
        glDrawArrays(GL_TRIANGLES,0,3);


        utl::enframeIMGUI();
        utl::multiViewportIMGUI(window);
        glfwSwapBuffers(window);
    }
}