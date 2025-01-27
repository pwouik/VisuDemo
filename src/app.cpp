#include "app.h"
#include <GLFW/glfw3.h>
#include <LeapC.h>
#include <climits>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <glm/ext/matrix_transform.hpp>
#include <glm/fwd.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include <iostream>
#include <memory>
#include <optional>
#include <vector>

#include "glm/gtc/quaternion.hpp"
#include "leap_connection.h"
#include "glm/gtc/type_ptr.hpp"
#include "glm/matrix.hpp"
#include "glm/ext.hpp"
#include <algorithm> //for std::fill
#include <mutex>
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

void linkProgram(GLuint program){
    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        printf("Shader program failed to link:\n");
        GLint logSize;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logSize);
        char* logMsg = new char[logSize];
        glGetProgramInfoLog(program, logSize, nullptr, logMsg);
        printf("%s\n", logMsg);
        delete[] logMsg;
        exit(EXIT_FAILURE);
    }
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
        uvl::fixedProcessInit();
    }
    void clean_data(){
        delete[] blackData;
        uvl::A_tractor.freeAll();
        uvl::B_tractor.freeAll();

        //todo unbind ssbo
    }

    //not used anywhere ? must be deleted
    void clearTexture(int w, int h, GLuint texture){
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_FLOAT, blackData);
    }

}


