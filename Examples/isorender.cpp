#include <AntTweakBar.h>

#define GLM_FORCE_RADIANS
#define GLFW_GLEXT_PROTOTYPES
#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>

#if defined(__APPLE_CC__)
#include <OpenGL/glext.h>
#else
#  include <GL/glext.h>
#endif

#include <teem/air.h>
#include <teem/biff.h>
#include <teem/nrrd.h>
#include <teem/limn.h>
#include <teem/seek.h>
#include "shader.h"
#include <glm/glm.hpp>
#include "glm/gtc/matrix_transform.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <iostream> 

/*Dimenstions of the AntTweakBar pannel*/
#define ATB_WIDTH 200
#define ATB_HEIGHT 200

/*Dimensions of the screen*/
double height = 480;
double width = 640;


//The Current Isovalue
float isovalue = 0.5;
//The seek data
Nrrd* seek_data;
//The seek context
seekContext *sctx;
//Poly data for the sample
limnPolyData *poly;
//The ATB pannel
TwBar *bar;

#define USE_TIME false

typedef enum {UI_POS_LEFT, UI_POS_TOP, UI_POS_RIGHT,
	      UI_POS_BOTTOM, UI_POS_CENTER } ui_position; 

struct render_info{
  GLuint vao = -1;
  GLuint buffs[3];
  GLuint uniforms[7];
  GLuint elms;
  ShaderProgram* shader = NULL;

  GLuint fbo_ext = -1;
  GLuint rbo_ext_color;
  GLuint rbo_ext_depth;

} render;

/*A structure which manages the state of the ui.
 *Used for interaction with the camera*/
struct ui_pos{
  bool isDown = false; //Mouse is down.
  GLuint mouseButton; //Which mouse button is down
  //0 for all, 1 for fov, 2 for just X, 3 for just y, 4 for just z
  ui_position mode;

  bool shift_click = false;

  //The last screen space coordinates that a ui event occured at
  double last_x;
  double last_y;
} ui;

struct camera{
  //Location at which the camera is pointed
  glm::vec3 center = glm::vec3(0.0f,0.0f,0.0f);
  //The loaction of the eyepoint of the camera
  glm::vec3 pos = glm::vec3(3.0f,0.0f,0.0f);
  //Up vector
  glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

  float fov = 1.0f;
  float near_plane= 1.0f;
  float far_plane= 1000.0f;

  bool fixUp = false;
} cam;

//Transformation Matricies
glm::mat4 view = glm::lookAt(cam.pos,cam.center,cam.up);
glm::mat4 proj = glm::perspective(cam.fov, ((float) width)/((float)height),
				  cam.near_plane, cam.far_plane);
glm::mat4 model = glm::mat4();
glm::vec3 light_dir = cam.pos - cam.center;

//The ray cast from the eyepoint throught a specific point in the world
//The default follows the direction of the camera
glm::vec3 ray = cam.center-cam.pos;
glm::vec3 origin = cam.center;

/*A structure which holds data for AntTweakBar to modify */
struct ATB_Vals{
  std::string fileOut = "out";//File name to be saved to for calls to take screenshot
  bool saveDepthMap = false;
} barVals;

/* -------- Prototypes -------------------------*/
void buffer_data(limnPolyData *lpd, bool buffer_new);
limnPolyData *generate_sample(float index);
void render_poly();
void TWCB_take_screenshot(void* clientData);
void bind_external_buffer();

/***************Function Defentions****************/

/*-----Functions Which (only) update global variables------*/

/* Update the view Matrix using the currently saved camera parameters*/
void update_view(){
  view = glm::lookAt(cam.pos,cam.center,cam.up);

  light_dir = cam.pos - cam.center;
} 

/*Update the projeciton Matrix using the currently save camera parameters*/
void update_proj(){
  proj = glm::perspective(cam.fov, ((float) width)/((float)height),
			  cam.near_plane, cam.far_plane);

}

static void error_callback(int error, const char* description){
  fputs(description, stderr);
}


/*-----Callbacks for AntTweakBar Events---------*/

/* Takes a screenshot of the current image/depthbuffer and saves it as a png.
 * Uses the name given by barVals.fileOut 
 */
