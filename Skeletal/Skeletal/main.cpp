////////////////////////////////////////////////////////////////////////
//
//   Source code based on asst.cpp by
//   Professor Steven Gortler
//
////////////////////////////////////////////////////////////////////////

#include <vector>
#include <string>
#include <memory>
#include <stdexcept>

#include <GL/glew.h>
#include <GL/glut.h>

#include "Skeleton.h"
#include "cvec.h"
#include "matrix4.h"
#include "rigtform.h"
#include "geometrymaker.h"
#include "ppm.h"
#include "glsupport.h"

using namespace std;      // for string, vector, iostream, and other standard C++ stuff

// G L O B A L S ///////////////////////////////////////////////////

static const float g_frustMinFov = 60.0;  // A minimal of 60 degree field of view
static float g_frustFovY = g_frustMinFov; // FOV in y direction (updated by updateFrustFovY)

static const float g_frustNear = -0.1;    // near plane
static const float g_frustFar = -50.0;    // far plane
static const float g_groundY = -2.0;      // y coordinate of the ground
static const float g_groundSize = 10.0;   // half the ground length

static int g_windowWidth = 512;
static int g_windowHeight = 512;
static bool g_mouseClickDown = false;    // is the mouse button pressed
static bool g_mouseLClickButton, g_mouseRClickButton, g_mouseMClickButton;
static int g_mouseClickX, g_mouseClickY; // coordinates for mouse click event
static int g_activeShader = 0;
static int g_multisample = 0;

struct ShaderState {
  GlProgram program;

  // Handles to uniform variables
  GLint h_uLight, h_uLight2;
  GLint h_uProjMatrix;
  GLint h_uUseBones;
  GLint h_uModelViewMatrix;
  GLint h_uNormalMatrix;
  GLint h_uBoneViewMatrix[3];
  GLint h_uBoneNormalMatrix[3];
  GLint h_uColor;

  // Handles to vertex attributes
  GLint h_aPosition;
  GLint h_aNormal;
  GLint h_aBoneNames;
  GLint h_aBoneWeights;

  ShaderState(const char* vsfn, const char* fsfn) {
    readAndCompileShader(program, vsfn, fsfn);

    const GLuint h = program; // short hand

    // Retrieve handles to uniform variables
    h_uLight = safe_glGetUniformLocation(h, "uLight");
    h_uLight2 = safe_glGetUniformLocation(h, "uLight2");
    h_uProjMatrix = safe_glGetUniformLocation(h, "uProjMatrix");
	h_uUseBones = safe_glGetUniformLocation(h, "uUseBones");
    h_uModelViewMatrix = safe_glGetUniformLocation(h, "uModelViewMatrix");
    h_uNormalMatrix = safe_glGetUniformLocation(h, "uNormalMatrix");
    h_uBoneViewMatrix[0] = safe_glGetUniformLocation(h, "uBone[0]");
    h_uBoneViewMatrix[1] = safe_glGetUniformLocation(h, "uBone[1]");
    h_uBoneViewMatrix[2] = safe_glGetUniformLocation(h, "uBone[2]");
    h_uBoneNormalMatrix[0] = safe_glGetUniformLocation(h, "uBoneNormal[0]");
    h_uBoneNormalMatrix[1] = safe_glGetUniformLocation(h, "uBoneNormal[1]");
    h_uBoneNormalMatrix[2] = safe_glGetUniformLocation(h, "uBoneNormal[2]");
	h_uColor = safe_glGetUniformLocation(h, "uColor");

    // Retrieve handles to vertex attributes
    h_aPosition = safe_glGetAttribLocation(h, "aPosition");
    h_aNormal = safe_glGetAttribLocation(h, "aNormal");
    h_aBoneNames = safe_glGetAttribLocation(h, "aBoneNames");
    h_aBoneWeights = safe_glGetAttribLocation(h, "aBoneWeights");

    checkGlErrors();
  }

};

static const int g_numShaders = 2;
static const char * const g_shaderFiles[g_numShaders][2] = {
  {"./shaders/basic.vshader", "./shaders/diffuse.fshader"},
  {"./shaders/basic.vshader", "./shaders/specular.fshader"}
};
static vector<shared_ptr<ShaderState> > g_shaderStates; // our global shader states

