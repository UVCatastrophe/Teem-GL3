#include <iostream>

/*

https://wiki.wxwidgets.org/Valgrind_Suppression_File_Howto

valgrind --leak-check=full --show-reachable=yes --error-limit=no --gen-suppressions=all --log-file=supress.log ./glkjdemo

./surpress-filter.sh < supress.log > ! supress.txt

valgrind --leak-check=full --show-reachable=yes --error-limit=no --suppressions=supress.txt ./glkjdemo

shader practices:
http://antongerdelan.net/opengl/shaders.html
https://www.opengl.org/wiki/Shader_Compilation
https://www.opengl.org/wiki/GLSL_:_common_mistakes
http://www.lighthouse3d.com/tutorials/glsl-core-tutorial/glsl-core-tutorial-inter-shader-communication/
http://duriansoftware.com/joe/An-intro-to-modern-OpenGL.-Chapter-2.2:-Shaders.html

possibly good precedent for CMake
http://www.opengl-tutorial.org/beginners-tutorials/tutorial-1-opening-a-window/

using sRGB:
http://www.glfw.org/docs/latest/window.html : GLFW_SRGB_CAPABLE'
http://gamedevelopment.tutsplus.com/articles/gamma-correction-and-why-it-matters--gamedev-14466

*/


#if defined(__APPLE_CC__)
#define GLFW_INCLUDE_GLCOREARB
#else
#define GL_GLEXT_PROTOTYPES
#endif

#include <GLFW/glfw3.h>

#include <teem/air.h>
#include <teem/biff.h>
#include <teem/nrrd.h>
#include <teem/limn.h>

#include <Hale.h>

#include "shader.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

limnPolyData *poly;

/* Dimensions of the screen*/
int height = 480;
int width = 640;

/* The parameters for call to generate_spiral. Modified by ATB */
float lpd_alpha = 0.2;
float lpd_beta = 0.2;
float lpd_theta = 50;
float lpd_phi = 50;

#define USE_TIME false

struct render_info {
  GLuint vao = -1;
  GLuint buffs[3];
  GLuint uniforms[4];
  GLuint elms;
  ShaderProgram* shader = NULL;
} render;

struct userInterface {
  bool isDown = false;
  int mouseButton;
  //0 for all, 1 for fov, 2 for just X, 3 for just y, 4 for just z
  int mode;
  int pixScale = 1; /* scaling between locations and pixels;
                       e.g. 2 for Mac retina displays */
  double last_x;
  double last_y;
} ui;

struct camera{
  glm::vec3 center = glm::vec3(0.0f,0.0f,0.0f);
  glm::vec3 pos = glm::vec3(3.0f,0.0f,0.0f);
  glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

  float fov = 1.0f;
  float near_plane= .1f;
  float far_plane= 100.0f;

  bool fixUp = false;
} cam;

// Transformation Matricies
glm::mat4 view = glm::lookAt(cam.pos,cam.center,cam.up);
glm::mat4 proj = glm::perspective(cam.fov, ((float) width)/((float)height),
				  cam.near_plane, cam.far_plane);
glm::mat4 model = glm::mat4();

glm::vec3 light_dir = glm::normalize(glm::vec3(1.0f,1.0f,3.0f));

/* -------- Prototypes ------------------------- */
void buffer_data(limnPolyData *lpd, bool buffer_new);
limnPolyData *generate_spiral(float A, float B,unsigned int thetaRes,
			      unsigned int phiRes);

/* ---------------------Function Definitions---------------- */

void update_view(){
  view = glm::lookAt(cam.pos,cam.center,cam.up);
}

void update_proj(){
  proj = glm::perspective(cam.fov, ((float) width)/((float)height),
			  cam.near_plane, cam.far_plane);
}

void TWCB_Cam_Set(const void *value, void *clientData){
  float* val_vec = (float*)value;
  float* cd_vec = (float*)clientData;

  cd_vec[0] = val_vec[0];
  cd_vec[1] = val_vec[1];
  cd_vec[2] = val_vec[2];

  update_view();
}