App::App(int w,int h)
{
    pos = glm::vec3(0.0,0.0,0.0);
    light_pos = glm::vec3(0.0,0.0,0.0);
    param1 = glm::vec3( -3.0f, 1.0f, 0.55f );
    param2 = glm::vec3( 1.0f, 1.1f, 2.2f );
    occlusion = 4.5;
    ambient = glm::vec3(1.0,0.6,0.9);
    diffuse = glm::vec3(1.0,0.6,0.0);
    specular = glm::vec3(1.0,1.0,1.0);
    k_a = 0.32f;
    k_d = 1.0;
    k_s = 1.0;
    alpha = 3.0;
    speed = 0.02f;
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
    DEBUG("ImGui initialized");

    int version = gladLoadGL(glfwGetProcAddress);
    printf("GL %d.%d\n", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
    
    //compute_program for ray marching
    compute_program = glCreateProgram();
    GLint comp = loadshader("shaders/render.comp", GL_COMPUTE_SHADER);
    glAttachShader(compute_program, comp);
    glLinkProgram(compute_program);
    linkProgram(compute_program);
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

    //depth texture (texture1, binding 1)
    glGenTextures(1, &depth_texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depth_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32I, w, h, 0, GL_RED_INTEGER, GL_INT, nullptr);
    //required for some reason on my computer
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0); 
    /*Ikd what those are are supposed to do
    
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); //either this or both line above is required for me to not get a white screen
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); //doesnt appear to do anyting
    //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);  //doesn't appear to do anythin
    */
    glBindImageTexture(1, depth_texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32I); //bind to channel 1

    //distance from last jump texture (texture 2, binding4)
    glGenTextures(1, &jumpdist_texture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, jumpdist_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32I, w, h, 0, GL_RED_INTEGER, GL_INT, nullptr); 
    //required for some reason on my computer
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0); 
    glBindImageTexture(4, jumpdist_texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32I); //bind to channel 4


    //compute_program for attractor
    compute_program_attractor = glCreateProgram();
    GLint comp_attractor = loadshader("shaders/render_attractor.comp", GL_COMPUTE_SHADER);
    glAttachShader(compute_program_attractor, comp_attractor);
    glLinkProgram(compute_program_attractor);
    linkProgram(compute_program_attractor);

    ssao_attractor = glCreateProgram();
    GLint ssao_shader = loadshader("shaders/ssao_attractor.comp", GL_COMPUTE_SHADER);
    glAttachShader(ssao_attractor, ssao_shader);
    glLinkProgram(ssao_attractor);
    linkProgram(ssao_attractor);


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

    proj = glm::perspective(glm::radians(70.0f),(float)width/(float)height,0.1f,10000.0f);
    glUniformMatrix4fv(glGetUniformLocation(compute_program, "persp"),1, GL_FALSE, glm::value_ptr(proj));

    prm::defaults();
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
                gbl::paused = true;
                if(curr_mode == Attractor){
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
        ImGui::Text("fractal at %.2fx, %.2fy, %.2fz. Move with arrows", fractal_position.x, fractal_position.y, fractal_position.z);
        ImGui::Text("fractal rotation at pitch: %.2f, yaw: %.2f, roll: %.2f. Change with numpad 1, 2 or 3", fractal_rotation.x, fractal_rotation.y, fractal_rotation.z);
        ImGui::Text("speed %.2e. Scroll to change", speed);
        ImGui::SeparatorText("params");
        ImGui::SliderFloat3("param1", glm::value_ptr(param1), -4.0, 4.0);
        ImGui::SliderFloat3("param2", glm::value_ptr(param2), -4.0, 4.0);
        ImGui::SliderFloat("k_a", &k_a, 0.0f, 2.0f);
        ImGui::ColorEdit3("ambient", glm::value_ptr(ambient));
        ImGui::SliderFloat("k_d", &k_d, 0.0f, 2.0f);
        ImGui::ColorEdit3("diffuse", glm::value_ptr(diffuse));
        ImGui::SliderFloat("k_s", &k_s, 0.0f, 2.0f);
        ImGui::SliderFloat("alpha", &alpha, 1.0f, 5.0f);
        ImGui::ColorEdit3("specular", glm::value_ptr(specular));
        ImGui::SliderFloat("occlusion", &occlusion, -10.0f, 10.0f);
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


void App::draw_ui_attractor(){
    ImGui::Begin("attractor");
        if(ImGui::CollapsingHeader("Debug & all", ImGuiTreeNodeFlags_DefaultOpen )){
            const char* items_cb2[] = { "per matrix", "per component"}; //MUST MATCH ENUM IN app.h !
            ImGui::SliderFloat("##lerpfactor", &uvl::lerpFactor, 0.0f, 1.0f, "lerp : %.3f");
            if(ImGui::Combo("lerping mode", (int*)&lerpmode, items_cb2, IM_ARRAYSIZE(items_cb2))){}
            
            ImGui::Checkbox("no clear", &ani::no_clear);
                ui::HelpMarker("Do not clear previous frame if view didn't changed");
            ImGui::Checkbox("Idle animation", &ani::idle);
                ui::HelpMarker("camera enter an idle spinning animation if checked");

            if(ImGui::TreeNode("Idle params")){
                ImGui::DragFloat("Spin period", &ani::spin_Period, 0.01f, 3.0f,10.0f,"%.2f");
                ImGui::DragFloat("Lerp period", &ani::lerp_period, 0.01f, 3.0f,10.0f,"%.2f");
                ImGui::DragFloat2("heigh & distance", ani::height_and_distance, 0.01f,-3.0f,3.0f,"%.2f");
                ImGui::DragFloat("Lerp stiffness", &ani::lerp_stiffness, 0.01f, 0.5f,20.0f,"%.2f");
                ui::HelpMarker("The stifness of the smoothing curve"
                    "The function is a sigmoid mapranged to range 0-1, ie :"
                    "(1/(1+exp(-k(2x-1))) - mv) * 1/(1-2mv)"
                    "where mv is the value at 0,  1+(1+exp(k))");
                ImGui::TreePop();
            }
            ui::param_settings();

            if(ImGui::TreeNode("Other Utils")){
                if(ImGui::Button("pref Speed")) speed = 0.025f;
                if(ImGui::Button("reset cam")) pos = glm::vec3(0.0,0.0,-0.5);

                ImGui::TreePop();
            }
        }

        if(ImGui::CollapsingHeader("Cool preset")){
            if(ImGui::Button("Sierpinski")) preset::sierpinski();
            if(ImGui::Button("Sierpintagon")) preset::sierpintagon();
            if(ImGui::Button("Sierpolygon")) preset::sierpolygon(); 
            SL ImGui::SetNextItemWidth(60); ImGui::DragInt("side", &preset::nb_cote, 1, 3, 10, "%d", ImGuiSliderFlags_AlwaysClamp);
            if(ImGui::Button("Barnsley Fern")) preset::barsnley_fern(); 
        }


        if(ImGui::CollapsingHeader("Attractors")){
            ImGui::SliderInt("nb functions", &uvl::matrixPerAttractor, 3, 10);
                utl::HelpMarker("The number of different random matrices per attractor. going above ten will crash the program");
            
            if (ImGui::TreeNode("Attractor A")){
                uvl::A_tractor.ui(uvl::matrixPerAttractor);

                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Attractor B")){
                uvl::B_tractor.ui(uvl::matrixPerAttractor);

                ImGui::TreePop();
            }
        }
        if(ImGui::CollapsingHeader("Coloring", ImGuiTreeNodeFlags_DefaultOpen )){
            ImGui::Text("temporary, must be hardcoded when it'll look nice");
            if(ImGui::TreeNode("Jump Distance MapRange")){
                ImGui::DragFloat("from min",&clr::JD_FR_MIN, 0.005f, 0.0f, 2.0f, "%.2f");
                ImGui::DragFloat("from max",&clr::JD_FR_MAX, 0.005f, 0.0f, 2.0f, "%.2f");
                ImGui::DragFloat("to min",&clr::JD_TO_MIN, 0.005f, 0.0f, 2.0f, "%.2f");
                ImGui::DragFloat("to max",&clr::JD_TO_MAX, 0.005f, 0.0f, 2.0f, "%.2f");

                ImGui::TreePop();
            }
            if(ImGui::TreeNode("Phong parameters")){
                ImGui::SliderFloat("k_a##attractor", &clr::k_a, 0.0f, 1.0f);
                ImGui::ColorEdit3("ambient##attractor", glm::value_ptr(clr::col_ambient));
                ImGui::SliderFloat("k_d##attractor", &clr::k_d, 0.0f, 1.0f);
                ImGui::ColorEdit3("diffuse##attractor", glm::value_ptr(clr::col_diffuse));
                ImGui::SliderFloat("k_s##attractor", &clr::k_s, 0.0f, 1.0f);
                ImGui::SliderFloat("alpha##attractor", &clr::alpha, 0.1f, 20.0f);
                ImGui::ColorEdit3("specular##attractor", glm::value_ptr(clr::col_specular));

                ImGui::TreePop();
            }
        }

    ImGui::End();
}

void App::run(){
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if(gbl::paused) continue;
        updateFps();
        
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
        if(glfwGetKey(window, GLFW_KEY_SPACE)==GLFW_PRESS){
            pos.y += speed;
        }
        if(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS){
            pos.y -= speed;
        }
        if(glfwGetKey(window, GLFW_KEY_L)==GLFW_PRESS){
            light_pos = pos;
        }

        glm::mat4 inv_camera_view = glm::inverse(glm::translate(glm::rotate(
                glm::rotate(glm::identity<glm::mat4>(),
                    -glm::radians(pitch),
                    glm::vec3(1.0, 0.0, 0.0)),
                -glm::radians(yaw),
                glm::vec3(0.0, 1.0, 0.0)),
            glm::vec3(-pos.x, -pos.y, -pos.z)));
        if(curr_mode == Raymarching){
            std::lock_guard<std::mutex> lock(leapmotion_mutex);
            if(glfwGetKey(window, GLFW_KEY_UP)==GLFW_PRESS){
                fractal_position += glm::vec3(-1.0f ,0.0f, 0.0f) * speed;
            }
            if(glfwGetKey(window, GLFW_KEY_DOWN)==GLFW_PRESS){
                fractal_position += glm::vec3(1.0f ,0.0f, 0.0f) * speed;
            }
            if(glfwGetKey(window, GLFW_KEY_LEFT)==GLFW_PRESS){
                fractal_position += glm::vec3(0.0f ,0.0f, 1.0f) * speed;
            }
            if(glfwGetKey(window, GLFW_KEY_RIGHT)==GLFW_PRESS){
                fractal_position += glm::vec3(0.0f ,0.0f, -1.0f) * speed;
            }
            if(glfwGetKey(window, GLFW_KEY_PAGE_UP)==GLFW_PRESS){
                fractal_position += glm::vec3(0.0f ,1.0f, 0.0f) * speed;
            }
            if(glfwGetKey(window, GLFW_KEY_PAGE_DOWN)==GLFW_PRESS){
                fractal_position += glm::vec3(0.0f ,-1.0f, 0.0f) * speed;
            }
            if(glfwGetKey(window, GLFW_KEY_L)==GLFW_PRESS){
                light_pos = pos;
            }

            glm::mat4 fractal_transform =  glm::translate(glm::identity<glm::mat4>(),fractal_position) * glm::toMat4(fractal_rotation);
            glm::mat4 total_transform = glm::inverse(fractal_transform)*inv_camera_view;

            utl::newframeIMGUI();
            draw_ui();

            glUseProgram(compute_program);

            glUniformMatrix4fv(glGetUniformLocation(compute_program, "inv_view"),1, GL_FALSE, glm::value_ptr(total_transform));
            glUniformMatrix4fv(glGetUniformLocation(compute_program, "inv_proj"),1, GL_FALSE, glm::value_ptr(glm::inverse(proj)));
            glUniform2ui(glGetUniformLocation(compute_program, "screen_size"), width,height);
            glUniform3fv(glGetUniformLocation(compute_program, "camera"), 1, glm::value_ptr(pos));
            glUniform3fv(glGetUniformLocation(compute_program, "light_pos"), 1, glm::value_ptr(glm::inverse(fractal_transform)*glm::vec4(light_pos,1.0)));
            glUniform3fv(glGetUniformLocation(compute_program, "param1"), 1, glm::value_ptr(param1));
            glUniform3fv(glGetUniformLocation(compute_program, "param2"), 1, glm::value_ptr(param2));
            glUniform1f(glGetUniformLocation(compute_program, "k_a"), k_a);
            glUniform3fv(glGetUniformLocation(compute_program, "ambient"), 1, glm::value_ptr(ambient));
            glUniform1f(glGetUniformLocation(compute_program, "k_d"), k_d);
            glUniform3fv(glGetUniformLocation(compute_program, "diffuse"), 1, glm::value_ptr(diffuse));
            glUniform1f(glGetUniformLocation(compute_program, "k_s"), k_s);
            glUniform1f(glGetUniformLocation(compute_program, "alpha"), alpha);
            glUniform3fv(glGetUniformLocation(compute_program, "specular"), 1, glm::value_ptr(specular));
            glUniform1f(glGetUniformLocation(compute_program, "occlusion"), occlusion);
            glUniform1f(glGetUniformLocation(compute_program, "time"), glfwGetTime());
            glDispatchCompute((width-1)/8+1, (height-1)/8+1, 1);

            // make sure writing to image has finished before read
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            glUseProgram(blit_program);
            glBindVertexArray(dummy_vao);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture);
            glDrawArrays(GL_TRIANGLES,0,3);

        }
        else if(curr_mode == Attractor){

            utl::newframeIMGUI();
            draw_ui();
            draw_ui_attractor();

            
            if(!ani::no_clear || inv_camera_view!=old_view){
                int depth_clear = INT_MIN;
                glClearTexImage(depth_texture,0, GL_RED_INTEGER,GL_INT,&depth_clear);
                glClearTexImage(jumpdist_texture,0, GL_RED_INTEGER,GL_INT,&depth_clear);
            }

            //overwrite camera view if idling
            if(ani::idle){
                //some optimizing could be done about inversing multiples times camera view
                inv_camera_view = glm::inverse(ani::getIdleView((float)glfwGetTime()));
            }
            
            old_view=inv_camera_view;
            glUseProgram(compute_program_attractor);

            glUniformMatrix4fv(glGetUniformLocation(compute_program_attractor, "view"),1, GL_FALSE, glm::value_ptr(glm::inverse(inv_camera_view)));
            glUniformMatrix4fv(glGetUniformLocation(compute_program_attractor, "proj"),1, GL_FALSE, glm::value_ptr(proj));
            glUniform2ui(glGetUniformLocation(compute_program_attractor, "screen_size"), width,height);
            glUniform3fv(glGetUniformLocation(compute_program_attractor, "camera"), 1, glm::value_ptr(pos));
            glUniform3fv(glGetUniformLocation(compute_program_attractor, "light_pos"), 1, glm::value_ptr(light_pos));
            glUniform1f(glGetUniformLocation(compute_program_attractor, "time"), glfwGetTime());
            glUniform1i(glGetUniformLocation(compute_program_attractor, "matrixPerAttractor"),uvl::matrixPerAttractor);
            glUniform1i(glGetUniformLocation(compute_program_attractor, "randInt_seed"),rand()%RAND_MAX);
            
            //send attractor data to compute shader
            uvl::update_ubo_matrices(lerpmode);
            glBindBuffer(GL_UNIFORM_BUFFER, atr::uboM4);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, uvl::matrixPerAttractor * sizeof(glm::mat4), uvl::ubo_matrices.data());
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
            
            glDispatchCompute((NBPTS-1)/1024+1, 1, 1);

            // make sure writing to image has finished before read
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);


            glUseProgram(ssao_attractor);
            glUniform2ui(glGetUniformLocation(ssao_attractor, "screen_size"), width,height);
            glUniformMatrix4fv(glGetUniformLocation(ssao_attractor, "inv_view"),1, GL_FALSE, glm::value_ptr(inv_camera_view));
            glUniformMatrix4fv(glGetUniformLocation(ssao_attractor, "inv_proj"),1, GL_FALSE, glm::value_ptr(glm::inverse(proj)));
            glUniform3fv(glGetUniformLocation(ssao_attractor, "camera"), 1, glm::value_ptr(pos)); //here todo
            glUniform1f(glGetUniformLocation(ssao_attractor, "JD_FR_MIN"), clr::JD_FR_MIN);
            glUniform1f(glGetUniformLocation(ssao_attractor, "JD_FR_MAX"), clr::JD_FR_MAX);
            glUniform1f(glGetUniformLocation(ssao_attractor, "JD_TO_MIN"), clr::JD_TO_MIN);
            glUniform1f(glGetUniformLocation(ssao_attractor, "JD_TO_MAX"), clr::JD_TO_MAX);
            glUniform1f(glGetUniformLocation(ssao_attractor, "k_a"), clr::k_a);
            glUniform3fv(glGetUniformLocation(ssao_attractor, "col_ambient"), 1, glm::value_ptr(clr::col_ambient));
            glUniform1f(glGetUniformLocation(ssao_attractor, "k_d"), clr::k_d);
            glUniform3fv(glGetUniformLocation(ssao_attractor, "col_diffuse"), 1, glm::value_ptr(clr::col_diffuse));
            glUniform1f(glGetUniformLocation(ssao_attractor, "k_s"), clr::k_s);
            glUniform1f(glGetUniformLocation(ssao_attractor, "alpha"), clr::alpha);
            glUniform3fv(glGetUniformLocation(ssao_attractor, "col_specular"), 1, glm::value_ptr(clr::col_specular));
            glDispatchCompute((width-1)/32+1, (height-1)/32+1, 1);

            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            glClear(GL_COLOR_BUFFER_BIT);

            glUseProgram(blit_program);
            glBindVertexArray(dummy_vao);
            glBindTexture(GL_TEXTURE_2D, texture);
            glDrawArrays(GL_TRIANGLES,0,3);

        }
        utl::enframeIMGUI();
        utl::multiViewportIMGUI(window);
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
        std::cout << "Frame " << frame.info.frame_id << " with " << frame.nHands << " hands. ";
        std::optional<LEAP_HAND> left = std::nullopt;
        std::optional<LEAP_HAND> right = std::nullopt;
        for (int i = 0; i < frame.nHands; i++)
        {
            if (frame.pHands[i].type == eLeapHandType_Left && frame.pHands[i].confidence > 0.1)
            {
                left = frame.pHands[i];
                continue;
            }
            if (frame.pHands[i].type == eLeapHandType_Right && frame.pHands[i].confidence > 0.1)
            {
                right = frame.pHands[i];
            }
        }
        static bool left_was_pinched = false;
        static bool right_was_pinched = false;
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
                fractal_position += glm::make_vec3(left.value().palm.velocity.v) * 7.5e-5;
                fractal_rotation *= glm::mix(glm::identity<glm::quat>(), glm::inverse(left_start_rotation) * palm_orientation, 0.75f);
                left_start_rotation = palm_orientation;
            }
            else
            {
                left_was_pinched = false;
            }
        }
        else
        {
            hasLeftHand = false;
            left_was_pinched = false;
        }
        if (right.has_value())
        {
            hasRightHand = true;
            static glm::quat start_param1;
            static glm::vec3 start_param2;
            if (right.value().pinch_distance < 25){
                if (!right_was_pinched)
                {
                    right_was_pinched = true;
                    start_param1 = glm::make_quat(right.value().palm.orientation.v);
                    start_param2 = glm::make_vec3(right.value().palm.position.v);
                }
                param1 += glm::eulerAngles(glm::inverse(start_param1) * glm::make_quat(right.value().palm.orientation.v));
                param2 += (start_param2 - glm::make_vec3(right.value().palm.position.v)) / 5e2;
                start_param1 = glm::make_quat(right.value().palm.orientation.v);
                start_param2 = glm::make_vec3(right.value().palm.position.v);
            }
            else
            {
                right_was_pinched = false;
            }
        }
        else
        {
            hasRightHand = false;
            right_was_pinched = false;
        }
        std::cout << "\n";
    }