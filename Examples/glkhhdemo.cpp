#include <iostream>

#define GLM_FORCE_RADIANS

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

#include <glm/gtx/string_cast.hpp>

/* Dimensions of the screen*/
double height = 480;
double width = 640;

/* The parameters for call to generate_spiral */
float lpd_alpha = 0.2;
float lpd_beta = 0.2;
float lpd_theta = 50;
float lpd_phi = 50;

// Poly data for the spiral
limnPolyData *poly;

#define USE_TIME false

struct render_info {
  GLuint vao = -1;
  GLuint buffs[3];
  GLuint uniforms[4];
  GLuint elms;
  ShaderProgram* shader = NULL;
} render;

glm::vec3 light_dir = glm::normalize(glm::vec3(1.0f,1.0f,3.0f));

/* -------- Prototypes ------------------------- */
void buffer_data(limnPolyData *lpd, bool buffer_new);
limnPolyData *generate_spiral(float A, float B,unsigned int thetaRes,
			      unsigned int phiRes);

/*
** Generates a spiral using limnPolyDataSpiralSuperquadratic and returns
** a pointer to the newly created object.
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
void render_poly(Hale::Viewer *viewer){
  static const char me[]="render_poly";
  //Transformaiton Matrix Uniforms
  /*
  fprintf(stderr, "!%s: view @ %p, proj @ %p\n", me,
          viewer->camera.viewPtr(), viewer->camera.projectPtr());
  fprintf(stderr, "!%s: view = %s\n", me,
          glm::to_string(viewer->camera.view()).c_str());
  fprintf(stderr, "!%s: proj = %s\n", me,
          glm::to_string(viewer->camera.project()).c_str());
  */
  glUniformMatrix4fv(render.uniforms[0],1,false,viewer->camera.projectPtr());
  glUniformMatrix4fv(render.uniforms[1],1,false,viewer->camera.viewPtr());

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
  render.uniforms[3] = render.shader->UniformLocation("light_dir");
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
    fprintf(stderr, "%s: Hale::init() failed\n", me);
    airMopError(mop);
    return 1;
  }

  Hale::Viewer *viewer = new Hale::Viewer(width, height, "Bingo");
  viewer->camera.init(glm::vec3(8.0f,0.0f,0.0f),
                      glm::vec3(0.0f,0.0f,0.0f),
                      glm::vec3(0.0f, 1.0f, 0.0f),
                      25, 1.33333, -2, 2, false);

  enable_shaders("glkhhdemo.vsh", "glkdemo.fsh");

  poly = generate_spiral(lpd_alpha, lpd_beta, lpd_theta, lpd_phi);
  buffer_data(poly,true);

  glBindVertexArray(render.vao);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render.elms);

  glClearColor(0.13f, 0.16f, 0.2f, 1.0f);
  glEnable(GL_DEPTH_TEST);

  while(!Hale::finishing){
    render_poly(viewer);
    glfwWaitEvents();
    viewer->bufferSwap();
  }

  /* clean exit; all okay */
  delete viewer;
  airMopOkay(mop);
  return 0;
}
