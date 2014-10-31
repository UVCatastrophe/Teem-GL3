#version 150 core

uniform mat4 proj;
uniform mat4 view;
uniform mat4 model;

in vec4 position;
in vec3 norm;
in vec4 color;
out vec3 norm_frag;
out vec4 color_frag;

void main(void) { 

    gl_Position = proj * view * model * position;
    norm_frag = vec3(model * vec4(norm,0.0));
    color_frag = color/(255.0);
    
}
