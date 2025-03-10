#pragma once
#include <LeapC.h>
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>

class RaymarchingRenderer{
    public:
        RaymarchingRenderer();
        void leap_update(const LEAP_TRACKING_EVENT& frame);
        void draw_ui();
        void render(float width,float height,glm::vec3 pos,glm::mat4 inv_view, glm::mat4 inv_proj, glm::vec3 light_pos);
    private:
        GLuint compute_program;
        bool right_was_pinched = false;
        bool left_was_pinched = false;
        glm::quat start_rotation;
        glm::vec3 start_offset;

        glm::vec3 fractal_position{0.0f, 0.0f, 0.0f};
        glm::quat fractal_rotation = glm::identity<glm::quat>();
        glm::quat left_start_rotation;
        glm::vec3 offset;
        glm::vec3 rotation;
        float scale;
        float color_freq;
        float min_light;
        float k_d;
        float k_s;
        float alpha;
        glm::vec3 specular;
        float occlusion;
};