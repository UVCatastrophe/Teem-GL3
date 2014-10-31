#version 150 core

uniform mat4 model;

in vec4 position;
in vec3 norm;
in vec4 color;

out VertexData {
    vec3 norm;
    vec4 color;
} VertexOut;

void main(void) { 

    gl_Position = model * position;
    VertexOut.norm = vec3(model * vec4(norm,0.0));
    VertexOut.color = vec4(0.0,0.0,0.0,1.0);

}
