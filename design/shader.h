#ifndef _SHADER_H_
#define _SHADER_H_

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

class Shader{
public:
    std::string name;
    ShaderProgram *prog;
    std::vector<PolyDataInstance> instances;
    std::map<std::string, GLuint> unifromLocations;
    
    void newShaderProgramFromFile(const char* vshFile,
                                  const char* fshFile);
    void newShaderProgramFromFile(const char* vshFile,
                                  const char* fshFile,
                                  const char* gshFile);
    void makeActive();
    void addInstance(PolyDataInstance *poly);
    
    void (*prerender)(void*);
    void *prerenderData;
    
private:
    void getAllUniforms();
};