void TWCB_take_screenshot(void* clientData){
 
  bind_external_buffer();

  glReadBuffer(GL_COLOR_ATTACHMENT0); 
 
  /*Render the image into the buffer*/
  render_poly();

  //The rendered image is read into ss
  GLvoid *ss = malloc(sizeof(unsigned char)*3*width*height);
  glReadPixels(0,0,width,height,GL_RGB,
	       GL_UNSIGNED_BYTE,ss);

  //Used to save the rendered image
  Nrrd *nflip = nrrdNew();
  Nrrd *nout = nrrdNew();

  char* err;
    //Saves the contents of the buffer as a png image
    //Currently upsidedown

if (nrrdWrap_va(nflip, ss, nrrdTypeUChar, 3,
		(size_t)3, (size_t)width, (size_t)height)
    || nrrdFlip(nout,nflip, 2) ||  nrrdSave((barVals.fileOut + std::string(".png")).c_str(), nout, NULL)) {
      err = biffGetDone(NRRD);
      fprintf(stderr, "trouble wrapping image:\n%s", err);
      free(err);
      return;
    }



 if(barVals.saveDepthMap){
   nrrdNuke(nout);
   nrrdNuke(nflip);
   nout = nrrdNew();
   nflip = nrrdNew();

   ss = malloc(sizeof(float)*width*height);
   glReadPixels(0,0,width,height,GL_DEPTH_COMPONENT, GL_FLOAT,ss);
   if(nrrdWrap_va(nflip,ss,nrrdTypeFloat, 3,(size_t)1, 
		  (size_t)width,(size_t)height) ||
      nrrdFlip(nout,nflip,2) ||
      nrrdSave((barVals.fileOut+std::string("Depth.png")).c_str(),nout,NULL)){
     err = biffGetDone(NRRD);
     fprintf(stderr, "trouble wrapping/flippings/saving depthmap: %s\n",err);
     free(err);
     return;
   }
 }

 nrrdNuke(nout);
 nrrdNuke(nflip);

  //rebind the frambuffer to the screen
 glBindFramebuffer (GL_FRAMEBUFFER, 0);

}

void TW_CALL TWCB_Set_String(const void *value, void *clientData)
{
  //Some crazy memory managment.
  //Taken from an example in the TW documentation
  const std::string *srcPtr = static_cast<const std::string *>(value);
  *((std::string*)clientData) = *srcPtr;
}

void TW_CALL TWCB_Get_String(void *value, void * clientData)
{
  //See the same example
  std::string *destPtr = static_cast<std::string *>(value);
  TwCopyStdStringToLibrary(*destPtr, *((std::string*)clientData));
}

/* Called when the ATB changes the values associated with the camera*/
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

/*Update the isovalue data, and then generate and buffer a new mesh for this
 *isovalue.
 */
void TWCB_Isovalue_Set(const void *value, void *clientData){

  *(float*)clientData = *(float*)value;
  
  limnPolyData *lpd = generate_sample(isovalue);
  buffer_data(lpd,true);

  free(poly);
  poly = lpd;

}

void TWCB_Isovalue_Get(void *value, void *clientData){
  *(float *)value = *(float*)clientData;
}

/*Print the camera parameters to the commandline*/
void TWCB_Print_Camera(void* clientData){
  std::cout << "Eye Point: " << cam.pos.x << " " << cam.pos.y << " " << cam.pos.z << std::endl;
  std::cout << "Lookat Point: " << cam.center.x << " " << cam.center.y <<  " " << cam.center.z << std::endl;
  std::cout << "Up Vector: " << cam.up.x << " " << cam.up.y << " " << cam.up.z << std::endl;

  std::cout << "Near Plane: " << cam.near_plane << " Far Plane: " << cam.far_plane << " FOV: " << cam.fov << std::endl;

  std::cout << "Width: " << width << " Height: " << height;

}

/*--------Functions for Primative Picking-------------*/

/* Performs a rendering pass to get the depth map for the current image.
 * Then returns the value at x,y in the map
 */
float getDepthAt(int x, int y){
  float z;

  bind_external_buffer();
  glReadBuffer(GL_COLOR_ATTACHMENT0);   
  render_poly();

  glReadPixels(x,y,1,1,GL_DEPTH_COMPONENT, GL_FLOAT,&z);

  glBindFramebuffer (GL_FRAMEBUFFER, 0);
  return z;
  
}

/* castas a ray in world space from cam.pos through the point x,y
 * and then updates 'ray' with the result.
 * x and y are described in screenspace.
 */