// --------- Geometry

// Macro used to obtain relative offset of a field within a struct
#define FIELD_OFFSET(StructType, field) &(((StructType *)0)->field)

// A vertex with floating point position and normal and bone coordinates
struct VertexPNB {
  Cvec3f p, n, bw;
  Cvec<int,3> bn;

  VertexPNB() {}
  VertexPNB(float x, float y, float z,
           float nx, float ny, float nz)
    : p(x,y,z), n(nx, ny, nz)
  {}

  VertexPNB(const SmallVertex& v)
	  : p(v.pos), n(v.normal), bn(v.boneNames), bw(v.boneWeights)
  {}

  // Define copy constructor and assignment operator from GenericVertex so we can
  // use make* functions from geometrymaker.h
  VertexPNB(const GenericVertex& v) {
    *this = v;
  }

  VertexPNB& operator = (const GenericVertex& v) {
    p = v.pos;
    n = v.normal;
    return *this;
  }
};

struct Geometry {
  GlBufferObject vbo, ibo;
  int vboLen, iboLen;
  int strips;

  Geometry(VertexPNB *vtx, unsigned short *idx, int vboLen, int iboLen,int strip_count = 0) {
    this->vboLen = vboLen;
    this->iboLen = iboLen;
    this->strips = strip_count;

    // Now create the VBO and IBO
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(VertexPNB) * vboLen, vtx, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned short) * iboLen, idx, GL_STATIC_DRAW);
  }

  void draw(const ShaderState& curSS) {
    // Enable the attributes used by our shader
    safe_glEnableVertexAttribArray(curSS.h_aPosition);
    safe_glEnableVertexAttribArray(curSS.h_aNormal);
    safe_glEnableVertexAttribArray(curSS.h_aBoneNames);
    safe_glEnableVertexAttribArray(curSS.h_aBoneWeights);

    // bind vbo
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    safe_glVertexAttribPointer(curSS.h_aPosition, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPNB), FIELD_OFFSET(VertexPNB, p));
    safe_glVertexAttribPointer(curSS.h_aNormal, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPNB), FIELD_OFFSET(VertexPNB, n));
    safe_glVertexAttribIPointer(curSS.h_aBoneNames, 3, GL_INT, sizeof(VertexPNB), FIELD_OFFSET(VertexPNB, bn));
    safe_glVertexAttribPointer(curSS.h_aBoneWeights, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPNB), FIELD_OFFSET(VertexPNB, bw));

    // bind ibo
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);

    // draw!
    if(strips > 0)
      for(int s = 0;s < strips;s++)
        glDrawElements(GL_TRIANGLE_STRIP, iboLen/strips, GL_UNSIGNED_SHORT, 
             (const GLvoid*) (s*iboLen/strips*sizeof(unsigned short)));
    else
      glDrawElements(GL_TRIANGLES, iboLen, GL_UNSIGNED_SHORT, 0);

    // Disable the attributes used by our shader
    safe_glDisableVertexAttribArray(curSS.h_aPosition);
    safe_glDisableVertexAttribArray(curSS.h_aNormal);
    safe_glDisableVertexAttribArray(curSS.h_aBoneNames);
    safe_glDisableVertexAttribArray(curSS.h_aBoneWeights);
  }
};

// Vertex buffer and index buffer associated with the ground and surface geometry
static shared_ptr<Geometry> g_ground, g_surface;
static shared_ptr<Skeleton> g_skeleton;

// --------- Scene

static const Cvec3 g_light1(2.0, 3.0, 14.0), g_light2(-2, -3.0, -5.0);  // define two lights positions in world space
static RigTForm g_skyRbt(Cvec3(0.0, 0.25, 4.0));
static RigTForm g_objectRbt[1] = {RigTForm(Cvec3(0,-1,0))};  // One surface
static Cvec3f g_objectColors[1] = {Cvec3f(0, 0, 1)};

///////////////// END OF G L O B A L S //////////////////////////////////////////////////

