#include <iostream>

#include <Hale.h>

#include <glm/glm.hpp>

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

struct render_info {
  GLuint vao = -1;
  GLuint buffs[3];
  GLuint elms;
} render;

#define OLD 0

// global as short-term hack
Hale::Program *program = NULL;

// also global as short-term hack
Hale::Polydata *hply = NULL;

glm::vec3 light_dir = glm::normalize(glm::vec3(1.0f,1.0f,3.0f));

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
  static const std::string me="render_poly";
  //fprintf(stderr, "%s: hi\n", me);

  program->uniform("proj", viewer->camera.project());
  program->uniform("view", viewer->camera.view());
  program->uniform("light_dir", light_dir);

  if (OLD) {
    /* ? needed with every render call ? */
    glBindVertexArray(render.vao);
    // printf("#""glBindVertexArray(%u)\n", render.vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render.elms);
    // printf("#""glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, %u)\n", render.elms);

    int offset = 0;
    //Render all specified primatives
    for(int i = 0; i < poly->primNum; i++){
      glDrawElements( Hale::limnToGLPrim(poly->type[i]), poly->icnt[i],
                      GL_UNSIGNED_INT, ((void*) 0));
      // printf("#""glDrawElements(%u, %u, GL_UNSIGNED_INT, 0);\n", Hale::limnToGLPrim(poly->type[i]), poly->icnt[i]);
      Hale::glErrorCheck(me, "glDrawElements(prim " + std::to_string(i) + ")");
      offset += poly->icnt[i];
    }
  } else {
    hply->draw();
  }

  viewer->bufferSwap();
}

/* Buffer the data stored in the global limPolyData Poly.
 * Buffer_new is set to true if the number or connectiviity of the vertices
 * has changed since the last buffering.
 */
