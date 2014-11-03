#ifndef _SHADER_H_
#define _SHADER_H_

#if defined(__APPLE_CC__)
#define GLFW_INCLUDE_GLCOREARB
#endif

#define GL_GLEXT_PROTOTYPES
#include <GLFW/glfw3.h>

class ShaderProgram{
 public:
  GLuint progId;
  GLuint vshId; //Vertex Shader index
  GLuint fshId; //Fragment Shader index
  GLuint gshId; //Geometry Shader index

  bool vertexShader(const char* file);
  bool fragmentShader(const char* file);
  bool geometryShader(const char* file);

  bool geometryEnabled();

  GLint UniformLocation (const char *name);

  ShaderProgram (); 

 protected:

  void shader_LoadFromFile (const char *file, int kind,int id);

};

#endif