static void initGround() {
  // A x-z plane at y = g_groundY of dimension [-g_groundSize, g_groundSize]^2
  VertexPNB vtx[4] = {
    VertexPNB(-g_groundSize, g_groundY, -g_groundSize, 0, 1, 0),
    VertexPNB(-g_groundSize, g_groundY,  g_groundSize, 0, 1, 0),
    VertexPNB( g_groundSize, g_groundY,  g_groundSize, 0, 1, 0),
    VertexPNB( g_groundSize, g_groundY, -g_groundSize, 0, 1, 0),
  };
  unsigned short idx[] = {0, 1, 2, 0, 2, 3};
  g_ground.reset(new Geometry(&vtx[0], &idx[0], 4, 6));
}

Cvec3f makeCylinderV(float s,float t) { return Cvec3f(0.4*cos(s),2*t,0.4*sin(s)); }
Cvec3f makeCylinderN(float s,float t) { return Cvec3f(cos(s),0.0,sin(s)); }
Cvec<int, 3> makeCylinderBN(float s, float t) { return Cvec<int, 3>(0, 1, 2); }
Cvec3f makeCylinderBW(float s,float t) 
{
	if (t < 1.0 / 4)
		return Cvec3f(1.0, 0.0, 0.0);
	if (t < 5.0 / 12)
		return Cvec3f(2.5 - 6 * t, -1.5 + 6 * t, 0.0);
	if (t < 7.0 / 12)
		return Cvec3f(0.0, 1.0, 0.0);
	if (t < 3.0 / 4)
		return Cvec3f(0.0, 4.5 - 6 * t, -3.5 + 6 * t);
	return Cvec3f(0.0, 0.0, 1.0);
}
static void initSurface() {
  int ibLen, vbLen;
  int sliceCount = 20;
  getSurfaceVbIbLen(20,true,sliceCount,false,vbLen, ibLen);

  // Temporary storage for surface geometry
  vector<VertexPNB> vtx(vbLen);
  vector<unsigned short> idx(ibLen);

  makeSurface(0.0,2*CS175_PI/20,20,true,0.0,1.0/sliceCount,sliceCount,false,
              makeCylinderV,makeCylinderN, makeCylinderBN, makeCylinderBW, 
              vtx.begin(), idx.begin());

  g_surface.reset(new Geometry(&vtx[0], &idx[0], vbLen, ibLen,sliceCount));
  g_skeleton.reset(new Skeleton());

  Bone* added = g_skeleton->addBone(0, NULL, RigTForm());
  added = g_skeleton->addBone(1, added, RigTForm(Cvec3(0.0, 2.0 / 3, 0.0)));
  added = g_skeleton->addBone(2, added, RigTForm(Cvec3(0.0, 2.0 / 3, 0.0)));


  // Now tweak the bones
  //g_skeleton->getNamedBone(1)->rotate(Quat::makeZRotation(90));
}

// takes a projection matrix and send to the the shaders
static void sendProjectionMatrix(const ShaderState& curSS, const Matrix4& projMatrix) {
  GLfloat glmatrix[16];
  projMatrix.writeToColumnMajorMatrix(glmatrix); // send projection matrix
  safe_glUniformMatrix4fv(curSS.h_uProjMatrix, glmatrix);
}

// takes MVM and its normal matrix to the shaders
static void sendModelViewNormalMatrix(const ShaderState& curSS, const Matrix4& MVM, const Matrix4& NMVM) {
  GLfloat glmatrix[16];
  MVM.writeToColumnMajorMatrix(glmatrix); // send MVM
  safe_glUniformMatrix4fv(curSS.h_uModelViewMatrix, glmatrix);

  NMVM.writeToColumnMajorMatrix(glmatrix); // send NMVM
  safe_glUniformMatrix4fv(curSS.h_uNormalMatrix, glmatrix);
}

// takes bone matrices to the shaders
static void sendBones(const ShaderState& curSS, Matrix4 bones[], Matrix4 normals[],int boneCount) {
  GLfloat glmatrix[16];
  for(int n = 0;n < boneCount;n++) {
	  bones[n].writeToColumnMajorMatrix(glmatrix); // send bones[n]
	  safe_glUniformMatrix4fv(curSS.h_uBoneViewMatrix[n], glmatrix);
	  normals[n].writeToColumnMajorMatrix(glmatrix); // send normals[n]
	  safe_glUniformMatrix4fv(curSS.h_uBoneNormalMatrix[n], glmatrix);
  }
}

