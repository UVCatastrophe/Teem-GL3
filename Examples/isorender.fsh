#version 150 core

uniform vec3 light_dir;

in FragmentData{
    vec3 norm;
    vec4 color;
} fin;
out vec4 fcol;

void main(void) {
/* bp-shading model */
  vec4 light_color = vec4(1.0,1.0,1.0,1.0);

  float a = max(0,dot( normalize(light_dir),normalize(fin.norm)));
  fcol = fin.color + (light_color * a);
  fcol.a = fin.color.a;


}
