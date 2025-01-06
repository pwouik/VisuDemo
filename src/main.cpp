#include <stdlib.h>
#include <stdio.h>

#define GLAD_GL_IMPLEMENTATION // Necessary for headeronly version.
#include <glad/gl.h>

#include <GLFW/glfw3.h>
GLfloat cubeVertices[8][3] = { {-0.5, -0.5, -0.5}, {0.5, -0.5, -0.5}, {0.5, 0.5, -0.5}, {-0.5, 0.5, -0.5},
                              {-0.5, -0.5, 0.5},  {0.5, -0.5, 0.5},  {0.5, 0.5, 0.5},  {-0.5, 0.5, 0.5} };
GLfloat cubeColors[6][3] = {
    {1.0, 0.0, 0.0},   // Red
    {0.0, 1.0, 0.0},   // Green
    {0.0, 0.0, 1.0},   // Blue
    {1.0, 1.0, 0.0},   // Yellow
    {1.0, 0.0, 1.0},   // Magenta
    {0.0, 1.0, 1.0}    // Cyan
};
const GLuint WIDTH = 800, HEIGHT = 600;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode) {
    if ((key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
}

int main(void) {
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "[glad] GL with GLFW", NULL, NULL);
    glfwMakeContextCurrent(window);

    glfwSetKeyCallback(window, key_callback);
    float p0x = 0.0;
    float p0y = 0.0;
    float p1x = 0.0;
    float p1y = 0.0;
    float p2x = 0.0;
    float p2y = 0.0;
    int version = gladLoadGL(glfwGetProcAddress);
    printf("GL %d.%d\n", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
    while (!glfwWindowShouldClose(window)) {
        double xpos,ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        {
            p0x=xpos/WIDTH*2.0-1.0;
            p0y=ypos/HEIGHT*2.0-1.0;
        }
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS)
        {
            p1x=xpos/WIDTH*2.0-1.0;
            p1y=ypos/HEIGHT*2.0-1.0;
        }
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
        {
            p2x=xpos/WIDTH*2.0-1.0;
            p2y=ypos/HEIGHT*2.0-1.0;
        }
        glfwPollEvents();
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        {
            
        }
        glClearColor(0.0f, 0.0f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glfwSwapBuffers(window);
    }

    glfwTerminate();

    return 0;
}