// update g_frustFovY from g_frustMinFov, g_windowWidth, and g_windowHeight
static void updateFrustFovY() {
  if (g_windowWidth >= g_windowHeight)
    g_frustFovY = g_frustMinFov;
  else {
    const double RAD_PER_DEG = 0.5 * CS175_PI/180;
    g_frustFovY = atan2(sin(g_frustMinFov * RAD_PER_DEG) * g_windowHeight / g_windowWidth, cos(g_frustMinFov * RAD_PER_DEG)) / RAD_PER_DEG;
  }
}

static Matrix4 makeProjectionMatrix() {
  return Matrix4::makeProjection(
           g_frustFovY, g_windowWidth / static_cast <double> (g_windowHeight),
           g_frustNear, g_frustFar);
}

static void drawStuff() {
  // short hand for current shader state
  const ShaderState& curSS = *g_shaderStates[g_activeShader];

  // build & send proj. matrix to vshader
  const Matrix4 projmat = makeProjectionMatrix();
  sendProjectionMatrix(curSS, projmat);

  // use the skyRbt as the eyeRbt
  const Matrix4 eyeRbt = rigTFormToMatrix(g_skyRbt);
  const Matrix4 invEyeRbt = inv(eyeRbt);

  const Cvec3 eyeLight1 = Cvec3(invEyeRbt * Cvec4(g_light1, 1)); // g_light1 position in eye coordinates
  const Cvec3 eyeLight2 = Cvec3(invEyeRbt * Cvec4(g_light2, 1)); // g_light2 position in eye coordinates
  safe_glUniform3f(curSS.h_uLight, eyeLight1[0], eyeLight1[1], eyeLight1[2]);
  safe_glUniform3f(curSS.h_uLight2, eyeLight2[0], eyeLight2[1], eyeLight2[2]);
  
  // draw ground
  // ===========
  //
  const Matrix4 groundRbt = Matrix4();  // identity
  Matrix4 MVM = invEyeRbt * groundRbt;
  Matrix4 NMVM = normalMatrix(MVM);
  sendModelViewNormalMatrix(curSS, MVM, NMVM);
  safe_glUniform1i(curSS.h_uUseBones,0);
  safe_glUniform3f(curSS.h_uColor, 0.1, 0.95, 0.1); // set color
  g_ground->draw(curSS);

  // draw shape
  // ==========
  Matrix4 bones[3];
  Matrix4 normals[3];
  for(int n = 0;n < 3;n++) {
	  bones[n] = invEyeRbt * rigTFormToMatrix(g_objectRbt[0])*g_skeleton->getNamedBone(n)->getBoneMatrix();
      normals[n] = normalMatrix(bones[n]);
  }
  sendBones(curSS,bones,normals,3);
  safe_glUniform1i(curSS.h_uUseBones,1);
  safe_glUniform3f(curSS.h_uColor, g_objectColors[0][0], g_objectColors[0][1], g_objectColors[0][2]);
  g_surface->draw(curSS);
}

static void display() {
  glUseProgram(g_shaderStates[g_activeShader]->program);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);                   // clear framebuffer color&depth

  if(g_multisample) {
	  glEnable(GL_MULTISAMPLE_ARB);
   }
  else {
	  glDisable(GL_MULTISAMPLE_ARB);
  }

  drawStuff();

  glutSwapBuffers();                                    // show the back buffer (where we rendered stuff)

  checkGlErrors();
}

static void reshape(const int w, const int h) {
  g_windowWidth = w;
  g_windowHeight = h;
  glViewport(0, 0, w, h);
  cerr << "Size of window is now " << w << "x" << h << endl;
  updateFrustFovY();
  glutPostRedisplay();
}

