#include <iostream>

#include <AntTweakBar.h>

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

#include "shader.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

/* Dimensions of the AntTweakBar panel*/
int atbWidth = 200;
int atbHeight = 300;

GLFWwindow *theWindow = NULL;

/* Dimensions of the screen*/
double height = 480;
double width = 640;

/* The parameters for call to generate_spiral. Modified by ATB */
float lpd_alpha = 0.2;
float lpd_beta = 0.2;
float lpd_theta = 50;
float lpd_phi = 50;

// Poly data for the spiral
limnPolyData *poly;
// The ATB panel
TwBar *bar;

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

void TWCB_Spiral_Get(void *value, void *clientData){
  *(float *)value = *(float*)clientData;
}

void mouseButtonCB(GLFWwindow* w, int button,
		   int action, int mods){

  glfwGetCursorPos (w, &(ui.last_x), &(ui.last_y));

  //User is not currently rotating or zooming.
  if (ui.isDown == false){
    int twret;
    //Pass the event to ATB
    twret = TwEventMouseButtonGLFW( button , action );
    if (twret) {
      /* ATB has handled it */
      return;
    }
  }

  //Else, set up the mode for rotating/zooming
  if (action == GLFW_PRESS) {
    ui.isDown = true;
    ui.mouseButton = button;
    /* hack: if the "super" modifier key has been pressed,
       which is Command on Macs, then we record this as a
       second-button press, in keeping with the Mac idiom
       of command-click being like right-click */
    if (GLFW_MOD_SUPER & mods) {
      ui.mouseButton = GLFW_MOUSE_BUTTON_2;
    }
  } else if (action == GLFW_RELEASE){
    ui.isDown = false;
    ui.mouseButton = -1; /* not a valid button */
  }

  if(ui.last_x <= width*.1)
    ui.mode = 1;
  else if(ui.last_x >= width*.9)
    ui.mode = 3;
  else if(ui.last_y <= height*.1)
    ui.mode = 4;
  else if(ui.last_y >= height*.9)
    ui.mode = 2;
  else
    ui.mode = 0;

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

void mousePosCB(GLFWwindow* w, double x, double y){

  /*
  ** If zooming/rotating is not occuring, then there's nothing for us
  ** to do except let ATB know about it. The return of the ATB call
  ** will say whether ATB handled it, but we don't need that info.
  */
  if (!ui.isDown) {
    TwEventMousePosGLFW( x*ui.pixScale, y*ui.pixScale );
    return;
  }

  float x_diff = ui.last_x - x;
  float y_diff = ui.last_y - y;

  //Standard (middle of the screen mode)
  if(ui.mode == 0){

    //Rotate
    if(ui.mouseButton == GLFW_MOUSE_BUTTON_1){
      rotate_diff(glm::vec3(-x_diff,y_diff,0.0f));
    }

    //Translate
    else if(ui.mouseButton == GLFW_MOUSE_BUTTON_2){
      translate_diff(glm::vec3(-x_diff,y_diff,0.0f));
    }

  }

  //Zooming Mode
  else if(ui.mode == 1){

    //FOV zoom
    if(ui.mouseButton == GLFW_MOUSE_BUTTON_1){
      cam.fov += (-y_diff / height);
      update_proj();
    }
    else if(ui.mouseButton == GLFW_MOUSE_BUTTON_2)
      std::cout << "here\n";
      translate_diff(glm::vec3(0.0f,0.0f,y_diff));

  }

  //modify u only
   else if(ui.mode == 4){
     if(ui.mouseButton == GLFW_MOUSE_BUTTON_1){
       if(x_diff != 0.0) //Can't rotate by 0
	 rotate_diff(glm::vec3(-x_diff,0.0f,0.0f));
     }
     else if(ui.mouseButton == GLFW_MOUSE_BUTTON_2)
       translate_diff(glm::vec3(-x_diff,0.0f,0.0f));
   }

  //modify v only
   else if(ui.mode == 3){
     if(ui.mouseButton == GLFW_MOUSE_BUTTON_1){
       if(y_diff != 0.0)
	 rotate_diff(glm::vec3(0.0f,y_diff,0.0f));
     }
     else if(ui.mouseButton == GLFW_MOUSE_BUTTON_2)
       translate_diff(glm::vec3(0.0f,y_diff,0.0f));
   }

  //modify w only
   else if(ui.mode == 2){
     if(ui.mouseButton == GLFW_MOUSE_BUTTON_1){
       float angle = (x_diff*3.1415*2) / width;
       glm::mat4 rot = glm::rotate(glm::mat4(),angle,
				   cam.pos-cam.center);
       cam.pos = glm::vec3(rot * glm::vec4(cam.pos,0.0));
       if(!cam.fixUp)
	 cam.up = glm::vec3(rot*glm::vec4(cam.up,0.0));
       update_view();
     }

   }

  ui.last_x = x;
  ui.last_y = y;

}

void screenSizeCB(GLFWwindow* win, int w, int h){
  int buffSx, buffSy;
  width = w;
  height = h;

  glfwGetFramebufferSize(win, &buffSx, &buffSy);
  /* fprintf(stderr, "%s: win %d %d, buff %d %d\n", "screenSizeCB",
     w, h, buffSx, buffSy); */
  //glViewport(0,0,width,height);
  glViewport(0,0,buffSx, buffSy);

  //Update the projection matrix to reflect the new aspect ratio
  update_proj();

}

void keyFunCB( GLFWwindow* window,int key,int scancode,int action,int mods)
{
  //TODO: add a reset key

  TwEventKeyGLFW( key , action );
  TwEventCharGLFW( key  , action );
}

void mouseScrollCB(  GLFWwindow* window, double x , double y )
{
  TwEventMouseWheelGLFW( (int)y );
}

/* Converts a teem enum to an openGL enum */
GLuint get_prim(unsigned char type){
  GLuint ret;
  switch(type){
  case limnPrimitiveUnknown:
  case limnPrimitiveNoop:
  case limnPrimitiveLast:
    ret = 0;
    break;
  case limnPrimitiveTriangles:
    ret = GL_TRIANGLES;
    break;
  case limnPrimitiveTriangleStrip:
    ret = GL_TRIANGLE_STRIP;
    break;
  case limnPrimitiveTriangleFan:
    ret = GL_TRIANGLE_FAN;
    break;
  case limnPrimitiveQuads:
    ret = GL_QUADS;
    break;
  case limnPrimitiveLineStrip:
    ret = GL_LINE_STRIP;
    break;
  case limnPrimitiveLines:
    ret = GL_LINES;
    break;
  }
  return ret;
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
    GLuint prim = get_prim(poly->type[i]);

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

//Initialize the ATB pannel.
void init_ATB(){
  int buffSx, buffSy, winSx, winSy;
  std::string s;

  glfwGetWindowSize(theWindow, &winSx, &winSy);
  glfwGetFramebufferSize(theWindow, &buffSx, &buffSy);
  ui.pixScale = (buffSx <= winSx) ? 1 : buffSx/winSx;
  fprintf(stderr, "init_ATB: hello: size win (%d %d); buff (%d %d) scale %d\n",
          winSx, winSy, buffSx, buffSy, ui.pixScale);
  if (ui.pixScale > 1) {
    s = "GLOBAL fontscaling=" + std::to_string(ui.pixScale) ;
    TwDefine(s.c_str());
  }

  TwInit(TW_OPENGL_CORE, NULL);
  TwWindowSize(buffSx, buffSy);

  bar = TwNewBar("lpdTweak");

  s = ("lpdTweak size='" + std::to_string(ui.pixScale*atbWidth)
       + " " + std::to_string(ui.pixScale*atbHeight) + "'");
  TwDefine(s.c_str());

  TwDefine("lpdTweak resizable=true");

  //position=top left corner
  s = "lpdTweak position='0 0'";
  TwDefine(s.c_str());

  TwAddVarCB(bar, "ALPHA", TW_TYPE_FLOAT, TWCB_Spiral_Set, TWCB_Spiral_Get,
	     &lpd_alpha, "min=0.0 step=.01 label=Alpha");

  TwAddVarCB(bar, "BETA", TW_TYPE_FLOAT, TWCB_Spiral_Set, TWCB_Spiral_Get,
	     &lpd_beta, "min=0.0 step=.01 label=Beta");

  TwAddVarCB(bar, "THETA", TW_TYPE_FLOAT, TWCB_Spiral_Set, TWCB_Spiral_Get,
	     &lpd_theta, "min=1.0 step=1 label=Theta");

  TwAddVarCB(bar, "PHI", TW_TYPE_FLOAT, TWCB_Spiral_Set, TWCB_Spiral_Get,
	     &lpd_phi, "min=1.0 step=1 label=Phi");

  TwAddVarRW(bar, "FixUP", TW_TYPE_BOOLCPP, &(cam.fixUp),
	     " label='Fix Up Vector' group='Camera'");

  TwAddVarCB(bar, "CamCenter", TW_TYPE_DIR3F,TWCB_Cam_Set, TWCB_Cam_Get,
	     glm::value_ptr(cam.center), "label='Center' group='Camera'");

  TwAddVarCB(bar, "CamPos", TW_TYPE_DIR3F, TWCB_Cam_Set, TWCB_Cam_Get,
	     glm::value_ptr(cam.pos), "label='Position' group='Camera'");

  TwAddVarCB(bar, "CamUp", TW_TYPE_DIR3F, TWCB_Cam_Set, TWCB_Cam_Get,
	     glm::value_ptr(cam.up),"label='Up Vector' group='Camera'");

  TwAddVarRW(bar, "LightDir", TW_TYPE_DIR3F,
	     glm::value_ptr(light_dir), "label='Light Direction'");

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
  hestOptAdd(&hopt, "tr", "theta res", airTypeInt, 1, 1, &thetaRes, "10",
             "theta resolution");
  hestOptAdd(&hopt, "pr", "phi res", airTypeInt, 1, 1, &phiRes, "10",
             "phi resolution");
  hestParseOrDie(hopt, argc-1, argv+1, hparm,
                 me, "demo program", AIR_TRUE, AIR_TRUE, AIR_TRUE);
  airMopAdd(mop, hopt, (airMopper)hestOptFree, airMopAlways);
  airMopAdd(mop, hopt, (airMopper)hestParseFree, airMopAlways);

  /* post-process command-line options */
  lpd_theta = thetaRes;
  lpd_phi = phiRes;

  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // Use OpenGL Core v3.2
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  theWindow = glfwCreateWindow(width, height, "Sample", NULL, NULL);
  if (!theWindow) {
    glfwTerminate();
    exit(EXIT_FAILURE);
  }

  glfwMakeContextCurrent(theWindow);

  init_ATB();

  enable_shaders("glkdemo.vsh", "glkdemo.fsh");

  poly = generate_spiral(lpd_alpha, lpd_beta, lpd_theta, lpd_phi);
  buffer_data(poly,true);

  glBindVertexArray(render.vao);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render.elms);

  glClearColor(0.13f, 0.16f, 0.2f, 1.0f);

  glfwSetCursorPosCallback(theWindow, mousePosCB);
  glfwSetMouseButtonCallback(theWindow,mouseButtonCB);
  glfwSetWindowSizeCallback(theWindow,screenSizeCB);
  glfwSetScrollCallback( theWindow , mouseScrollCB );
  glfwSetKeyCallback(theWindow , keyFunCB);

  glEnable(GL_DEPTH_TEST);

  while(!glfwWindowShouldClose(theWindow)){
    render_poly();
    TwDraw();
    glfwWaitEvents();
    glfwSwapBuffers(theWindow);

  }

  /* clean exit; all okay */
  airMopOkay(mop);
  return 0;
}