void buffer_data(limnPolyData *lpd, bool buffer_new){

  //First Pass
  if(render.vao == -1){

    glGenBuffers(3,render.buffs);
    // printf("#""glGenBuffers(3, &); -> %u %u %u\n", render.buffs[0], render.buffs[1], render.buffs[2]);
    glGenVertexArrays(1, &(render.vao));
    // printf("#""glGenVertexArrays(1, &); -> %u\n", render.vao);
    glBindVertexArray(render.vao);
    // printf("#""glBindVertexArray(%u);\n", render.vao);
    glEnableVertexAttribArray(Hale::vertAttrIdxXYZW);
    // printf("#""glEnableVertexAttribArray(%u);\n", Hale::vertAttrIdxXYZW);
    glEnableVertexAttribArray(Hale::vertAttrIdxRGBA);
    // printf("#""glEnableVertexAttribArray(%u);\n", Hale::vertAttrIdxRGBA);
    glEnableVertexAttribArray(Hale::vertAttrIdxNorm);
    // printf("#""glEnableVertexAttribArray(%u);\n", Hale::vertAttrIdxNorm);
  }

  //Verts
  glBindBuffer(GL_ARRAY_BUFFER, render.buffs[0]);
  // printf("#""glBindBuffer(GL_ARRAY_BUFFER, %u);\n", render.buffs[0]);
  if(buffer_new) {
    glBufferData(GL_ARRAY_BUFFER, lpd->xyzwNum*sizeof(float)*4,
		 lpd->xyzw, GL_DYNAMIC_DRAW);
    // printf("#""glBufferData(GL_ARRAY_BUFFER, %u, lpd->xyzw, GL_DYNAMIC_DRAW);\n", (unsigned int)(lpd->xyzwNum*sizeof(float)*4));
  } else {//No change in number of vertices
    glBufferSubData(GL_ARRAY_BUFFER, 0, lpd->xyzwNum*sizeof(float)*4,lpd->xyzw);
    // printf("#""glBufferSubData(GL_ARRAY_BUFFER, 0, %u, lpd->xyzw);\n", (unsigned int)(lpd->xyzwNum*sizeof(float)*4));
  }
  glVertexAttribPointer(Hale::vertAttrIdxXYZW, 4, GL_FLOAT, GL_FALSE, 0, 0);
  // printf("#""glVertexAttribPointer(%u, 4, GL_FLOAT, GL_FALSE, 0, 0);\n", Hale::vertAttrIdxXYZW);

  //Norms
  glBindBuffer(GL_ARRAY_BUFFER, render.buffs[1]);
  // printf("#""glBindBuffer(GL_ARRAY_BUFFER, %u);\n", render.buffs[1]);
  if(buffer_new) {
    glBufferData(GL_ARRAY_BUFFER, lpd->normNum*sizeof(float)*3,
		 lpd->norm, GL_DYNAMIC_DRAW);
    // printf("#""glBufferData(GL_ARRAY_BUFFER, %u, lpd->norm, GL_DYNAMIC_DRAW);\n", (unsigned int)(lpd->normNum*sizeof(float)*3));
  } else {
    glBufferSubData(GL_ARRAY_BUFFER, 0, lpd->normNum*sizeof(float)*3,lpd->norm);
    // printf("#""glBufferSubData(GL_ARRAY_BUFFER, 0, %u, lpd->norm);\n", (unsigned int)(lpd->normNum*sizeof(float)*3));
  }
  glVertexAttribPointer(Hale::vertAttrIdxNorm, 3, GL_FLOAT, GL_FALSE, 0, 0);
  // printf("#""glVertexAttribPointer(%u, 3, GL_FLOAT, GL_FALSE, 0, 0);\n", Hale::vertAttrIdxNorm);

  //Colors
  glBindBuffer(GL_ARRAY_BUFFER, render.buffs[2]);
  // printf("#""glBindBuffer(GL_ARRAY_BUFFER, %u);\n", render.buffs[2]);
  if(buffer_new) {
    glBufferData(GL_ARRAY_BUFFER, lpd->rgbaNum*sizeof(char)*4, lpd->rgba, GL_DYNAMIC_DRAW);
    // printf("#""glBufferData(GL_ARRAY_BUFFER, %u, lpd->rgba, GL_DYNAMIC_DRAW);\n", (unsigned int)(lpd->rgbaNum*sizeof(char)*4));
  } else {
    glBufferSubData(GL_ARRAY_BUFFER, 0, lpd->rgbaNum*sizeof(char)*4,lpd->rgba);
    // printf("#""glBufferSubData(GL_ARRAY_BUFFER, 0, %u, lpd->rgba);\n", (unsigned int)(lpd->rgbaNum*sizeof(char)*4));
  }
  glVertexAttribPointer(Hale::vertAttrIdxRGBA, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, 0);
  // printf("#""glVertexAttribPointer(%u, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, 0);\n", Hale::vertAttrIdxRGBA);

  if(buffer_new){
    //Indices
    glGenBuffers(1, &(render.elms));
    // printf("#""glGenBuffers(1, &); -> %u\n", render.elms);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render.elms);
    // printf("#""glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, %u);\n", render.elms);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
		 lpd->indxNum * sizeof(unsigned int),
		 lpd->indx, GL_DYNAMIC_DRAW);
    // printf("#""glBufferData(GL_ELEMENT_ARRAY_BUFFER, %u, lpd->indx, GL_DYNAMIC_DRAW);\n", (unsigned int)(lpd->indxNum * sizeof(unsigned int)));
  }
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

  Hale::init();
  Hale::Viewer viewer(width, height, "Bingo");
  viewer.camera.init(glm::vec3(8.0f,0.0f,0.0f),
                     glm::vec3(0.0f,0.0f,0.0f),
                     glm::vec3(0.0f, 1.0f, 0.0f),
                     25, 1.33333, -2, 2, false);
  viewer.refreshCB((Hale::ViewerRefresher)render_poly);
  /* using this pointer as the data for the refresh
     callback is a little silly, but we may want this
     ability for something better later */
  viewer.refreshData(&viewer);
  viewer.current();

  program = new Hale::Program("glsl/demo_v.glsl", "glsl/demo_f.glsl");
  program->compile();
  program->bindAttribute(Hale::vertAttrIdxXYZW, "position");
  program->bindAttribute(Hale::vertAttrIdxNorm, "norm");
  program->bindAttribute(Hale::vertAttrIdxRGBA, "color");
  program->link();
  program->use();

  poly = generate_spiral(lpd_alpha, lpd_beta, lpd_theta, lpd_phi);
  if (OLD) {
    buffer_data(poly,true);
    /*
    glBindVertexArray(render.vao);
    // printf("#""glBindVertexArray(%u);\n", render.vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render.elms);
    // printf("#""glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, %u);\n", render.elms);
    */
  } else {
    hply = new Hale::Polydata(poly);
  }

  glClearColor(0.13f, 0.16f, 0.2f, 1.0f);
  glEnable(GL_DEPTH_TEST);
  //glEnable(GL_BLEND);
  //glBlendFunc(GL_SRC_ALPHA,GL_ONE);

  while(!Hale::finishing){
    glfwWaitEvents();
    glClear(GL_DEPTH_BUFFER_BIT);
    glClear(GL_COLOR_BUFFER_BIT);
    render_poly(&viewer);
  }

  /* clean exit; all okay */
  airMopOkay(mop);
  return 0;
}
