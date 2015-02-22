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

// global as short-term hack
Hale::Program *program = NULL;

// also global as short-term hack
Hale::Polydata *hply = NULL;

glm::vec3 light_dir = glm::normalize(glm::vec3(-1.0f,1.0f,3.0f));

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
void render(Hale::Viewer *viewer){
  static const std::string me="render";
  //fprintf(stderr, "%s: hi\n", me);

  program->uniform("proj", viewer->camera.project());
  program->uniform("view", viewer->camera.view());
  program->uniform("light_dir", light_dir);

  hply->draw();

  viewer->bufferSwap();
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
  viewer.refreshCB((Hale::ViewerRefresher)render);
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
  hply = new Hale::Polydata(poly);

  glClearColor(0.13f, 0.16f, 0.2f, 1.0f);
  glEnable(GL_DEPTH_TEST);
  //glEnable(GL_BLEND);
  //glBlendFunc(GL_SRC_ALPHA,GL_ONE);

  while(!Hale::finishing){
    glfwWaitEvents();
    glClear(GL_DEPTH_BUFFER_BIT);
    glClear(GL_COLOR_BUFFER_BIT);
    render(&viewer);
  }

  /* clean exit; all okay */
  airMopOkay(mop);
  return 0;
}
