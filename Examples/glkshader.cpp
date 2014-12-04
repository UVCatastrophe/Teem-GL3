#include <Hale.h>

#include "glkshader.h"
#include <sys/stat.h>
#include <cstring>
#include <iostream>

ShaderProgram::ShaderProgram(){
  this->fshId = -1;
  this->vshId = -1;
  this->gshId = -1;
  this->progId = glCreateProgram();
}

bool ShaderProgram::vertexShader(const char* file){
  this->vshId = Hale::shaderNew(GL_VERTEX_SHADER, file);
  glAttachShader(this->progId, this->vshId);
  return true;
}

bool ShaderProgram::fragmentShader(const char* file){
  this->fshId = Hale::shaderNew(GL_FRAGMENT_SHADER, file);
  glAttachShader(this->progId, this->fshId);
  return true;
}

bool ShaderProgram::geometryShader(const char* file){
  this->gshId = Hale::shaderNew(GL_GEOMETRY_SHADER, file);
  glAttachShader(this->progId,this->gshId);
  return true;
}

//Taken from the CS237 library created by John Reppy
GLint ShaderProgram::UniformLocation (const char *name)
{
    GLint id = glGetUniformLocation (this->progId, name);
    if (id < 0) {
	std::cerr << "Error: shader uniform \"" << name << "\" is invalid\n" << std::endl;
	exit (1);
    }
    return id;
}