int bn;
static void motion(const int x, const int y) {
  const double dx = x - g_mouseClickX;
  const double dy = g_windowHeight - y - 1 - g_mouseClickY;

  if (g_mouseClickDown) {

	  if (g_mouseLClickButton && !g_mouseRClickButton) { // left button down?
		  //g_objectRbt[0].setRotation(g_objectRbt[0].getRotation()*Quat::makeXRotation(-dy) * Quat::makeYRotation(dx));
		  g_skeleton->getNamedBone(bn)->rotate(Quat::makeZRotation(-dy)* Quat::makeZRotation(-dx));
	  }
	  else if (g_mouseRClickButton && !g_mouseLClickButton) { // right button down?
		  g_objectRbt[0].setTranslation(g_objectRbt[0].getTranslation() + Cvec3(dx, dy, 0) * 0.01);
	  }
	  else if (g_mouseMClickButton || (g_mouseLClickButton && g_mouseRClickButton)) {  // middle or (left and right) button down?
		  g_objectRbt[0].setTranslation(g_objectRbt[0].getTranslation() + Cvec3(0, 0, -dy) * 0.01);
	  }

	  glutPostRedisplay(); // we always redraw if we changed the scene

  }

  g_mouseClickX = x;
  g_mouseClickY = g_windowHeight - y - 1;
}

static void mouse(const int button, const int state, const int x, const int y) {
  g_mouseClickX = x;
  g_mouseClickY = g_windowHeight - y - 1;  // conversion from GLUT window-coordinate-system to OpenGL window-coordinate-system

  g_mouseLClickButton |= (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN);
  g_mouseRClickButton |= (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN);
  g_mouseMClickButton |= (button == GLUT_MIDDLE_BUTTON && state == GLUT_DOWN);

  g_mouseLClickButton &= !(button == GLUT_LEFT_BUTTON && state == GLUT_UP);
  g_mouseRClickButton &= !(button == GLUT_RIGHT_BUTTON && state == GLUT_UP);
  g_mouseMClickButton &= !(button == GLUT_MIDDLE_BUTTON && state == GLUT_UP);

  g_mouseClickDown = g_mouseLClickButton || g_mouseRClickButton || g_mouseMClickButton;
}


Quat currkey0, currkey1, currkey2;

static void keyFrameAnimate(int x) {
	Quat key01(Quat::makeZRotation(60));
	Quat key02 = Quat();
	Quat key11(Quat::makeZRotation(30));
	Quat key12(Quat::makeZRotation(-10));
	Quat key21(Quat::makeZRotation(20));
	Quat key22(Quat::makeZRotation(10));
	if (x == 0) {
		currkey0 = g_skeleton->getNamedBone(0)->getRotation();
		currkey1 = g_skeleton->getNamedBone(1)->getRotation();
		currkey2 = g_skeleton->getNamedBone(2)->getRotation();
	}
	if (x < 50) {
		g_skeleton->getNamedBone(0)->setRotate(slerp(currkey0, key01, x / 50.0));
		g_skeleton->getNamedBone(1)->setRotate(slerp(currkey1, key11, x / 50.0));
		g_skeleton->getNamedBone(2)->setRotate(slerp(currkey2, key21, x / 50.0));
		glutPostRedisplay();
		glutTimerFunc(20, keyFrameAnimate, x + 1);
	}
	else if (x < 100) {
		g_skeleton->getNamedBone(0)->setRotate(slerp(key01, key02, (x - 50) / 50.0));
		g_skeleton->getNamedBone(1)->setRotate(slerp(key11, key12, (x - 50) / 50.0));
		g_skeleton->getNamedBone(2)->setRotate(slerp(key21, key22, (x - 50) / 50.0));

		glutPostRedisplay();
		glutTimerFunc(20, keyFrameAnimate, x + 1);
	}
	else {
		g_skeleton->getNamedBone(0)->setRotate(key02);
		g_skeleton->getNamedBone(1)->setRotate(key12);
		g_skeleton->getNamedBone(2)->setRotate(key22);
		glutPostRedisplay();
	}
}