void cast_ray(float x_screen,float y_screen){
  float z  = getDepthAt(x_screen,y_screen);

  glm::vec4 viewport = glm::vec4(0,0,width,height);
  glm::vec3 wincoord = glm::vec3(x_screen,height-y_screen-1,z);
  ray = glm::unProject(wincoord,view,proj,viewport);

}

/*-----------Helper founctions for camera control-------------*/

/* Rotate the up and pos vectors based on the given diff vector and then 
 * updates the view matrix.
 * This vector represents a direction vector in screen space coordinates.
 * Its magnitude is used tot determine the angle of the rotation.
 */
void rotate_diff(glm::vec3 diff){

  glm::mat4 inv = glm::inverse(view);
  glm::vec4 invV = inv * glm::vec4(diff,0.0);

  glm::vec3 norm = glm::cross(cam.center-cam.pos,glm::vec3(invV));
  float angle = (glm::length(diff) * 2*3.1415 ) / width;
  
  //Create a rotation matrix around norm.
  glm::mat4 rot = glm::rotate(glm::mat4(),angle,glm::normalize(norm));
  //Align the origin with the look-at point of the camera.
  glm::mat4 trans = glm::translate(glm::mat4(), -cam.center);
  cam.pos = glm::vec3(glm::inverse(trans)*rot*trans*glm::vec4(cam.pos,1.0));

  if(!cam.fixUp)
    cam.up = glm::vec3(rot*glm::vec4(cam.up,0.0));

  update_view();
}

/*Translate the pos vector in a direction given by the diff vector.
 *The diff vector is a displacement given in scrrenspace coordinates
 *and the amount displaced is proportional to its magnitude
 */
void translate_diff(glm::vec3 diff){
  glm::mat4 inv = glm::inverse(view);
  glm::vec4 invV = inv * glm::vec4(diff,0.0);
  
  glm::mat4 trans = glm::translate(glm::mat4(),
				   glm::vec3(invV));
  cam.center = glm::vec3(trans*glm::vec4(cam.center,1.0));
  cam.pos = glm::vec3(trans*glm::vec4(cam.pos,1.0));
  update_view();
}

/*----------Functions Which contorl the camera---------------*/

/*Called when the mouse button is clicked*/
void mouseButtonCB(GLFWwindow* w, int button, 
		   int action, int mods){


  glfwGetCursorPos (w, &(ui.last_x), &(ui.last_y));
  ui.mouseButton = button;

  //User is not currently rotating or zooming.
  if(ui.isDown == false){
    //Pass the event to ATB
    TwEventMouseButtonGLFW( button , action );
    
    //Get the location of the ATB window
    int pos[2];
    int tw_size[2];
    TwGetParam(bar, NULL, "position", TW_PARAM_INT32, 2, pos);
    TwGetParam(bar,NULL, "size", TW_PARAM_INT32, 2, tw_size);

    /*If the event is on the ATB pannel, then return
     *This prevents the camera from zooming/rotating while trying to click on
     *the ATB user interface
     */
    if(pos[0] <= ui.last_x && pos[0] + tw_size[0] >= ui.last_x && 
       pos[1] <= ui.last_y && pos[1] + tw_size[1] >= ui.last_y)
      return;
  }

  /*On shift-click, cast a ray through the position that was clicked*/
  if(ui.shift_click && action == GLFW_PRESS){
    cast_ray(ui.last_x,ui.last_y);
    return;
  }

  //Else, set up the mode for rotating/zooming
  if(action == GLFW_PRESS){
    ui.isDown = true;
  }
  else if(action == GLFW_RELEASE){
    ui.isDown = false;
  }

  if(ui.last_x <= width*.1)
    ui.mode = UI_POS_LEFT;
  else if(ui.last_x >= width*.9)
    ui.mode = UI_POS_RIGHT;
  else if(ui.last_y <= height*.1)
    ui.mode = UI_POS_BOTTOM;
  else if(ui.last_y >= height*.9)
    ui.mode = UI_POS_TOP;
  else
    ui.mode = UI_POS_CENTER;

}


