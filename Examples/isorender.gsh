#version 150 core

uniform vec3 origin;
uniform vec3 ray;
uniform int pixel_trace_on;

uniform mat4 proj;
uniform mat4 view;

layout(triangles) in;
layout (triangle_strip, max_vertices=3) out;

in VertexData {
    vec3 norm;
    vec4 color;
} VertexIn[];

out FragmentData {
     vec3 norm;
     vec4 color;
}fout;


void main(){
     bool paint = false;
     if(pixel_trace_on != 0){
       vec3 p0 = vec3(gl_in[0].gl_Position);
       vec3 p1 = vec3(gl_in[1].gl_Position);
       vec3 p2 = vec3(gl_in[2].gl_Position);
       vec3 normal = cross((p1-p0),(p2-p0));
       float t = -(dot(normal,origin) + dot(normal,p0)) / dot(ray,normal);
       vec3 x = origin + t*ray;

       if( dot(cross((p1-p0),(x-p0)),normal) >= 0 &&
           dot(cross((p2-p1),(x-p1)),normal) >= 0 &&
	   dot(cross((p0-p2),(x-p2)),normal) >= 0)
           paint = true;
     }
     for(int i = 0; i < gl_in.length(); i++){
         gl_Position = proj*view*(gl_in[i].gl_Position);
         fout.norm = VertexIn[i].norm;
	 gl_PrimitiveID = gl_PrimitiveIDIn;
         if(paint)
            fout.color = vec4(1.0,0,0,1.0);
	 else
	    fout.color = VertexIn[i].color;
	 EmitVertex();
     }

}