void TWCB_Cam_Get(void *value, void *clientData){
  float* val_vec = (float*)value;
  float* cd_vec = (float*)clientData;

  val_vec[0] = cd_vec[0];
  val_vec[1] = cd_vec[1];
  val_vec[2] = cd_vec[2];

}

void TWCB_Spiral_Set(const void *value, void *clientData){
#if USE_TIME
  double genStart;
  double buffStart;
  double buffTime;
  double genTime;
#endif

  *(float*)clientData = *(float*)value;

#if USE_TIME
    genStart = airTime();
#endif

  limnPolyData *lpd = generate_spiral(lpd_alpha,lpd_beta,
				      lpd_theta,lpd_phi);

#if USE_TIME
     genTime = airTime();
     buffStart = airTime();
#endif

  buffer_data(lpd,true);

#if USE_TIME
    buffTime = airTime();
#endif

#if USE_TIME
    std::cout << "With Reallocation" << std::endl;
    std::cout << "Generation Time is: " << genTime-genStart << std::endl;
    std::cout << "Buffering Time is: " << buffTime-buffStart << std::endl;

    if(clientData == &lpd_beta){
      std::cout << "Without Reallocation: ";
      buffStart = airTime();
      buffer_data(lpd,false);
      buffTime = airTime();
      std::cout << buffTime-buffStart << std::endl;
    }
#endif

  limnPolyDataNix(poly);
  poly = lpd;
}

void rotate_diff(glm::vec3 diff){

  glm::mat4 inv = glm::inverse(view);
  glm::vec4 invV = inv * glm::vec4(diff,0.0);

  glm::vec3 norm = glm::cross(cam.center-cam.pos,glm::vec3(invV));
  float angle = (glm::length(diff) * 2*3.1415 ) / width;

  //Create a rotation matrix around norm.
  glm::mat4 rot = glm::rotate(glm::mat4(),angle,norm);
  cam.pos = glm::vec3(rot * glm::vec4(cam.pos,0.0));
  if(!cam.fixUp)
    cam.up = glm::vec3(rot*glm::vec4(cam.up,0.0));

  update_view();
}

void translate_diff(glm::vec3 diff){
  glm::mat4 inv = glm::inverse(view);
  glm::vec4 invV = inv * glm::vec4(diff,0.0);

  glm::mat4 trans = glm::translate(glm::mat4(),
				   glm::vec3(invV)/(float)width);
  cam.center = glm::vec3(trans*glm::vec4(cam.center,1.0));
  cam.pos = glm::vec3(trans*glm::vec4(cam.pos,1.0));
  update_view();
}

/*Generates a spiral using limnPolyDataSpiralSuperquadratic and returns
* a pointer to the newly created object.
*/
limnPolyData *generate_spiral(float A, float B,unsigned int thetaRes,
			      unsigned int phiRes){
  airArray *mop;

  char *err;
  unsigned int *res, resNum, resIdx, parmNum, parmIdx, flag;
  float *parm;
  limnPolyData *lpd;

  lpd = limnPolyDataNew();
  /* this controls which arrays of per-vertex info will be allocated
     inside the limnPolyData */
  flag = ((1 << limnPolyDataInfoRGBA)
          | (1 << limnPolyDataInfoNorm));

  /* this creates the polydata, re-using arrays where possible
     and allocating them when needed */
  if (limnPolyDataSpiralSuperquadric(lpd, flag,
				     A, B, /* alpha, beta */
				     thetaRes, phiRes)) {
    airMopAdd(mop, err = biffGetDone(LIMN), airFree, airMopAlways);
    fprintf(stderr, "trouble making polydata:\n%s", err);
    airMopError(mop);
    return NULL;
  }

  /* do something with per-vertex data to make it visually more interesting:
     the R,G,B colors increase with X, Y, and Z, respectively */
  unsigned int vertIdx;
  for (vertIdx=0; vertIdx<lpd->xyzwNum; vertIdx++) {
    float *xyzw = lpd->xyzw + 4*vertIdx;
    unsigned char *rgba = lpd->rgba + 4*vertIdx;
    rgba[0] = AIR_CAST(unsigned char, AIR_AFFINE(-1, xyzw[0], 1, 0, 255));
    rgba[1] = AIR_CAST(unsigned char, AIR_AFFINE(-1, xyzw[1], 1, 0, 255));
    rgba[2] = AIR_CAST(unsigned char, AIR_AFFINE(-1, xyzw[2], 1, 0, 255));
    rgba[3] = 255;
  }

  return lpd;
}