/*Called when the mouse moves*/
void mousePosCB(GLFWwindow* w, double x, double y){

  //If zooming/rotating is not occuring, just pass to ATB
  if(!ui.isDown){
    TwEventMousePosGLFW( (int)x, (int)y );
    return;
  }

  float x_diff = ui.last_x - x;
  float y_diff = ui.last_y - y;
  
  //Standard (middle of the screen mode)
  if(ui.mode == UI_POS_CENTER){

    //Left-Click (Rotate)
    if(ui.mouseButton == GLFW_MOUSE_BUTTON_1){
      rotate_diff(glm::vec3(-x_diff,y_diff,0.0f));
    }

    //Right-Click (Translate)
    else if(ui.mouseButton == GLFW_MOUSE_BUTTON_2){
      translate_diff(glm::vec3(-x_diff,y_diff,0.0f));
    }

  }
  
  //Zooming Mode
  else if(ui.mode == UI_POS_LEFT){

    //FOV zoom
    if(ui.mouseButton == GLFW_MOUSE_BUTTON_1){
      cam.fov += (-y_diff / height);
      update_proj();
    }
    else if(ui.mouseButton == GLFW_MOUSE_BUTTON_2)
      translate_diff(glm::vec3(0.0f,0.0f,y_diff));

  }

  //modify u only
   else if(ui.mode == UI_POS_TOP){
     if(ui.mouseButton == GLFW_MOUSE_BUTTON_1){
       if(x_diff != 0.0) //Can't rotate by 0
	 rotate_diff(glm::vec3(-x_diff,0.0f,0.0f));
     }
     else if(ui.mouseButton == GLFW_MOUSE_BUTTON_2)
       translate_diff(glm::vec3(-x_diff,0.0f,0.0f));
   }

  //modify v only
   else if(ui.mode == UI_POS_RIGHT){
     if(ui.mouseButton == GLFW_MOUSE_BUTTON_1){
       if(y_diff != 0.0)
	 rotate_diff(glm::vec3(0.0f,y_diff,0.0f));
     }
     else if(ui.mouseButton == GLFW_MOUSE_BUTTON_2)
       translate_diff(glm::vec3(0.0f,y_diff,0.0f));
   }

  //modify w only
   else if(ui.mode == UI_POS_BOTTOM){
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

/*---------Other GLFW Event Callbacks----------*/

void screenSizeCB(GLFWwindow* win, int w, int h){
  width = w;
  height = h;

  glViewport(0,0,width,height);

  //Update the projection matrix to reflect the new aspect ratio
  update_proj();

}

//Register if certain keys are pressed or released
void keyFunCB( GLFWwindow* window,int key,int scancode,int action,int mods)
{
  //TODO: add a reset key
  bool shift = (key == GLFW_KEY_LEFT_SHIFT) || (key == GLFW_KEY_RIGHT_SHIFT);
  if(shift && action == GLFW_PRESS){
    ui.shift_click = true;
  }
  else if(shift && action == GLFW_RELEASE){
    ui.shift_click = false;
  }

  //Pass to ATB
  TwEventKeyGLFW( key , action );
  TwEventCharGLFW( key  , action );
}

void mouseScrollCB(  GLFWwindow* window, double x , double y )
{
  TwEventMouseWheelGLFW( (int)y );
}

/*-------Interactions with OpenGL to set up and render the scene-------*/

/*Sets up (if necessary) and then binds an rgb and depth buffer
 *to the rbo render_info.rbo_ext.
 *
 *External buffers are bound to perform a rendering pass which is not written
 *to the screen (i.e. for saving an image or acquiring data from the render itself)
 */
void bind_external_buffer(){
  /*first time calling bind_external_buffer. Generate the rbo's/fbo
   *and then allocate space for them.
   * Boilerplate OpenGL
   */
  if(render.fbo_ext == -1){
    glGenRenderbuffers(1, &(render.rbo_ext_color));

    glBindRenderbuffer(GL_RENDERBUFFER,render.rbo_ext_color);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB, 
			  width,height);
    
    glGenRenderbuffers(1, &(render.rbo_ext_depth));
    glBindRenderbuffer(GL_RENDERBUFFER,render.rbo_ext_depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
			  width,height);
    
    glGenFramebuffers(1, &(render.fbo_ext));
    glBindFramebuffer(GL_FRAMEBUFFER,render.fbo_ext);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, 
			      GL_COLOR_ATTACHMENT0,
			      GL_RENDERBUFFER,
			      render.rbo_ext_color);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, 
			      GL_DEPTH_ATTACHMENT,
			      GL_RENDERBUFFER,
			      render.rbo_ext_depth);
  }

  glBindFramebuffer(GL_FRAMEBUFFER,render.fbo_ext);
  
}

