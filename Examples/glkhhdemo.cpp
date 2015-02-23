#include <iostream>

#include <Hale.h>

#include <glm/glm.hpp>

/* The parameters for call to generate_spiral */
float lpd_alpha = 0.2;
float lpd_beta = 0.2;
float lpd_theta = 50;
float lpd_phi = 50;

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
  //flag = ((1 << limnPolyDataInfoRGBA) | (1 << limnPolyDataInfoNorm));
  flag = (1 << limnPolyDataInfoNorm);

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

  if (0) {
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
  }

  return lpd;
}

void render(Hale::Viewer *viewer){
  Hale::uniform("proj", viewer->camera.project());
  Hale::uniform("view", viewer->camera.view());

  viewer->draw();
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

  /* first, create empty scene */
  Hale::Scene scene;

  /* then create viewer (in order to create the OpenGL context) */
  Hale::Viewer viewer(640, 480, "Bingo", &scene);
  viewer.lightDir(glm::vec3(-1.0f, 1.0f, 3.0f));
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

  /* then create geometry, and add it to scene */
  limnPolyData *poly = generate_spiral(lpd_alpha, lpd_beta, lpd_theta, lpd_phi);
  Hale::Polydata hply(poly, true); // hply now owns poly
  hply.model(glm::mat4(200.2f, 0.0f, 0.0f, 0.0f,
                       0.0f, 20.2f, 0.0f, 0.0f,
                       0.0f, 0.0f, 200.0f, 0.0f,
                       200.0f, 0.0f, 0.0f, 1.0f));
  hply.colorSolid(1, 1, 0);
  scene.add(&hply);

  Hale::Polydata hply2(generate_spiral(1.0, 1.0, 5, 5), true); // hply now owns poly
  hply2.colorSolid(1, 1, 1);
  scene.add(&hply2);

  Hale::Program program(Hale::preprogramAmbDiffSolid);
  program.compile();
  program.bindAttribute(Hale::vertAttrIdxXYZW, "position");
  program.bindAttribute(Hale::vertAttrIdxNorm, "norm");
  program.bindAttribute(Hale::vertAttrIdxRGBA, "color");
  program.link();
  program.use();

  scene.drawInit();
  while(!Hale::finishing){
    glfwWaitEvents();
    render(&viewer);
  }

  /* clean exit; all okay */
  airMopOkay(mop);
  return 0;
}
