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
#include <algorithm> //for std::fill
#include <vector>

// #include <thread> //this therad
// #include <chrono> //sleep

#include "mathracttor.hpp" //utility function for attractors



#define BUFFER_OFFSET(offset) ((GLvoid*)(offset))

using namespace std;

#define NBPTS 20000000


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

namespace gbl{
    bool paused = false;
}
namespace atr{
    GLfloat* blackData; //need a blackscreen to reset
    GLuint ssbo_pts; //ssbo of points
    
    //glm::mat4 for attractors
    GLuint uboM4;


    void randArray(float* array, int size, float range){
        for(int i=0; i<size; i+=4){
            array[i] = range * (((float)rand()/RAND_MAX) *2 -1);
            array[i+1] = range * (((float)rand()/RAND_MAX) *2 -1);
            array[i+2] = range * (((float)rand()/RAND_MAX) *2 -1);
            array[i+3] = 1;
        }
    }

    void origindArray(float* array, int size){
        for(int i=0; i<size; i+=4){
            //which is x, whihc is y, which is z, which is w ?
            array[i] = 0.0f; 
            array[i+1] = 0.0f;
            array[i+2] = 0.0f;
            array[i+3] = 1.0f;
        }
    }


    void init_data(int w, int h){
        blackData = new GLfloat[w * h * 4];
        std::fill(blackData, blackData + w * h * 4, 0.0f);

        //generates points SSBO
        float* data = new float[NBPTS*4];
        randArray(data, NBPTS, 1);
        //origindArray(data, NBPTS);
        glGenBuffers(1, &ssbo_pts);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_pts);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * 4 * NBPTS, data, GL_DYNAMIC_DRAW); //GL_DYNAMIC_DRAW update occasionel et lecture frequente
        //glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data); //to update partially
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssbo_pts);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // unbind

        delete[] data; //points memory only needed on GPU

        //attractors (a max of 10 matrices stored as UBO)
        glGenBuffers(1, &uboM4);
        glBindBuffer(GL_UNIFORM_BUFFER, uboM4);
        glBufferData(GL_UNIFORM_BUFFER, 10 * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW); // Allocate space for up to 10 matrices
        glBindBufferBase(GL_UNIFORM_BUFFER, 1, uboM4); // Binding point 0
        glBindBuffer(GL_UNIFORM_BUFFER, 0); // Unbind

        //reserve matrices to be pushed to UBO each frame
        uvl::ubo_matrices.reserve(10);
    }
    void clean_data(){
        delete[] blackData;
        //todo unbind ssbo
    }

    void clearTexture(int w, int h, GLuint texture){
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_FLOAT, blackData);
    }

}