static void keyFrameAnimate2(int x) {
	Quat key01(Quat::makeZRotation(60));
	Quat key02 = Quat();
	Quat key11(Quat::makeXRotation(30)*Quat::makeYRotation(-20));
	Quat key12(Quat::makeYRotation(-50));
	Quat key21(Quat::makeYRotation(20));
	Quat key22(Quat::makeXRotation(10) * Quat::makeZRotation(90));
	if (x == 0) {
		currkey0 = g_skeleton->getNamedBone(0)->getRotation();
		currkey1 = g_skeleton->getNamedBone(1)->getRotation();
		currkey2 = g_skeleton->getNamedBone(2)->getRotation();
	}
	if (x < 50) {
		g_skeleton->getNamedBone(0)->setRotate(slerp(currkey0, key01, x / 50.0));
		g_skeleton->getNamedBone(1)->setRotate(slerp(currkey1, key11, x / 50.0));
		g_skeleton->getNamedBone(2)->setRotate(slerp(currkey2, key21, x / 50.0));
		glutPostRedisplay();
		glutTimerFunc(20, keyFrameAnimate2, x + 1);
	}
	else if (x < 100) {
		g_skeleton->getNamedBone(0)->setRotate(slerp(key01, key02, (x - 50) / 50.0));
		g_skeleton->getNamedBone(1)->setRotate(slerp(key11, key12, (x - 50) / 50.0));
		g_skeleton->getNamedBone(2)->setRotate(slerp(key21, key22, (x - 50) / 50.0));

		glutPostRedisplay();
		glutTimerFunc(20, keyFrameAnimate2, x + 1);
	}
	else {
		g_skeleton->getNamedBone(0)->setRotate(key02);
		g_skeleton->getNamedBone(1)->setRotate(key12);
		g_skeleton->getNamedBone(2)->setRotate(key22);
		glutPostRedisplay();
	}
}

static void keyboard(const unsigned char key, const int x, const int y) {
  switch (key) {
  case 27:
    exit(0);                                  // ESC
  case 'h':
    cout << " ============== H E L P ==============\n\n"
    << "h\t\thelp menu\n"
    << "s\t\tsave screenshot\n"
    << "1\t\tDiffuse only\n"
	<< "2\t\tDiffuse and specular\n"
	<< "a\t\tAnimate shape\n"
    << "drag left mouse to rotate\n" << endl;
    break;
  case 's':
    glFlush();
    writePpmScreenshot(g_windowWidth, g_windowHeight, "out.ppm");
    break;
  case '1':
	  g_activeShader = 0;
	  break;
  case '2':
	  g_activeShader = 1;
	  break;
  case 'q':
	  bn = 0;
	  break;
  case 'w':
	  bn = 1;
	  break;
  case 'e':
	  bn = 2;
	  break;
  case 'k':
	  glutTimerFunc(20, keyFrameAnimate, 0);
	  break;
  case 'l':
	  glutTimerFunc(20, keyFrameAnimate2, 0);
	  break;
  }
  glutPostRedisplay();
}

static void initGlutState(int argc, char * argv[]) {
  glutInit(&argc, argv);                                  // initialize Glut based on cmd-line args
  glutInitDisplayMode(GLUT_RGBA|GLUT_DOUBLE|GLUT_DEPTH|GLUT_MULTISAMPLE);  //  RGBA pixel channels and double buffering
  glutInitWindowSize(g_windowWidth, g_windowHeight);      // create a window
  glutCreateWindow("Skinning Demo");                       // title the window

  glutDisplayFunc(display);                               // display rendering callback
  glutReshapeFunc(reshape);                               // window reshape callback
  glutMotionFunc(motion);                                 // mouse movement callback
  glutMouseFunc(mouse);                                   // mouse click callback
  glutKeyboardFunc(keyboard);
}

static void initGLState() {
  glClearColor(128./255., 200./255., 255./255., 0.);
  glClearDepth(0.);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  //glCullFace(GL_BACK);
  //glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_GREATER);
  glReadBuffer(GL_BACK);

  int samples;
  glGetIntegerv(GL_SAMPLES, &samples);
  cout << "Number of samples is " << samples << endl;
}

static void initShaders() {
  g_shaderStates.resize(g_numShaders);
  for (int i = 0; i < g_numShaders; ++i) {
    g_shaderStates[i].reset(new ShaderState(g_shaderFiles[i][0], g_shaderFiles[i][1]));
  }
}

static void initGeometry() {
  initGround();
  initSurface();
}

int main(int argc, char * argv[]) {
  try {
    initGlutState(argc,argv);

    glewInit(); // load the OpenGL extensions

    if (!GLEW_VERSION_3_0)
      throw runtime_error("Error: card/driver does not support OpenGL Shading Language v1.3");
    
    initGLState();
    initShaders();
    initGeometry();

    glutMainLoop();
    return 0;
  }
  catch (const runtime_error& e) {
    cout << "Exception caught: " << e.what() << endl;
    return -1;
  }
}