//Render the limnPolyData given in the global variable "poly"
void render_poly(){
  //Transformaiton Matrix Uniforms
  glUniformMatrix4fv(render.uniforms[0],1,false,glm::value_ptr(proj));
  glUniformMatrix4fv(render.uniforms[1],1,false,glm::value_ptr(view));
  glUniformMatrix4fv(render.uniforms[2],1,false,glm::value_ptr(model));

  //Light Direction Uniforms
  glUniform3fv(render.uniforms[3],1,glm::value_ptr(light_dir));

  glClear(GL_DEPTH_BUFFER_BIT);
  glClear(GL_COLOR_BUFFER_BIT);
  int offset = 0;

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render.elms);
  //Render all specified primatives
  for(int i = 0; i < poly->primNum; i++){
    GLuint prim = Hale::limnToGLPrim(poly->type[i]);

    glDrawElements( prim, poly->icnt[i],
		    GL_UNSIGNED_INT, ((void*) 0));
    offset += poly->icnt[i];
    GLuint error;
    if( (error = glGetError()) != GL_NO_ERROR)
      std::cout << "GLERROR: " << error << std::endl;
  }

}



/* Buffer the data stored in the global limPolyData Poly.
 * Buffer_new is set to true if the number or connectiviity of the vertices
 * has changed since the last buffering.
 */
void buffer_data(limnPolyData *lpd, bool buffer_new){

  //First Pass
  if(render.vao == -1){

    glGenVertexArrays(1, &(render.vao));
    glGenBuffers(3,render.buffs);

    glBindVertexArray(render.vao);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
  }

  //Verts
  glBindBuffer(GL_ARRAY_BUFFER, render.buffs[0]);
  if(buffer_new)
    glBufferData(GL_ARRAY_BUFFER, lpd->xyzwNum*sizeof(float)*4,
		 lpd->xyzw, GL_DYNAMIC_DRAW);
  else//No change in number of vertices
    glBufferSubData(GL_ARRAY_BUFFER, 0,
		    lpd->xyzwNum*sizeof(float)*4,lpd->xyzw);
  glVertexAttribPointer(0, 4, GL_FLOAT,GL_FALSE,0, 0);

  //Norms
  glBindBuffer(GL_ARRAY_BUFFER, render.buffs[1]);
  if(buffer_new)
    glBufferData(GL_ARRAY_BUFFER, lpd->normNum*sizeof(float)*3,
		 lpd->norm, GL_DYNAMIC_DRAW);
  else
    glBufferSubData(GL_ARRAY_BUFFER, 0,
		    lpd->normNum*sizeof(float)*3,lpd->norm);
  glVertexAttribPointer(1, 3, GL_FLOAT,GL_FALSE,0, 0);

  //Colors
  glBindBuffer(GL_ARRAY_BUFFER, render.buffs[2]);
  if(buffer_new)
    glBufferData(GL_ARRAY_BUFFER, lpd->rgbaNum*sizeof(char)*4,
		 lpd->rgba, GL_DYNAMIC_DRAW);
  else
    glBufferSubData(GL_ARRAY_BUFFER, 0,
		    lpd->rgbaNum*sizeof(char)*4,lpd->rgba);
  glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, 0);

  if(buffer_new){
    //Indices
    glGenBuffers(1, &(render.elms));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render.elms);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
		 lpd->indxNum * sizeof(unsigned int),
		 lpd->indx, GL_DYNAMIC_DRAW);
  }
}