App::App(int w,int h)
{
    DEBUG("aaa");
    pos = glm::vec3(0.0,0.0,0.0);
    light_pos = glm::vec3(0.0,0.0,0.0);
    param1 = glm::vec3( -4.54f, -1.26f, 0.1f );
    width = w;
    height = h;
    speed = 1.0;

    DEBUG("aaa1");
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    DEBUG("aaa2");
    window = glfwCreateWindow(width, height, "[glad] GL with GLFW", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, onKey);
    glfwSetCursorPosCallback (window, onMouse);
    glfwSetScrollCallback(window, onScroll);
    glfwSetFramebufferSizeCallback(window, onResize);

    utl::initIMGUI(window);
    DEBUG("aaa3");

    int version = gladLoadGL(glfwGetProcAddress);
    printf("GL %d.%d\n", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
    
    DEBUG("aaa4");
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

    prm::defaults();
}

void App::draw_ui(){
    ImGui::Begin("Base info");
        ImGui::SeparatorText("current rendering");
            const char* items_cb1[] = { "Raymarching", "Attractor"}; //MUST MATCH DEFINE IN app.h !
            if(ImGui::Combo("combo", &curr_mode, items_cb1, IM_ARRAYSIZE(items_cb1))){
                gbl::paused = true;
                if(curr_mode == ATTRACTOR){
                    atr::init_data(width, height);
                }
                else {
                    atr::clean_data();
                }
                gbl::paused = false;
            }
        ImGui::SeparatorText("generic debug info");
        ImGui::Text("camera in %.2fx, %.2fy, %.2fz", pos.x, pos.y, pos.z);
        ImGui::Text("light in %.2fx, %.2fy, %.2fz . Set with L", light_pos.x, light_pos.y, light_pos.z);
        ImGui::Text("speed %.2e. Scroll to change", speed);
        ImGui::SeparatorText("params");
        ImGui::SliderFloat("param1.x", &param1.x, -5.0f, 5.0f);
        ImGui::SliderFloat("param1.y", &param1.y, -5.0f, 5.0f);
        ImGui::SliderFloat("param1.z", &param1.z, -5.0f, 5.0f);
    ImGui::End();
}


void App::draw_ui_attractor(){
    ImGui::Begin("attractir");
        ImGui::SeparatorText("debug");
        if(ImGui::Button("pref Speed")) speed = 0.025f;
        if(ImGui::Button("reset cam")) pos = glm::vec3(0.0,0.0,-0.5);
        
        ImGui::SeparatorText("attractor preset");
        if(ImGui::Button("Sierpinski")) uvl::set_sierpinski();
        if(ImGui::Button("fixed process")) uvl::set_fixedProcess();

        ImGui::SeparatorText("random ranges");
            ui::param_settings();

        ImGui::SeparatorText("Attractors");
            ImGui::SliderFloat("slider float", &uvl::lerpFactor, 0.0f, 1.0f, "lerp : %.3f");
            ImGui::InputInt("nb", &uvl::matrixPerAttractor);
            utl::HelpMarker("The number of different random matrices per attractor. Live editing may fuck up everything");
            ImGui::InputInt("immobile count", &uvl::immobileCount);
            utl::HelpMarker("pseudo random reason or idk. given a random int between 0 and nb+immobile count point will move if random values is less than nb to corresponding attractionfunction[random] check .comp if unclear");

        if (ImGui::TreeNode("Attractor A")){
            uvl::A_tractor.ui();

            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Attractor B")){
            uvl::B_tractor.ui();

            ImGui::TreePop();
        }


    ImGui::End();
}

void App::run(){
    while (!glfwWindowShouldClose(window)) {
        if(curr_mode == RAYMARCHING){
            if(gbl::paused) continue;
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

            glUseProgram(compute_program);
            glActiveTexture(GL_TEXTURE0);

            glUniformMatrix4fv(glGetUniformLocation(compute_program, "inv_view"),1, GL_FALSE, glm::value_ptr(glm::inverse(view)));
            glUniformMatrix4fv(glGetUniformLocation(compute_program, "inv_proj"),1, GL_FALSE, glm::value_ptr(glm::inverse(proj)));
            glUniform2ui(glGetUniformLocation(compute_program, "screen_size"), width,height);
            glUniform3fv(glGetUniformLocation(compute_program, "camera"), 1, glm::value_ptr(pos));
            glUniform3fv(glGetUniformLocation(compute_program, "light_pos"), 1, glm::value_ptr(light_pos));
            glUniform3fv(glGetUniformLocation(compute_program, "param1"), 1, glm::value_ptr(param1));
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
        else if(curr_mode == ATTRACTOR){
            if(gbl::paused) continue;
            glfwPollEvents();
            

            if(glfwGetKey(window, GLFW_KEY_Q)==GLFW_PRESS) lockcam = !lockcam;
       
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
            draw_ui_attractor();

            glClearColor(0.0f, 0.0f, 0.1f, 1.0f);

            //if(view!=old_view) //when doing test on UI we need update but we are still
                atr::clearTexture(width, height, texture); //balck background for texture
            old_view=view;
            glUseProgram(compute_program_attractor);
            glActiveTexture(GL_TEXTURE0);

            glUniformMatrix4fv(glGetUniformLocation(compute_program_attractor, "view"),1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(compute_program_attractor, "proj"),1, GL_FALSE, glm::value_ptr(proj));
            glUniform2ui(glGetUniformLocation(compute_program_attractor, "screen_size"), width,height);
            glUniform3fv(glGetUniformLocation(compute_program_attractor, "camera"), 1, glm::value_ptr(pos));
            glUniform3fv(glGetUniformLocation(compute_program_attractor, "light_pos"), 1, glm::value_ptr(light_pos));
            glUniform1f(glGetUniformLocation(compute_program_attractor, "time"), glfwGetTime());
            glUniform1i(glGetUniformLocation(compute_program_attractor, "matrixPerAttractor"),uvl::matrixPerAttractor);
            glUniform1i(glGetUniformLocation(compute_program_attractor, "immobileCount"),uvl::immobileCount);
            glUniform1i(glGetUniformLocation(compute_program_attractor, "randInt_seed"),rand()%RAND_MAX);
            
            //send attractor data to compute shader
            //TODO HERE uncommnet
            uvl::update_ubo_matrices();
            glBindBuffer(GL_UNIFORM_BUFFER, atr::uboM4);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, uvl::ubo_matrices.size() * sizeof(glm::mat4), uvl::ubo_matrices.data());
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
            
            glDispatchCompute((NBPTS-1)/1024+1, 1, 1);

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