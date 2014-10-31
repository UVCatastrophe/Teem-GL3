#version 150 core

uniform vec3 light_dir;

in vec4 color_frag;
in vec3 norm_frag;
out vec4 fcol;

void main(void) {
/* bp-shading model */
  vec4 light_color = vec4(1.0,1.0,1.0,1.0);

  float a = max(0,dot( normalize(light_dir),normalize(norm_frag)));
  fcol = color_frag + (light_color * a);
  fcol.a = color_frag.a;

}
