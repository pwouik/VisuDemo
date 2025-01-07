#include "app.h"
#include <GLFW/glfw3.h>
#include <cstdlib>
#include <fstream>
#include <glm/ext/matrix_transform.hpp>
#include <glm/fwd.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <iostream>
#include "glm/gtc/type_ptr.hpp"
#include "glm/matrix.hpp"
#include "glm/ext.hpp"
#include "imgui/imgui.h"
#define BUFFER_OFFSET(offset) ((GLvoid*)(offset))

using namespace std;



std::string readShaderSource(const std::string& shaderFile)
{
    std::ifstream file(shaderFile);
    if (!file.is_open())
    {
        std::cerr << "Error could not open file " << shaderFile << std::endl;
        return "";
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    return content;
}

GLuint loadshader(const char* file,GLuint type)
{
    const std::string shaderSource = readShaderSource(file);
    const GLchar* source = shaderSource.c_str();
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        printf("%s failed to compile:\n", file);
        GLint logSize;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logSize);
        char* logMsg = new char[logSize];
        glGetShaderInfoLog(shader, logSize, nullptr, logMsg);
        printf("%s\n", logMsg);
        delete[] logMsg;
        exit(EXIT_FAILURE);
    }
    return shader;
}
App::App(int w,int h)
{
    pos = glm::vec3(0.0,0.0,0.0);
    light_pos = glm::vec3(0.0,0.0,0.0);
    param1 = glm::vec3( -5.0f, -5.0f, 0.55f );
    occlusion = 3.0;
    ambient = glm::vec3(1.0,0.6,0.9);
    diffuse = glm::vec3(1.0,0.6,0.0);
    specular = glm::vec3(1.0,1.0,1.0);
    k_a = 0.32;
    k_d = 1.0;
    k_s = 1.0;
    alpha = 3.0;
    speed = 1.0;
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

    utl::initIMGUI(window);

    int version = gladLoadGL(glfwGetProcAddress);
    printf("GL %d.%d\n", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
    
    //compute_program for ray marching
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
        glGetProgramInfoLog(compute_program, logSize, nullptr, logMsg);
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
                GL_FLOAT, nullptr);

    glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);


    //compute_program for attractor
    compute_program_attractor = glCreateProgram();
    GLint comp_attractor = loadshader("shaders/render_attractor.comp", GL_COMPUTE_SHADER);
    glAttachShader(compute_program_attractor, comp_attractor);
    glLinkProgram(compute_program_attractor);
    GLint linked_attractor; //probably useless to redeclare
    glGetProgramiv(compute_program_attractor, GL_LINK_STATUS, &linked_attractor);
    if (!linked_attractor) {
        printf("Shader program failed to link:\n");
        GLint logSize;
        glGetProgramiv(compute_program_attractor, GL_INFO_LOG_LENGTH, &logSize);
        char* logMsg = new char[logSize];
        glGetProgramInfoLog(compute_program_attractor, logSize, nullptr, logMsg);
        printf("%s\n", logMsg);
        delete[] logMsg;
        exit(EXIT_FAILURE);
    }


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
        glGetProgramInfoLog(blit_program, logSize, nullptr, logMsg);
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

