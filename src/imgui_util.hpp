/*
You would be right to notice that there's no reason for function here to static

The point is that for some reason linkage go boom if not

So they'll just stay static for now

*/
#pragma once
#include "glm/glm.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <string>


//UI util
#define SL ImGui::SameLine();
#define CW ImGui::SetNextItemWidth(inputWidth);
//generate unique ID for ImGui input fields
#define UID(val) (std::string("##") + std::to_string(reinterpret_cast<uintptr_t>(&val))).c_str()
#define UIDT(txt, val) (std::string(txt) + std::string("##") + std::to_string(reinterpret_cast<uintptr_t>(&val))).c_str()

static void initIMGUI(GLFWwindow* window){
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         //enables multi viewport

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);          // Second param install_callback=true will install GLFW callbacks and chain to existing ones.
    ImGui_ImplOpenGL3_Init();
}

static void newframeIMGUI(){
    // (Your code calls glfwPollEvents())
    // ...
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

static void enframeIMGUI(){
    // Rendering
    // (Your code clears your framebuffer, renders your other stuff etc.)
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    // (Your code calls glfwSwapBuffers() etc.)
}

static void multiViewportIMGUI(GLFWwindow* window){
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable){
        // Update and Render additional Platform Windows
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();

        // TODO for OpenGL: restore current GL context.
        glfwMakeContextCurrent(window);
    }
}

static void shutdownIMGUI(){
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

static float inputWidth = 45.0f; //width for most input float

//editable field for float of float vector
static void valf(const char* desc, float& v, float speed = 0.005f, float minv = 0.0f, float maxv = 1.0f){
    ImGui::Text(desc);
    SL CW ImGui::DragFloat(UID(v), &v, speed, minv, maxv, "%.2f");
}


static void valf(const char* desc, glm::vec3& v, float minv = -1.0f, float maxv = 1.0f){
    ImGui::Text(desc);
    SL ImGui::DragFloat3(UID(v),&v.x, 0.005f, minv, maxv);
}

static void valf(const char* desc, glm::mat4& mat, float speed = 0.005f, float minv = -1.0f, float maxv = 1.0f){
    ImGui::Text(desc);
    for (int row = 0; row < 4; ++row) {
        //we cannot use dragfloat4 because it would be column first or else it would add some messing with tranpose
            CW ImGui::DragFloat(UID(mat[0][row]), &mat[0][row], speed, minv, maxv, "%.2f");
        SL CW ImGui::DragFloat(UID(mat[1][row]), &mat[1][row], speed, minv, maxv, "%.2f");
        SL CW ImGui::DragFloat(UID(mat[2][row]), &mat[2][row], speed, minv, maxv, "%.2f");
        SL CW ImGui::DragFloat(UID(mat[3][row]), &mat[3][row], speed, minv, maxv, "%.2f");
    }
}
static void HelpMarker(const char* desc){
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}
