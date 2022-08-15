#version 450

layout( push_constant ) uniform PushConstant 
{ 
	mat4 mvp; 
};

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_tex_coord;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() 
{
    gl_Position = mvp *  vec4(in_position, 1.0);
}