void App::draw_ui(){
    ImGui::Begin("Base info");
        ImGui::SeparatorText("current rendering");
            const char* items_cb1[] = { "Raymarching", "Attractor"}; //MUST MATCH DEFINE IN app.h !
            ImGui::Combo("combo", &curr_mode, items_cb1, IM_ARRAYSIZE(items_cb1));
        ImGui::SeparatorText("generic debug info");
        ImGui::Text("camera in %.2fx, %.2fy, %.2fz", pos.x, pos.y, pos.z);
        ImGui::Text("light in %.2fx, %.2fy, %.2fz . Set with L", light_pos.x, light_pos.y, light_pos.z);
        ImGui::Text("speed %.2e. Scroll to change", speed);
        ImGui::SeparatorText("params");
        ImGui::SliderFloat("param1.x", &param1.x, -5.0f, 5.0f);
        ImGui::SliderFloat("param1.y", &param1.y, -5.0f, 5.0f);
        ImGui::SliderFloat("param1.z", &param1.z, -5.0f, 5.0f);
        ImGui::SliderFloat("k_a", &k_a, 0.0f, 2.0f);
        ImGui::ColorEdit3("ambient", glm::value_ptr(ambient));
        ImGui::SliderFloat("k_d", &k_d, 0.0f, 2.0f);
        ImGui::ColorEdit3("diffuse", glm::value_ptr(diffuse));
        ImGui::SliderFloat("k_s", &k_s, 0.0f, 2.0f);
        ImGui::SliderFloat("alpha", &alpha, 1.0f, 5.0f);
        ImGui::ColorEdit3("specular", glm::value_ptr(specular));
        ImGui::SliderFloat("occlusion", &occlusion, -10.0f, 10.0f);
    ImGui::End();
}
void App::run(){
    while (!glfwWindowShouldClose(window)) {
        if(curr_mode == RAYMARCHING){
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
            draw_ui();

            glUseProgram(compute_program);
            glActiveTexture(GL_TEXTURE0);

            glUniformMatrix4fv(glGetUniformLocation(compute_program, "inv_view"),1, GL_FALSE, glm::value_ptr(glm::inverse(view)));
            glUniformMatrix4fv(glGetUniformLocation(compute_program, "inv_proj"),1, GL_FALSE, glm::value_ptr(glm::inverse(proj)));
            glUniform2ui(glGetUniformLocation(compute_program, "screen_size"), width,height);
            glUniform3fv(glGetUniformLocation(compute_program, "camera"), 1, glm::value_ptr(pos));
            glUniform3fv(glGetUniformLocation(compute_program, "light_pos"), 1, glm::value_ptr(light_pos));
            glUniform3fv(glGetUniformLocation(compute_program, "param1"), 1, glm::value_ptr(param1));
            glUniform1f(glGetUniformLocation(compute_program, "k_a"), k_a);
            glUniform3fv(glGetUniformLocation(compute_program, "ambient"), 1, glm::value_ptr(ambient));
            glUniform1f(glGetUniformLocation(compute_program, "k_d"), k_d);
            glUniform3fv(glGetUniformLocation(compute_program, "diffuse"), 1, glm::value_ptr(diffuse));
            glUniform1f(glGetUniformLocation(compute_program, "k_s"), k_s);
            glUniform1f(glGetUniformLocation(compute_program, "alpha"), alpha);
            glUniform3fv(glGetUniformLocation(compute_program, "specular"), 1, glm::value_ptr(specular));
            glUniform1f(glGetUniformLocation(compute_program, "occlusion"), occlusion);
            glUniform1f(glGetUniformLocation(compute_program, "time"), glfwGetTime());
            glDispatchCompute((width-1)/32+1, (height-1)/32+1, 1);

            // make sure writing to image has finished before read
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            glUseProgram(blit_program);
            glBindVertexArray(dummy_vao);
            glBindTexture(GL_TEXTURE_2D, texture);
            glDrawArrays(GL_TRIANGLES,0,3);


            utl::enframeIMGUI();
            utl::multiViewportIMGUI(window);
            glfwSwapBuffers(window);
        }
        else if(curr_mode == ATTRACTOR){
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
            draw_ui();

            glClearColor(0.0f, 0.0f, 0.1f, 1.0f);

            glUseProgram(compute_program_attractor);
            glActiveTexture(GL_TEXTURE0);

            glUniformMatrix4fv(glGetUniformLocation(compute_program_attractor, "inv_view"),1, GL_FALSE, glm::value_ptr(glm::inverse(view)));
            glUniformMatrix4fv(glGetUniformLocation(compute_program_attractor, "inv_proj"),1, GL_FALSE, glm::value_ptr(glm::inverse(proj)));
            glUniform2ui(glGetUniformLocation(compute_program_attractor, "screen_size"), width,height);
            glUniform3fv(glGetUniformLocation(compute_program_attractor, "camera"), 1, glm::value_ptr(pos));
            glUniform3fv(glGetUniformLocation(compute_program_attractor, "light_pos"), 1, glm::value_ptr(light_pos));
            glUniform3fv(glGetUniformLocation(compute_program_attractor, "param1"), 1, glm::value_ptr(param1));
            glUniform1f(glGetUniformLocation(compute_program_attractor, "time"), glfwGetTime());
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
}