/*Loads and enables the vertex and fragment shaders. Acquires uniforms
 * for the transformation matricies/
 */
void enable_shaders(const char* vshFile, const char* fshFile){
  //Initialize the shaders
  render.shader = new ShaderProgram();

  //Set up the shader
  render.shader->vertexShader(vshFile);
  render.shader->fragmentShader(fshFile);

  glBindAttribLocation(render.shader->progId,0, "position");
  glBindAttribLocation(render.shader->progId,1, "norm");
  glBindAttribLocation(render.shader->progId,2, "color");

  glLinkProgram(render.shader->progId);
  glUseProgram(render.shader->progId);

  render.uniforms[0] = render.shader->UniformLocation("proj");
  render.uniforms[1] = render.shader->UniformLocation("view");
  render.uniforms[2] = render.shader->UniformLocation("model");
  render.uniforms[3] = render.shader->UniformLocation("light_dir");

}

int
doit(float lpd_alpha, float lpd_beta, int lpd_theta, int lpd_phi) {
  static const char me[]="doit";

  Hale::Viewer viewer(width, height, "Sample");

  enable_shaders("glkdemo.vsh", "glkdemo.fsh");

  poly = generate_spiral(lpd_alpha, lpd_beta, lpd_theta, lpd_phi);
  buffer_data(poly,true);

  glBindVertexArray(render.vao);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render.elms);

  glClearColor(0.13f, 0.16f, 0.2f, 1.0f);

  glEnable(GL_DEPTH_TEST);

  while(Hale::finishingStatusNot != Hale::finishing) {
    render_poly();
    glfwWaitEvents();
    viewer.bufferSwap();
  }

  return 0;
}

int
main(int argc, const char **argv) {
  /* these variables are used for any program that conforms
     to this basic way of using hest, and being careful
     about avoiding memory leaks, via the airMop */
  const char *me;
  char *err;
  hestOpt *hopt;
  hestParm *hparm;
  airArray *mop;

  /* variables learned via hest */
  int thetaRes, phiRes;

  /* boilerplate hest code */
  me = argv[0];
  mop = airMopNew();
  hparm = hestParmNew();
  airMopAdd(mop, hparm, (airMopper)hestParmFree, airMopAlways);
  hopt = NULL;

  /* setting up the command-line options */
  hparm->noArgsIsNoProblem = AIR_TRUE;
  hestOptAdd(&hopt, "a", "alpha", airTypeFloat, 1, 1, &lpd_alpha, "0.2",
             "alpha of super-quadric");
  hestOptAdd(&hopt, "b", "beta", airTypeFloat, 1, 1, &lpd_beta, "0.2",
             "beta of super-quadric");
  hestOptAdd(&hopt, "tr", "theta res", airTypeInt, 1, 1, &thetaRes, "40",
             "theta resolution");
  hestOptAdd(&hopt, "pr", "phi res", airTypeInt, 1, 1, &phiRes, "40",
             "phi resolution");
  hestParseOrDie(hopt, argc-1, argv+1, hparm,
                 me, "demo program", AIR_TRUE, AIR_TRUE, AIR_TRUE);
  airMopAdd(mop, hopt, (airMopper)hestOptFree, airMopAlways);
  airMopAdd(mop, hopt, (airMopper)hestParseFree, airMopAlways);

  /* post-process command-line options */
  lpd_theta = thetaRes;
  lpd_phi = phiRes;

  if (Hale::init()) {
    fprintf(stderr, "%s: trouble\n", me);
    airMopError(mop);
    return 1;
  }

  if (doit(lpd_alpha, lpd_beta, lpd_theta, lpd_phi)) {
    fprintf(stderr, "%s: trouble\n", me);
    airMopError(mop);
    return 1;
  }

  Hale::done();
  airMopOkay(mop);
  return 0;
}
