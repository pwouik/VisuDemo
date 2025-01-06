#version 430 core
	
out vec2 tex_coord;
void main()
{
    float x = (gl_VertexID/2);
    float y = mod(gl_VertexID,2);
    gl_Position = vec4(x*4-1,y*4-1,0.0,0.1);
    tex_coord = vec2(x*2,y*2);
}