/*------- Functions for setting up and using TEEM-----------*/

/*Coverts the TEEM enum for primiative types to OGL enum*/
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

//Initializes the seek context
void init_seek(){
  sctx = seekContextNew();
  seekVerboseSet(sctx, 0);
  seekNormalsFindSet(sctx, AIR_TRUE);

  if (seekDataSet(sctx, seek_data, NULL, 0)
      || seekTypeSet(sctx, seekTypeIsocontour)) {
    char* err = biffGetDone(SEEK);
    fprintf(stderr, "trouble with set-up:\n%s", err);
    free(err);
  }

}

/*Parse arguments using air*/
void parse_args(int argc,const char**argv){
  const char *me;
  hestOpt *hopt;
  hestParm *hparm;
  airArray *mop;

  me = argv[0];
  mop = airMopNew();
  hparm = hestParmNew();
  hopt = NULL;
  airMopAdd(mop, hparm, (airMopper)hestParmFree, airMopAlways);
  hestOptAdd(&hopt, "i", "nin", airTypeOther, 1, 1, &seek_data, 
	     NULL,"volume from which to extract isosurfaces", NULL,
	     NULL,nrrdHestNrrd);
  hestParseOrDie(hopt, argc-1, argv+1, hparm,
                 me,NULL, AIR_TRUE, AIR_TRUE, AIR_TRUE);
  airMopAdd(mop, hopt, (airMopper)hestOptFree, airMopAlways);
  airMopAdd(mop, hopt, (airMopper)hestParseFree, airMopAlways);

}

/*Generates a spiral using limnPolyDataSpiralSuperquadratic and returns
* a pointer to the newly created object.
*/
limnPolyData *generate_sample(float index){

  limnPolyData* lpd = limnPolyDataNew();
  if (seekIsovalueSet(sctx, index)
      || seekUpdate(sctx)
      || seekExtract(sctx, lpd)) {
    char* err = biffGetDone(SEEK);
    fprintf(stderr, "trouble getting isovalue:\n%s", err);
    free(err);
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

  glUniform3fv(render.uniforms[4],1,glm::value_ptr(origin));

  glUniform3fv(render.uniforms[5],1,glm::value_ptr(ray));

  //bool paint
  glUniform1i(render.uniforms[6],(GLint)1.0);


  glClear(GL_DEPTH_BUFFER_BIT);
  glClear(GL_COLOR_BUFFER_BIT);
  int offset = 0;
  //Render all specified primatives

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render.elms);
  glBindVertexArray(render.vao);

  for(int i = 0; i < poly->primNum; i++){
    GLuint prim = get_prim(poly->type[i]);
    glDrawElements( prim, poly->icnt[i], 
		    GL_UNSIGNED_INT,  0);
    offset += poly->icnt[i];
    GLuint error;
    if( (error = glGetError()) != GL_NO_ERROR)
      std::cout << "ERROR: " << error << std::endl;
  }
  
}

/*--------Primary calls to set-up and render the scene--------*/

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
    if(lpd->norm != NULL)
      glEnableVertexAttribArray(1);
    if(lpd->rgba != NULL)
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

  if(lpd->norm != NULL){
    glBindBuffer(GL_ARRAY_BUFFER, render.buffs[1]);
    if(buffer_new)
      glBufferData(GL_ARRAY_BUFFER, lpd->normNum*sizeof(float)*3,
		   lpd->norm, GL_DYNAMIC_DRAW);
    else
      glBufferSubData(GL_ARRAY_BUFFER, 0,
		      lpd->normNum*sizeof(float)*3,lpd->norm);
    glVertexAttribPointer(1, 3, GL_FLOAT,GL_FALSE,0, 0);
  }

  //Colors
  if(lpd->rgba != NULL){
    glBindBuffer(GL_ARRAY_BUFFER, render.buffs[2]);
    if(buffer_new)
      glBufferData(GL_ARRAY_BUFFER, lpd->rgbaNum*sizeof(char)*4,
		   lpd->rgba, GL_DYNAMIC_DRAW);
    else
      glBufferSubData(GL_ARRAY_BUFFER, 0, 
		      lpd->rgbaNum*sizeof(char)*4,lpd->rgba);
    glVertexAttribPointer(2, 4, GL_BYTE,GL_FALSE,0, 0);
  }

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
void enable_shaders(const char* vshFile, 
		    const char* fshFile,
		    const char* gshFile){
  //Initialize the shaders
  render.shader = new ShaderProgram(); 
  
  //Set up the shader
  render.shader->vertexShader(vshFile);
  render.shader->fragmentShader(fshFile);
  if(gshFile != NULL)
    render.shader->geometryShader(gshFile);
  
  glBindAttribLocation(render.shader->progId,0, "position");
  glBindAttribLocation(render.shader->progId,1, "norm");
  glBindAttribLocation(render.shader->progId,2, "color");
  
  glLinkProgram(render.shader->progId);
  glUseProgram(render.shader->progId);
  
  render.uniforms[0] = render.shader->UniformLocation("proj");
  render.uniforms[1] = render.shader->UniformLocation("view");
  render.uniforms[2] = render.shader->UniformLocation("model");
  render.uniforms[3] = render.shader->UniformLocation("light_dir");

  render.uniforms[4] = render.shader->UniformLocation("origin");
  render.uniforms[5] = render.shader->UniformLocation("ray");
  render.uniforms[6] = render.shader->UniformLocation("pixel_trace_on");
  
}

