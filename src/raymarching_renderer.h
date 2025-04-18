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
    void reset();
    void render(float width,float height,glm::vec3 pos,glm::mat4 inv_view, glm::mat4 inv_proj, glm::vec3 light_pos);
private:
    GLuint compute_program;
    bool right_was_pinched = false;
    bool left_was_pinched = false;
    glm::quat start_rotation;
    glm::vec3 start_offset;

    // Smoothing variables
    float smoothing_factor;            // General smoothing factor
    float position_smoothing_factor;   // Position-specific smoothing
    float rotation_smoothing_factor;   // Rotation-specific smoothing
    double last_update_time;           // Time tracking for frame-rate independent smoothing
        
    glm::vec3 fractal_position{0.0f, 0.0f, 0.0f};
    glm::vec3 target_fractal_position{0.0f, 0.0f, 0.0f}; // Target position for smoothing
    glm::quat fractal_rotation = glm::identity<glm::quat>();
    glm::quat target_fractal_rotation = glm::identity<glm::quat>(); // Target rotation for smoothing
    glm::quat left_start_rotation;
    glm::vec3 offset;
    glm::vec3 rotation;
    float scale;
    float color_freq;
    float hue;
    float min_light;
    float k_d;
    float k_s;
    float alpha;
    glm::vec3 specular;
    glm::vec3 bgcolor;
    glm::vec3 bloomcolor;
    glm::vec3 aocolor;
    float occlusion;
};