//Initialize the ATB pannel.
void init_ATB(){
  TwInit(TW_OPENGL_CORE, NULL);

  TwWindowSize(width,height);

  bar = TwNewBar("lpdTweak");

  //size='ATB_WIDTH ATB_HEIGHT'
  std::string s = std::string("lpdTweak size='") + std::to_string(ATB_WIDTH) + 
    std::string(" ") + std::to_string(ATB_HEIGHT) + std::string("'");
  TwDefine(s.c_str());

  TwDefine(" lpdTweak resizable=true ");

  //position=top-right corner
  s = std::string("lpdTweak position='") + 
    std::to_string((int)(width - ATB_WIDTH)) + std::string(" 0'");
  TwDefine(s.c_str());

  TwAddVarCB(bar, "ISOVALUE", TW_TYPE_FLOAT, TWCB_Isovalue_Set, 
	     TWCB_Isovalue_Get, &isovalue, 
	     "min=0.0 step=.1 label=Isovalue");

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

  TwAddButton(bar, "PrintCamera", TWCB_Print_Camera, NULL, 
	      "label='Output Camera Inforamation'");

  TwAddButton(bar, "ScreenShot", TWCB_take_screenshot, NULL,
	      "label='Screenshot' group='Screenshot'");

  TwAddVarRW(bar, "SaveDepthMap", TW_TYPE_BOOLCPP, &(barVals.saveDepthMap),
	     "label='Save Depth Map?' group='Screenshot'");

  TwAddVarCB(bar, "ScreenshotName", TW_TYPE_STDSTRING, 
	     TWCB_Set_String, TWCB_Get_String, &(barVals.fileOut), 
	     "label='Screenshot Name' group='Screenshot'");

}

int main(int argc, const char **argv) {

  glfwSetErrorCallback(error_callback);

  if(!glfwInit()){
    std::cout << "failed to initialize glfw\n";
    exit(EXIT_FAILURE);
  }
 
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // Use OpenGL Core v3.2
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  

  GLFWwindow *window = glfwCreateWindow(640,480, "Sample", NULL, NULL);
  if(!window){
    std::cout << "failed to create context\n";
    glfwTerminate();
    exit(EXIT_FAILURE);
  }

  glfwMakeContextCurrent(window);
  

  init_ATB();

  parse_args(argc,argv);
  init_seek();

  enable_shaders("isorender.vsh","isorender.fsh","isorender.gsh");


  poly = generate_sample(isovalue);
  buffer_data(poly,true);

  glBindVertexArray(render.vao);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render.elms);

  glClearColor(0.5f, 0.5f, 0.5f, 1.0f);

  glfwSetCursorPosCallback(window, mousePosCB);
  glfwSetMouseButtonCallback(window,mouseButtonCB);
  glfwSetWindowSizeCallback(window,screenSizeCB);
  glfwSetScrollCallback( window , mouseScrollCB );
  glfwSetKeyCallback(window , keyFunCB);


  glEnable(GL_DEPTH_TEST);


  while(true){
    render_poly();
    TwDraw();
    glfwWaitEvents();
    glfwSwapBuffers(window);

  }

  exit(0);
}
