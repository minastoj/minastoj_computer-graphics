/*
  CSCI 420 Computer Graphics, USC
  Assignment 2: Roller Coaster
  C++ starter code

  Student username: mstojano
*/

#include "basicPipelineProgram.h"
#include "openGLMatrix.h"
#include "imageIO.h"
#include "openGLHeader.h"
#include "glutHeader.h"

#include <iostream>
#include <cstring>
#include <vector>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(WIN32) || defined(_WIN32)
  #ifdef _DEBUG
    #pragma comment(lib, "glew32d.lib")
  #else
    #pragma comment(lib, "glew32.lib")
  #endif
#endif

#if defined(WIN32) || defined(_WIN32)
  char shaderBasePath[1024] = SHADER_BASE_PATH;
#else
  char shaderBasePath[1024] = "../openGLHelper-starterCode";
#endif

using namespace std;

int mousePos[2]; // x,y screen coordinates of the current mouse position

int leftMouseButton = 0; // 1 if pressed, 0 if not 
int middleMouseButton = 0; // 1 if pressed, 0 if not
int rightMouseButton = 0; // 1 if pressed, 0 if not

typedef enum { ROTATE, TRANSLATE, SCALE } CONTROL_STATE;
CONTROL_STATE controlState = ROTATE;

// Transformations of the terrain.
float terrainRotate[3] = { 0.0f, 0.0f, 0.0f }; 
// terrainRotate[0] gives the rotation around x-axis (in degrees)
// terrainRotate[1] gives the rotation around y-axis (in degrees)
// terrainRotate[2] gives the rotation around z-axis (in degrees)
float terrainTranslate[3] = { 0.0f, 0.0f, 0.0f };
float terrainScale[3] = { 1.0f, 1.0f, 1.0f };

// Width and height of the OpenGL window, in pixels.
int windowWidth = 1280;
int windowHeight = 720;
char windowTitle[512] = "CSCI 420 homework I";

// VBOs
GLuint splinesVBO;
GLuint railVBO;
GLuint groundVBO;
GLuint skyVBO;

// VAOs
GLuint splinesVAO;
GLuint railVAO;
GLuint groundVAO;
GLuint skyVAO;

int screenshotCount = 0;

int numPointsInSplines = 0;

int cameraFrame = 0;

vector<float> cameraPositions; // holds the positions of the camera as it travels along the spline
vector<float> cameraTangents; // holds the tangents of the camera
vector<float> cameraNormals; // holds the normals (up vectors) of the camera
vector<float> cameraBinormals; // holds the binormals of the camera

vector<float> railPositions; // holds the vertices of the rail (triangles)
vector<float> railColors; // holds the colors of the rail (triangles)

vector<float> groundPositions;
vector<float> skyPositions;

// CSCI 420 helper classes.
OpenGLMatrix matrix;
BasicPipelineProgram * railPipelineProgram;
BasicPipelineProgram * groundPipelineProgram;
BasicPipelineProgram * skyPipelineProgram;

GLuint groundTexHandle;
GLuint skyTexHandle;

// represents one control point along the spline 
struct Point 
{
  double x;
  double y;
  double z;
};

// spline struct 
// contains how many control points the spline has, and an array of control points 
struct Spline 
{
  int numControlPoints;
  Point * points;
};

// the spline array 
Spline * splines;
// total number of splines 
int numSplines;

int loadSplines(char * argv) 
{
  char * cName = (char *) malloc(128 * sizeof(char));
  FILE * fileList;
  FILE * fileSpline;
  int iType, i = 0, j, iLength;

  // load the track file 
  fileList = fopen(argv, "r");
  if (fileList == NULL) 
  {
    printf ("can't open file\n");
    exit(1);
  }
  
  // stores the number of splines in a global variable 
  fscanf(fileList, "%d", &numSplines);

  splines = (Spline*) malloc(numSplines * sizeof(Spline));

  // reads through the spline files 
  for (j = 0; j < numSplines; j++) 
  {
    i = 0;
    fscanf(fileList, "%s", cName);
    fileSpline = fopen(cName, "r");

    if (fileSpline == NULL) 
    {
      printf ("can't open file\n");
      exit(1);
    }

    // gets length for spline file
    fscanf(fileSpline, "%d %d", &iLength, &iType);

    // allocate memory for all the points
    splines[j].points = (Point *)malloc(iLength * sizeof(Point));
    splines[j].numControlPoints = iLength;

    // saves the data to the struct
    while (fscanf(fileSpline, "%lf %lf %lf", 
	   &splines[j].points[i].x, 
	   &splines[j].points[i].y, 
	   &splines[j].points[i].z) != EOF) 
    {
      i++;
    }
  }

  free(cName);

  return 0;
}

int initTexture(const char * imageFilename, GLuint textureHandle)
{
  // read the texture image
  ImageIO img;
  ImageIO::fileFormatType imgFormat;
  ImageIO::errorType err = img.load(imageFilename, &imgFormat);

  if (err != ImageIO::OK) 
  {
    printf("Loading texture from %s failed.\n", imageFilename);
    return -1;
  }

  // check that the number of bytes is a multiple of 4
  if (img.getWidth() * img.getBytesPerPixel() % 4) 
  {
    printf("Error (%s): The width*numChannels in the loaded image must be a multiple of 4.\n", imageFilename);
    return -1;
  }

  // allocate space for an array of pixels
  int width = img.getWidth();
  int height = img.getHeight();
  unsigned char * pixelsRGBA = new unsigned char[4 * width * height]; // we will use 4 bytes per pixel, i.e., RGBA

  // fill the pixelsRGBA array with the image pixels
  memset(pixelsRGBA, 0, 4 * width * height); // set all bytes to 0
  for (int h = 0; h < height; h++)
    for (int w = 0; w < width; w++) 
    {
      // assign some default byte values (for the case where img.getBytesPerPixel() < 4)
      pixelsRGBA[4 * (h * width + w) + 0] = 0; // red
      pixelsRGBA[4 * (h * width + w) + 1] = 0; // green
      pixelsRGBA[4 * (h * width + w) + 2] = 0; // blue
      pixelsRGBA[4 * (h * width + w) + 3] = 255; // alpha channel; fully opaque

      // set the RGBA channels, based on the loaded image
      int numChannels = img.getBytesPerPixel();
      for (int c = 0; c < numChannels; c++) // only set as many channels as are available in the loaded image; the rest get the default value
        pixelsRGBA[4 * (h * width + w) + c] = img.getPixel(w, h, c);
    }

  // bind the texture
  glBindTexture(GL_TEXTURE_2D, textureHandle);

  // initialize the texture
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelsRGBA);

  // generate the mipmaps for this texture
  glGenerateMipmap(GL_TEXTURE_2D);

  // set the texture parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  // query support for anisotropic texture filtering
  GLfloat fLargest;
  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &fLargest);
  printf("Max available anisotropic samples: %f\n", fLargest);
  // set anisotropic texture filtering
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 0.5f * fLargest);

  // query for any errors
  GLenum errCode = glGetError();
  if (errCode != 0) 
  {
    printf("Texture initialization error. Error code: %d.\n", errCode);
    return -1;
  }
  
  // de-allocate the pixel array -- it is no longer needed
  delete [] pixelsRGBA;

  return 0;
}

// Write a screenshot to the specified filename.
void saveScreenshot(const char * filename)
{
  unsigned char * screenshotData = new unsigned char[windowWidth * windowHeight * 3];
  glReadPixels(0, 0, windowWidth, windowHeight, GL_RGB, GL_UNSIGNED_BYTE, screenshotData);

  ImageIO screenshotImg(windowWidth, windowHeight, 3, screenshotData);

  if (screenshotImg.save(filename, ImageIO::FORMAT_JPEG) == ImageIO::OK)
    cout << "File " << filename << " saved successfully." << endl;
  else cout << "Failed to save file " << filename << '.' << endl;

  delete [] screenshotData;
}

void setTextureUnit(GLint unit, BasicPipelineProgram* pipeline)
{
  glActiveTexture(unit);
  GLint h_textureImage = glGetUniformLocation(pipeline->GetProgramHandle(), "textureImage");
  glUniform1i(h_textureImage, unit - GL_TEXTURE0);
}

void displayFunc()
{
  // This function performs the actual rendering.

  // First, clear the screen.
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Set up the camera position, focus point, and the up vector.
  matrix.SetMatrixMode(OpenGLMatrix::ModelView);
  matrix.LoadIdentity();

  // camera position: position at each point of the spline
  // camera focus point: tangent at each point of the spline
  // camera up vector: normal at each point of the spline
  glm::vec3 finalPos = {cameraPositions[cameraFrame] + 0.3 * cameraNormals[cameraFrame],
  cameraPositions[cameraFrame + 1] + 0.3 * cameraNormals[cameraFrame + 1],
  cameraPositions[cameraFrame + 2] + 0.3 * cameraNormals[cameraFrame + 2]};

  glm::vec3 focusPoint = {finalPos.x + cameraTangents[cameraFrame],
  finalPos.y + cameraTangents[cameraFrame + 1],
  finalPos.z + cameraTangents[cameraFrame + 2]};

  glm::vec3 upVector = {cameraNormals[cameraFrame],
  cameraNormals[cameraFrame + 1],
  cameraNormals[cameraFrame + 2]};

  matrix.LookAt(finalPos.x, finalPos.y, finalPos.z, 
                focusPoint.x, focusPoint.y, focusPoint.z, 
                upVector.x, upVector.y, upVector.y);

  cameraFrame = (cameraFrame + 3) % (cameraPositions.size()); // increment the camera image for the next frame (IT LOOPS)

  // In here, you can do additional modeling on the object, such as performing translations, rotations and scales.
  // matrix.Translate(terrainTranslate[0], terrainTranslate[1], terrainTranslate[2]);
  // matrix.Rotate(terrainRotate[0], 1.0f, 0.0f, 0.0f);
  // matrix.Rotate(terrainRotate[1], 0.0f, 1.0f, 0.0f);
  // matrix.Rotate(terrainRotate[2], 0.0f, 0.0f, 1.0f);
  // matrix.Scale(terrainScale[0], terrainScale[1], terrainScale[2]);

  // Read the current modelview and projection matrices.
  float modelViewMatrix[16];
  matrix.SetMatrixMode(OpenGLMatrix::ModelView);
  matrix.GetMatrix(modelViewMatrix);

  float projectionMatrix[16];
  matrix.SetMatrixMode(OpenGLMatrix::Projection);
  matrix.GetMatrix(projectionMatrix);

  float view[16];
  matrix.GetMatrix(view); // read the view matrix
  glm::mat4 viewMatrix = {
    view[0], view[4], view[8], view[12],
    view[1], view[5], view[9], view[13],
    view[2], view[6], view[10], view[14],
    view[3], view[7], view[11], view[15]
  };

  // Bind the pipeline program.
  // If an incorrect pipeline program is bound, then the modelview and projection matrices
  // will be sent to the wrong pipeline program, causing shader 
  // malfunction (typically, nothing will be shown on the screen).
  railPipelineProgram->Bind(); // binds rail Pipeline

  // Upload the modelview and projection matrices to the GPU.
  railPipelineProgram->SetModelViewMatrix(modelViewMatrix);
  railPipelineProgram->SetProjectionMatrix(projectionMatrix);

  //  -----------------
  // |  PHONG SHADING  |
  //  -----------------
  GLint h_viewLightDirection = glGetUniformLocation(railPipelineProgram->GetProgramHandle(), "viewLightDirection");
  float lightDir[3] = {0, 1, 0}; // sun at noon?
  float viewLightDirection[3];
  glm::vec4 lightDir4 = {lightDir[0], lightDir[1], lightDir[2], 0};
  glm::vec4 temp = viewMatrix * lightDir4; // viewLightDirection = (view * lightDir4).xyz;
  viewLightDirection[0] = temp.x;
  viewLightDirection[1] = temp.y;
  viewLightDirection[2] = temp.z;

  glUniform3fv(h_viewLightDirection, 1, viewLightDirection);

  glm::vec4 La = {1, 1, 1, 1};
  GLint h_La = glGetUniformLocation(railPipelineProgram->GetProgramHandle(), "La");
  glUniform4f(h_La, La.x, La.y, La.z, La.w);

  glm::vec4 Ld = {1, 1, 1, 1};
  GLint h_Ld = glGetUniformLocation(railPipelineProgram->GetProgramHandle(), "Ld");
  glUniform4f(h_Ld, Ld.x, Ld.y, Ld.z, Ld.w);

  glm::vec4 Ls = {1, 1, 1, 1};
  GLint h_Ls = glGetUniformLocation(railPipelineProgram->GetProgramHandle(), "Ls");
  glUniform4f(h_Ls, Ls.x, Ls.y, Ls.z, Ls.w);

  glm::vec4 Ka = {0.15, 0.15, 0.15, 0.15};
  GLint h_Ka = glGetUniformLocation(railPipelineProgram->GetProgramHandle(), "Ka");
  glUniform4f(h_Ka, Ka.x, Ka.y, Ka.z, Ka.w);
  
  glm::vec4 Kd = {0.2, 0.2, 0.2, 0.2};
  GLint h_Kd = glGetUniformLocation(railPipelineProgram->GetProgramHandle(), "Kd");
  glUniform4f(h_Kd, Kd.x, Kd.y, Kd.z, Kd.w);
  
  glm::vec4 Ks = {0.2, 0.2, 0.2, 0.2};
  GLint h_Ks = glGetUniformLocation(railPipelineProgram->GetProgramHandle(), "Ks");
  glUniform4f(h_Ks, Ks.x, Ks.y, Ks.z, Ks.w);

  float alpha = 10;
  GLint h_alpha = glGetUniformLocation(railPipelineProgram->GetProgramHandle(), "alpha");
  glUniform1f(h_alpha, alpha);

  GLint h_normalMatrix = glGetUniformLocation(railPipelineProgram->GetProgramHandle(), "normalMatrix");
  float n[16];
  matrix.SetMatrixMode(OpenGLMatrix::ModelView);
  matrix.GetNormalMatrix(n); // get normal matrix
  GLboolean isRowMajor = GL_FALSE;
  glUniformMatrix4fv(h_normalMatrix, 1, isRowMajor, n);

  // glBindVertexArray(splinesVAO); // Bind the splinesVAO
  // glDrawArrays(GL_LINES, 0, numPointsInSplines); // Render the splinesVAO

  glBindVertexArray(railVAO); // Bind the railVAO
  glDrawArrays(GL_TRIANGLES, 0, railPositions.size() / 3); // Render the railVAO

  groundPipelineProgram->Bind(); // binds ground Pipeline

  // Upload the modelview and projection matrices to the GPU.
  groundPipelineProgram->SetModelViewMatrix(modelViewMatrix);
  groundPipelineProgram->SetProjectionMatrix(projectionMatrix);

  setTextureUnit(GL_TEXTURE0, groundPipelineProgram); // Select the active texture unit
  glBindTexture(GL_TEXTURE_2D, groundTexHandle); // Bind the (moon) texture to the texture unit

  glBindVertexArray(groundVAO); // Bind the groundVAO
  glDrawArrays(GL_TRIANGLES, 0, 6); // Render the groundVAO

  skyPipelineProgram->Bind(); // binds sky box Pppeline

  // Upload the modelview and projection matrices to the GPU.
  skyPipelineProgram->SetModelViewMatrix(modelViewMatrix);
  skyPipelineProgram->SetProjectionMatrix(projectionMatrix);

  setTextureUnit(GL_TEXTURE1, skyPipelineProgram); // Select the active texture unit
  glBindTexture(GL_TEXTURE_2D, skyTexHandle); // Bind the (sky) texture to the texture unit

  glBindVertexArray(skyVAO); // Bind the skyVAO
  glDrawArrays(GL_TRIANGLES, 0, 30); // Render the skyVAO

  // Swap the double-buffers.
  glutSwapBuffers();
}

void idleFunc()
{
  // Do some stuff... 

  // For example, here, you can save the screenshots to disk (to make the animation).
  // USE sprintf() to take screenshots for animation
  char filename[128];
  sprintf(filename, "screenshot%03d.jpg", screenshotCount);
  saveScreenshot(filename);
  screenshotCount++;

  // Send the signal that we should call displayFunc.
  glutPostRedisplay();
}

void reshapeFunc(int w, int h)
{
  glViewport(0, 0, w, h);

  // When the window has been resized, we need to re-set our projection matrix.
  matrix.SetMatrixMode(OpenGLMatrix::Projection);
  matrix.LoadIdentity();
  // You need to be careful about setting the zNear and zFar. 
  // Anything closer than zNear, or further than zFar, will be culled.
  const float zNear = 0.1f;
  const float zFar = 10000.0f;
  const float humanFieldOfView = 60.0f;
  matrix.Perspective(humanFieldOfView, 1.0f * w / h, zNear, zFar);
}

void mouseMotionDragFunc(int x, int y)
{
  // Mouse has moved, and one of the mouse buttons is pressed (dragging).

  // the change in mouse position since the last invocation of this function
  int mousePosDelta[2] = { x - mousePos[0], y - mousePos[1] };

  switch (controlState)
  {
    // translate the terrain
    case TRANSLATE:
      if (leftMouseButton)
      {
        // control x,y translation via the left mouse button
        terrainTranslate[0] += mousePosDelta[0] * 0.05f;
        terrainTranslate[1] -= mousePosDelta[1] * 0.05f;
      }
      if (middleMouseButton)
      {
        // control z translation via the middle mouse button
        terrainTranslate[2] += mousePosDelta[1] * 0.05f;
      }
      break;

    // rotate the terrain
    case ROTATE:
      if (leftMouseButton)
      {
        // control x,y rotation via the left mouse button
        terrainRotate[0] += mousePosDelta[1];
        terrainRotate[1] += mousePosDelta[0];
      }
      if (middleMouseButton)
      {
        // control z rotation via the middle mouse button
        terrainRotate[2] += mousePosDelta[1];
      }
      break;

    // scale the terrain
    case SCALE:
      if (leftMouseButton)
      {
        // control x,y scaling via the left mouse button
        terrainScale[0] *= 1.0f + mousePosDelta[0] * 0.01f;
        terrainScale[1] *= 1.0f - mousePosDelta[1] * 0.01f;
      }
      if (middleMouseButton)
      {
        // control z scaling via the middle mouse button
        terrainScale[2] *= 1.0f - mousePosDelta[1] * 0.01f;
      }
      break;
  }

  // store the new mouse position
  mousePos[0] = x;
  mousePos[1] = y;
}

void mouseMotionFunc(int x, int y)
{
  // Mouse has moved.
  // Store the new mouse position.
  mousePos[0] = x;
  mousePos[1] = y;
}

void mouseButtonFunc(int button, int state, int x, int y)
{
  // A mouse button has has been pressed or depressed.

  // Keep track of the mouse button state, in leftMouseButton, middleMouseButton, rightMouseButton variables.
  switch (button)
  {
    case GLUT_LEFT_BUTTON:
      leftMouseButton = (state == GLUT_DOWN);
    break;

    case GLUT_MIDDLE_BUTTON:
      middleMouseButton = (state == GLUT_DOWN);
    break;

    case GLUT_RIGHT_BUTTON:
      rightMouseButton = (state == GLUT_DOWN);
    break;
  }

  // Keep track of whether CTRL and SHIFT keys are pressed.
  switch (glutGetModifiers())
  {
    case GLUT_ACTIVE_CTRL:
      controlState = TRANSLATE;
    break;

    case GLUT_ACTIVE_SHIFT:
      controlState = SCALE;
    break;

    // If CTRL and SHIFT are not pressed, we are in rotate mode.
    default:
      controlState = ROTATE;
    break;
  }

  // Store the new mouse position.
  mousePos[0] = x;
  mousePos[1] = y;
}

void keyboardFunc(unsigned char key, int x, int y)
{
  switch (key)
  {
    case 27: // ESC key
      exit(0); // exit the program
    break;

    case ' ':
      cout << "You pressed the spacebar." << endl;
    break;

    case 'x':
      // Take a screenshot.
      saveScreenshot("screenshot.jpg");
    break;

    case 't':
      controlState = TRANSLATE;
    break;
  }
}

void createRailVBOandVAO(vector<float>& positions, vector<float>& normals, int vertices, GLuint& passedVBO, GLuint& passedVAO, BasicPipelineProgram* pipeline)
{
  float* pPosition = positions.data(); // turning positions vector into a float array
  float* pNormal = normals.data(); // turning colors vector into a float array

  // 1. Create the VBO
  glGenBuffers(1, &passedVBO);
  glBindBuffer(GL_ARRAY_BUFFER, passedVBO);

  // 2. Allocate an empty VBO of the correct size to hold positions and colors.
  const int numBytesInPositions = vertices * 3 * sizeof(float);
  const int numBytesInNormals = vertices * 4 * sizeof(float);
  glBufferData(GL_ARRAY_BUFFER, numBytesInPositions + numBytesInNormals, nullptr, GL_STATIC_DRAW);

  // 3. Write the position and color data into the VBO.
  glBufferSubData(GL_ARRAY_BUFFER, 0, numBytesInPositions, pPosition); // The VBO starts with positions.
  glBufferSubData(GL_ARRAY_BUFFER, numBytesInPositions, numBytesInNormals, pNormal); // The normals are written after the positions.

  // 4. Create the VAO
  glGenVertexArrays(1, &passedVAO);
  glBindVertexArray(passedVAO);
  glBindBuffer(GL_ARRAY_BUFFER, passedVBO); // The VBO that we bind here will be used in the glVertexAttribPointer calls below. If we forget to bind the VBO here, the program will malfunction.

  // 5. Set up the relationship between the "position" shader variable and the VAO.
  const GLuint locationOfPosition = glGetAttribLocation(pipeline->GetProgramHandle(), "position"); // Obtain a handle to the shader variable "position".
  glEnableVertexAttribArray(locationOfPosition); // Must always enable the vertex attribute. By default, it is disabled.
  const int stride = 0; // Stride is 0, i.e., data is tightly packed in the VBO.
  const GLboolean normalized = GL_FALSE; // Normalization is off.
  glVertexAttribPointer(locationOfPosition, 3, GL_FLOAT, normalized, stride, (const void *)0); // The shader variable "position" receives its data from the currently bound VBO (i.e., vertexPositionAndColorVBO), starting from offset 0 in the VBO. There are 3 float entries per vertex in the VBO (i.e., x,y,z coordinates). 

  // Set up the relationship between the "normal" shader variable and the VAO.
  const GLuint locationOfNormal = glGetAttribLocation(pipeline->GetProgramHandle(), "normal"); // Obtain a handle to the shader variable "normal".
  glEnableVertexAttribArray(locationOfNormal); // Must always enable the vertex attribute. By default, it is disabled.
  glVertexAttribPointer(locationOfNormal, 4, GL_FLOAT, normalized, stride, (const void *)(unsigned long)numBytesInPositions); // The shader variable "color" receives its data from the currently bound VBO (i.e., vertexPositionAndColorVBO), starting from offset "numBytesInPositions" in the VBO. There are 4 float entries per vertex in the VBO (i.e., r,g,b,a channels). 
  // glVertexAttribPointer(locationOfNormal, 3, GL_FLOAT, normalized, stride, (const void *)(unsigned long)numBytesInPositions); // The shader variable "color" receives its data from the currently bound VBO (i.e., vertexPositionAndColorVBO), starting from offset "numBytesInPositions" in the VBO. There are 4 float entries per vertex in the VBO (i.e., r,g,b,a channels). 
}

void createGroundVBOandVAO(vector<float>& posAndTexCoords, int vertices, GLuint& passedVBO, GLuint& passedVAO, BasicPipelineProgram* pipeline)
{
  float* pData = posAndTexCoords.data(); // turning positions vector into a float array

  // 1. Create the VBO
  glGenBuffers(1, &passedVBO);
  glBindBuffer(GL_ARRAY_BUFFER, passedVBO);

  // 2. Allocate an empty VBO of the correct size to hold positions and tex coordinates.
  const int numBytes = posAndTexCoords.size() * sizeof(float);
  glBufferData(GL_ARRAY_BUFFER, numBytes, nullptr, GL_STATIC_DRAW);

  // 3. Write the position and tex coordinate data into the VBO.
  glBufferSubData(GL_ARRAY_BUFFER, 0, numBytes, pData); // The VBO starts with positions.
  
  // 4. Create the VAO
  glGenVertexArrays(1, &passedVAO);
  glBindVertexArray(passedVAO);
  glBindBuffer(GL_ARRAY_BUFFER, passedVBO); // The VBO that we bind here will be used in the glVertexAttribPointer calls below. If we forget to bind the VBO here, the program will malfunction.

  // 5. Set up the relationship between the "position" shader variable and the VAO.
  const GLuint locationOfPosition = glGetAttribLocation(pipeline->GetProgramHandle(), "position"); // Obtain a handle to the shader variable "position".
  glEnableVertexAttribArray(locationOfPosition); // Must always enable the vertex attribute. By default, it is disabled.
  const int stride = 5 * sizeof(float); // Stride is 0, i.e., data is tightly packed in the VBO.
  const GLboolean normalized = GL_FALSE; // Normalization is off.
  glVertexAttribPointer(locationOfPosition, 3, GL_FLOAT, normalized, stride, (const void *)0); // The shader variable "position" receives its data from the currently bound VBO (i.e., vertexPositionAndColorVBO), starting from offset 0 in the VBO. There are 3 float entries per vertex in the VBO (i.e., x,y,z coordinates). 

  // Set up the relationship between the "texCoord" shader variable and the VAO.
  const GLuint locationOfTexCoord = glGetAttribLocation(pipeline->GetProgramHandle(), "texCoord"); // Obtain a handle to the shader variable "texCoord".
  glEnableVertexAttribArray(locationOfTexCoord); // Must always enable the vertex attribute. By default, it is disabled.
  glVertexAttribPointer(locationOfTexCoord, 2, GL_FLOAT, normalized, stride, (const void *)(3 * sizeof(float))); // The shader variable "texCoord" receives its data from the currently bound VBO (i.e., vertexPositionAndColorVBO), starting from offset "numBytesInPositions" in the VBO. There are 4 float entries per vertex in the VBO (i.e., r,g,b,a channels). 
}

void pushPosAndColor(vector<float>& positions, vector<float>& colors, glm::vec3 XYZ, glm::vec4 RGBA)
{
  // push the location of the vertex
  positions.push_back(XYZ[0]); // x
  positions.push_back(XYZ[1]); // y
  positions.push_back(XYZ[2]); // z

  // push the color of the vertex
  colors.push_back(RGBA[0]); // r
  colors.push_back(RGBA[1]); // g
  colors.push_back(RGBA[2]); // b
  colors.push_back(RGBA[3]); // a
}

void pushValueToVector(vector<float>& vector, glm::vec3 XYZ)
{
  vector.push_back(XYZ[0]);
  vector.push_back(XYZ[1]);
  vector.push_back(XYZ[2]);
}

void pushValueToVector(vector<float>& vector, glm::vec2 XY)
{
  vector.push_back(XY[0]);
  vector.push_back(XY[1]);
}

void createTriangle(vector<float>& posVector, vector<float>& colorVector, glm::vec3 vertex1, glm::vec3 vertex2, glm::vec3 vertex3, glm::vec4 rgba)
{
  // push all positions and colors for each point in the triangle
  pushPosAndColor(posVector, colorVector, vertex1, rgba);
  pushPosAndColor(posVector, colorVector, vertex2, rgba);
  pushPosAndColor(posVector, colorVector, vertex3, rgba);
}

void initScene(int argc, char *argv[])
{
  // Set the background color.
  glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // Black color.

  // Enable z-buffering (i.e., hidden surface removal using the z-buffer algorithm).
  glEnable(GL_DEPTH_TEST);

  // Create and bind the pipeline program. This operation must be performed BEFORE we initialize any VAOs.
  railPipelineProgram = new BasicPipelineProgram;
  int ret = railPipelineProgram->Init(shaderBasePath, "basic.vertexShader.glsl", "basic.fragmentShader.glsl");
  if (ret != 0) 
  { 
    abort();
  }
  railPipelineProgram->Bind();

  vector<float> positions;
  vector<float> colors; // all lines will be white

  const float s = 0.5f; // s value for the basis matrix

  // create basis matrix
  glm::mat4x4 basisMatrix = {
    -s, 2*s, -s, 0,
    2-s, s-3, 0, 1,
    s-2, 3-2*s, s, 0,
    s, -s, 0, 0
  };

  Spline currSpline;
  Point pnt1, pnt2, pnt3, pnt4;
  glm::vec4 currUVec; // create a vector for the u value
  glm::mat3x4 basisAndControl; // matrix that will hold basis * control

  glm::vec4 currCameraU;   // used to find the tangent
  glm::vec3 T0, N0, B0; // track the last point's T, N, and B vectors
  glm::vec3 T1, N1, B1; // track the current point's T, N, and B vectors

  // SETTING CAMERA POSITION, NORMALS, TANGENTS, AND BINORMALS
  for(int i = 0; i < numSplines; i++)
  {
    currSpline = splines[i]; // get the current spline

    // loop through number of control points in spline
    for(int j = 1; j <= currSpline.numControlPoints - 3; j++)
    { 
      // get the four points being dealt with now
      pnt1 = currSpline.points[j - 1];
      pnt2 = currSpline.points[j];
      pnt3 = currSpline.points[j + 1];
      pnt4 = currSpline.points[j + 2];

      // create the control matrix
      glm::mat3x4 controlMatrix = {
        pnt1.x, pnt2.x, pnt3.x, pnt4.x,
        pnt1.y, pnt2.y, pnt3.y, pnt4.y,
        pnt1.z, pnt2.z, pnt3.z, pnt4.z,
      };

      basisAndControl = basisMatrix * controlMatrix;

      if(cameraPositions.empty())
      {
        // CAMERA STUFF (T0, N0, B0)
        currCameraU = {3 * 0.0f * 0.0f, 2 * 0.0f, 1, 0}; // u = 0 to start
        glm::vec3 V = {0, 0, 1}; // random vector to start
        T0 = glm::normalize(glm::vec3(currCameraU * basisAndControl));   
        N0 = glm::normalize(glm::cross(T0, V));
        B0 = glm::normalize(glm::cross(T0, N0));
      }

      // push second point
      pushPosAndColor(positions, colors, {pnt2.x, pnt2.y, pnt2.z}, {1, 1, 1, 1});

      // loop u from 0.0f to 1.0f
      for(float u = 0.0f; u < 1.01f; u += 0.01f)
      {
        // u vec = {u^3, u^2, u, 1}
        currUVec = {u * u * u, u * u, u, 1};

        // location of this point on the spline based on u
        glm::vec3 loc = currUVec * basisAndControl;

        // push each vertex twice to make the line
        pushPosAndColor(positions, colors, loc, {1, 1, 1, 1});
        pushPosAndColor(positions, colors, loc, {1, 1, 1, 1});

        // CAMERA STUFF
        currCameraU = {3 * u * u, 2 * u, 1, 0};
        T1 = glm::normalize(glm::vec3(currCameraU * basisAndControl));
        N1 = glm::normalize(glm::cross(B0, T1));
        B1 = glm::normalize(glm::cross(T1, N1));

        // push camera position, tangent, and up vector
        pushValueToVector(cameraPositions, loc);
        pushValueToVector(cameraTangents, T1);
        pushValueToVector(cameraNormals, N1);
        pushValueToVector(cameraBinormals, B1);

        // at the end of this iteration, T1, N1, and B1 become T0, N0, and B0
        T0 = T1;
        N0 = N1;
        B0 = B1;
      }

      // push third point
      pushPosAndColor(positions, colors, {pnt3.x, pnt3.y, pnt3.z}, {1, 1, 1, 1});
    }
  }

  // CREATION OF THE RAIL(S)
  glm::vec3 P0, P1;
  glm::vec3 V0, V1, V2, V3, V4, V5, V6, V7; // first set of vertices
  glm::vec3 V8, V9, V10, V11, V12, V13, V14, V15; // second set of verticess
  float alpha = 0.05f; // alpha value for the rail
  float beta = 0.1f; // space between the bars

  // CREATES 2 T-SHAPE CROSS-SECTIONS
  for(int p = 0; p < cameraPositions.size() - 3; p += 3)
  {
    P0 = {cameraPositions[p], cameraPositions[p + 1], cameraPositions[p + 2]};
    T0 = {cameraTangents[p], cameraTangents[p + 1], cameraTangents[p + 2]};
    N0 = {cameraNormals[p], cameraNormals[p + 1], cameraNormals[p + 2]};
    B0 = {cameraBinormals[p], cameraBinormals[p + 1], cameraBinormals[p + 2]};

    glm::vec3 offset = {beta * B0.x, beta * B0.y, beta * B0.z};
    P0 += offset;

    P1 = {cameraPositions[p + 3], cameraPositions[p + 4], cameraPositions[p + 5]};
    T1 = {cameraTangents[p + 3], cameraTangents[p + 4], cameraTangents[p + 5]};
    N1 = {cameraNormals[p + 3], cameraNormals[p + 4], cameraNormals[p + 5]};
    B1 = {cameraBinormals[p + 3], cameraBinormals[p + 4], cameraBinormals[p + 5]};

    offset = {beta * B1.x, beta * B1.y, beta * B1.z};
    P1 += offset;

    V0 = P0 - (alpha * N0) + (alpha / 4 * B0);
    V1 = P0 + (alpha / 4 * N0) + (alpha / 4 * B0);
    V2 = P0 + (alpha / 4 * N0) + (alpha * B0);
    V3 = P0 + (alpha * (N0 + B0));
    V4 = P0 + (alpha * (N0 - B0));
    V5 = P0 + (alpha / 4 * N0) - (alpha * B0);
    V6 = P0 + (alpha / 4 * N0) - (alpha / 4 * B0);
    V7 = P0 - (alpha * N0) - (alpha / 4 * B0);

    V8 = P1 - (alpha * N1) + (alpha / 4 * B1);
    V9 = P1 + (alpha / 4 * N1) + (alpha / 4 * B1);
    V10 = P1 + (alpha / 4 * N1) + (alpha * B1);
    V11 = P1 + (alpha * (N1 + B1));
    V12 = P1 + (alpha * (N1 - B1));
    V13 = P1 + (alpha / 4 * N1) - (alpha * B1);
    V14 = P1 + (alpha / 2 * N1) - (alpha / 4 * B1);
    V15 = P1 - (alpha * N1) - (alpha / 4 * B1);

    createTriangle(railPositions, railColors, V0, V1, V9, {B0.x, B0.y, B0.z, 1});
    createTriangle(railPositions, railColors, V0, V8, V9, {B0.x, B0.y, B0.z, 1});

    createTriangle(railPositions, railColors, V1, V2, V10, {-N0.x, -N0.y, -N0.z, 1});
    createTriangle(railPositions, railColors, V1, V9, V10, {-N0.x, -N0.y, -N0.z, 1});

    createTriangle(railPositions, railColors, V2, V3, V11, {B0.x, B0.y, B0.z, 1});
    createTriangle(railPositions, railColors, V2, V10, V11, {B0.x, B0.y, B0.z, 1});

    createTriangle(railPositions, railColors, V3, V4, V12, {N0.x, N0.y, N0.z, 1});
    createTriangle(railPositions, railColors, V3, V11, V12, {N0.x, N0.y, N0.z, 1});

    createTriangle(railPositions, railColors, V4, V5, V13, {-B0.x, -B0.y, -B0.z, 1});
    createTriangle(railPositions, railColors, V4, V12, V13, {-B0.x, -B0.y, -B0.z, 1});

    createTriangle(railPositions, railColors, V5, V6, V14, {-N0.x, -N0.y, -N0.z, 1});
    createTriangle(railPositions, railColors, V5, V13, V14, {-N0.x, -N0.y, -N0.z, 1});

    createTriangle(railPositions, railColors, V6, V7, V15, {-B0.x, -B0.y, -B0.z, 1});
    createTriangle(railPositions, railColors, V6, V14, V15, {-B0.x, -B0.y, -B0.z, 1});

    createTriangle(railPositions, railColors, V7, V0, V8, {N0.x, N0.y, N0.z, 1});
    createTriangle(railPositions, railColors, V7, V15, V8, {N0.x, N0.y, N0.z, 1});

    offset = {-2 * beta * B0.x, -2 * beta * B0.y, -2 * beta * B0.z};
    P0 += offset;

    offset = {-2 * beta * B1.x, -2 * beta * B1.y, -2 * beta * B1.z};
    P1 += offset;

    V0 = P0 - (alpha * N0) + (alpha / 4 * B0);
    V1 = P0 + (alpha / 4 * N0) + (alpha / 4 * B0);
    V2 = P0 + (alpha / 4 * N0) + (alpha * B0);
    V3 = P0 + (alpha * (N0 + B0));
    V4 = P0 + (alpha * (N0 - B0));
    V5 = P0 + (alpha / 4 * N0) - (alpha * B0);
    V6 = P0 + (alpha / 4 * N0) - (alpha / 4 * B0);
    V7 = P0 - (alpha * N0) - (alpha / 4 * B0);

    V8 = P1 - (alpha * N1) + (alpha / 4 * B1);
    V9 = P1 + (alpha / 4 * N1) + (alpha / 4 * B1);
    V10 = P1 + (alpha / 4 * N1) + (alpha * B1);
    V11 = P1 + (alpha * (N1 + B1));
    V12 = P1 + (alpha * (N1 - B1));
    V13 = P1 + (alpha / 4 * N1) - (alpha * B1);
    V14 = P1 + (alpha / 4 * N1) - (alpha / 4 * B1);
    V15 = P1 - (alpha * N1) - (alpha / 4 * B1);

    createTriangle(railPositions, railColors, V0, V1, V9, {B0.x, B0.y, B0.z, 1});
    createTriangle(railPositions, railColors, V0, V8, V9, {B0.x, B0.y, B0.z, 1});

    createTriangle(railPositions, railColors, V1, V2, V10, {-N0.x, -N0.y, -N0.z, 1});
    createTriangle(railPositions, railColors, V1, V9, V10, {-N0.x, -N0.y, -N0.z, 1});

    createTriangle(railPositions, railColors, V2, V3, V11, {B0.x, B0.y, B0.z, 1});
    createTriangle(railPositions, railColors, V2, V10, V11, {B0.x, B0.y, B0.z, 1});

    createTriangle(railPositions, railColors, V3, V4, V12, {N0.x, N0.y, N0.z, 1});
    createTriangle(railPositions, railColors, V3, V11, V12, {N0.x, N0.y, N0.z, 1});

    createTriangle(railPositions, railColors, V4, V5, V13, {-B0.x, -B0.y, -B0.z, 1});
    createTriangle(railPositions, railColors, V4, V12, V13, {-B0.x, -B0.y, -B0.z, 1});

    createTriangle(railPositions, railColors, V5, V6, V14, {-N0.x, -N0.y, -N0.z, 1});
    createTriangle(railPositions, railColors, V5, V13, V14, {-N0.x, -N0.y, -N0.z, 1});

    createTriangle(railPositions, railColors, V6, V7, V15, {-B0.x, -B0.y, -B0.z, 1});
    createTriangle(railPositions, railColors, V6, V14, V15, {-B0.x, -B0.y, -B0.z, 1});

    createTriangle(railPositions, railColors, V7, V0, V8, {N0.x, N0.y, N0.z, 1});
    createTriangle(railPositions, railColors, V7, V15, V8, {N0.x, N0.y, N0.z, 1});
  } 

  // // create the spline VBO & VAO
  // numPointsInSplines = positions.size() / 3; // number of points pushed
  // createRailVBOandVAO(positions, colors, numPointsInSplines, splinesVBO, splinesVAO, railPipelineProgram);

  // create the rail VBO & VAO
  createRailVBOandVAO(railPositions, railColors, railPositions.size() / 3, railVBO, railVAO, railPipelineProgram);

  // !---GROUND PLANE---!
  // Create and bind the pipeline program. This operation must be performed BEFORE we initialize any VAOs.
  groundPipelineProgram = new BasicPipelineProgram;
  ret = groundPipelineProgram->Init(shaderBasePath, "basic.textureVertexShader.glsl", "basic.textureFragmentShader.glsl");
  if (ret != 0) 
  { 
    abort();
  }
  groundPipelineProgram->Bind();

  // read the moon texture
  ImageIO * imageIO = new ImageIO();
  string str = "moon.jpg";
  const char* imageFilename = str.c_str();
  if (imageIO->loadJPEG(imageFilename) != ImageIO::OK)
  {
    cout << "Error reading image " << imageFilename << "." << endl;
    exit(EXIT_FAILURE); 
  }

  // initialize the texture
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, imageIO->getWidth(), imageIO->getHeight(), 0, GL_RGB, GL_UNSIGNED_BYTE, imageIO->getPixels());
  
  // specify the texture parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  glGenTextures(1, &groundTexHandle);

  int code = initTexture(imageFilename, groundTexHandle);
  if(code != 0)
  {
    cout << "Error loading the texture image: " << imageFilename << endl;
    exit(EXIT_FAILURE);
  }

  // CREATE GROUND PLANE OUT OF 2 TRIANGLES
  int groundSize = 500;
  int groundHeight = 100;

  // triangle 1 -- positions & texture coordinates
  pushValueToVector(groundPositions, {-groundSize, -groundSize, -groundHeight}); // bottom left
  pushValueToVector(groundPositions, {0, 0}); // bottom left

  pushValueToVector(groundPositions, {groundSize, -groundSize, -groundHeight}); // bottom right
  pushValueToVector(groundPositions, {0, 1}); // bottom right

  pushValueToVector(groundPositions, {groundSize, groundSize, -groundHeight}); // top right
  pushValueToVector(groundPositions, {1, 1}); // top right

  // triangle 2 -- positions & texture coordinates
  pushValueToVector(groundPositions, {-groundSize, -groundSize, -groundHeight}); // bottom left
  pushValueToVector(groundPositions, {0, 0}); // bottom left

  pushValueToVector(groundPositions, {-groundSize, groundSize, -groundHeight}); // top left
  pushValueToVector(groundPositions, {1, 0}); // top left

  pushValueToVector(groundPositions, {groundSize, groundSize, -groundHeight}); // top right
  pushValueToVector(groundPositions, {1, 1}); // top right

  createGroundVBOandVAO(groundPositions, 6, groundVBO, groundVAO, groundPipelineProgram);

  skyPipelineProgram = new BasicPipelineProgram;
  ret = skyPipelineProgram->Init(shaderBasePath, "basic.textureVertexShader.glsl", "basic.textureFragmentShader.glsl");
  if (ret != 0) 
  { 
    abort();
  }
  skyPipelineProgram->Bind();

  // read the moon texture
  imageIO = new ImageIO();
  str = "starry_sky_texture.jpg";
  const char* fileName = str.c_str();
  if (imageIO->loadJPEG(fileName) != ImageIO::OK)
  {
    cout << "Error reading image " << fileName << "." << endl;
    exit(EXIT_FAILURE); 
  }

  // specify the texture parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  glGenTextures(1, &skyTexHandle);

  code = initTexture(fileName, skyTexHandle);
  if(code != 0)
  {
    cout << "Error loading the texture image: " << fileName << endl;
    exit(EXIT_FAILURE);
  }

  // CREATE SKY BOX (with only 5 sides) OUT OF 10 TRIANGLES
  // TOP FACE
  pushValueToVector(skyPositions, {-groundSize, -groundSize, groundHeight}); // bottom left
  pushValueToVector(skyPositions, {0, 0}); // bottom left

  pushValueToVector(skyPositions, {groundSize, -groundSize, groundHeight}); // bottom right
  pushValueToVector(skyPositions, {0, 1}); // bottom right

  pushValueToVector(skyPositions, {groundSize, groundSize, groundHeight}); // top right
  pushValueToVector(skyPositions, {1, 1}); // top right

  pushValueToVector(skyPositions, {-groundSize, -groundSize, groundHeight}); // bottom left
  pushValueToVector(skyPositions, {0, 0}); // bottom left

  pushValueToVector(skyPositions, {-groundSize, groundSize, groundHeight}); // top left
  pushValueToVector(skyPositions, {1, 0}); // top left

  pushValueToVector(skyPositions, {groundSize, groundSize, groundHeight}); // top right
  pushValueToVector(skyPositions, {1, 1}); // top right

  // FRONT FACE
  pushValueToVector(skyPositions, {groundSize, -groundSize, -groundHeight}); // bottom left
  pushValueToVector(skyPositions, {0, 0}); // bottom left

  pushValueToVector(skyPositions, {-groundSize, -groundSize, -groundHeight}); // bottom right
  pushValueToVector(skyPositions, {0, 1}); // bottom right

  pushValueToVector(skyPositions, {-groundSize, -groundSize, groundHeight}); // top right
  pushValueToVector(skyPositions, {1, 1}); // top right

  pushValueToVector(skyPositions, {groundSize, -groundSize, -groundHeight}); // bottom left
  pushValueToVector(skyPositions, {0, 0}); // bottom left

  pushValueToVector(skyPositions, {groundSize, -groundSize, groundHeight}); // top left
  pushValueToVector(skyPositions, {1, 0}); // top left

  pushValueToVector(skyPositions, {-groundSize, -groundSize, groundHeight}); // top right
  pushValueToVector(skyPositions, {1, 1}); // top right

  // BACK FACE
  pushValueToVector(skyPositions, {-groundSize, groundSize, -groundHeight}); // bottom left
  pushValueToVector(skyPositions, {0, 0}); // bottom left

  pushValueToVector(skyPositions, {groundSize, groundSize, -groundHeight}); // bottom right
  pushValueToVector(skyPositions, {0, 1}); // bottom right

  pushValueToVector(skyPositions, {groundSize, groundSize, groundHeight}); // top right
  pushValueToVector(skyPositions, {1, 1}); // top right

  pushValueToVector(skyPositions, {-groundSize, groundSize, -groundHeight}); // bottom left
  pushValueToVector(skyPositions, {0, 0}); // bottom left

  pushValueToVector(skyPositions, {-groundSize, groundSize, groundHeight}); // top left
  pushValueToVector(skyPositions, {1, 0}); // top left

  pushValueToVector(skyPositions, {groundSize, groundSize, groundHeight}); // top right
  pushValueToVector(skyPositions, {1, 1}); // top right

  // LEFT FACE
  pushValueToVector(skyPositions, {-groundSize, groundSize, -groundHeight}); // bottom left
  pushValueToVector(skyPositions, {0, 0}); // bottom left

  pushValueToVector(skyPositions, {-groundSize, -groundSize, -groundHeight}); // bottom right
  pushValueToVector(skyPositions, {0, 1}); // bottom right

  pushValueToVector(skyPositions, {-groundSize, -groundSize, groundHeight}); // top right
  pushValueToVector(skyPositions, {1, 1}); // top right

  pushValueToVector(skyPositions, {-groundSize, groundSize, -groundHeight}); // bottom left
  pushValueToVector(skyPositions, {0, 0}); // bottom left

  pushValueToVector(skyPositions, {-groundSize, groundSize, groundHeight}); // top left
  pushValueToVector(skyPositions, {1, 0}); // top left

  pushValueToVector(skyPositions, {-groundSize, -groundSize, groundHeight}); // top right
  pushValueToVector(skyPositions, {1, 1}); // top right

  // RIGHT FACE
  pushValueToVector(skyPositions, {groundSize, groundSize, -groundHeight}); // bottom left
  pushValueToVector(skyPositions, {0, 0}); // bottom left

  pushValueToVector(skyPositions, {groundSize, -groundSize, -groundHeight}); // bottom right
  pushValueToVector(skyPositions, {0, 1}); // bottom right

  pushValueToVector(skyPositions, {groundSize, -groundSize, groundHeight}); // top right
  pushValueToVector(skyPositions, {1, 1}); // top right

  pushValueToVector(skyPositions, {groundSize, groundSize, -groundHeight}); // bottom left
  pushValueToVector(skyPositions, {0, 0}); // bottom left

  pushValueToVector(skyPositions, {groundSize, groundSize, groundHeight}); // top left
  pushValueToVector(skyPositions, {1, 0}); // top left

  pushValueToVector(skyPositions, {groundSize, -groundSize, groundHeight}); // top right
  pushValueToVector(skyPositions, {1, 1}); // top right

  // Create the VBO and VAO for the sky box
  createGroundVBOandVAO(skyPositions, 30, skyVBO, skyVAO, skyPipelineProgram);

  // Check for any OpenGL errors.
  std::cout << "GL error: " << glGetError() << std::endl;
}

// Note for Windows/MS Visual Studio:
// You should set argv[1] to track.txt.
// To do this, on the "Solution Explorer",
// right click your project, choose "Properties",
// go to "Configuration Properties", click "Debug",
// then type your track file name for the "Command Arguments".
// You can also repeat this process for the "Release" configuration.
int main(int argc, char *argv[])
{
  if (argc<2)
  {  
    printf ("usage: %s <trackfile>\n", argv[0]);
    exit(0);
  }

  cout << "Initializing GLUT..." << endl;
  glutInit(&argc,argv);

  cout << "Initializing OpenGL..." << endl;

  #ifdef __APPLE__
    glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
  #else
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
  #endif

  glutInitWindowSize(windowWidth, windowHeight);
  glutInitWindowPosition(0, 0);  
  glutCreateWindow(windowTitle);

  cout << "OpenGL Version: " << glGetString(GL_VERSION) << endl;
  cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << endl;
  cout << "Shading Language Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;

  #ifdef __APPLE__
    // This is needed on recent Mac OS X versions to correctly display the window.
    glutReshapeWindow(windowWidth - 1, windowHeight - 1);
  #endif

  // tells glut to use a particular display function to redraw 
  glutDisplayFunc(displayFunc);
  // perform animation inside idleFunc
  glutIdleFunc(idleFunc);
  // callback for mouse drags
  glutMotionFunc(mouseMotionDragFunc);
  // callback for idle mouse movement
  glutPassiveMotionFunc(mouseMotionFunc);
  // callback for mouse button changes
  glutMouseFunc(mouseButtonFunc);
  // callback for resizing the window
  glutReshapeFunc(reshapeFunc);
  // callback for pressing the keys on the keyboard
  glutKeyboardFunc(keyboardFunc);

  // init glew
  #ifdef __APPLE__
    // nothing is needed on Apple
  #else
    // Windows, Linux
    GLint result = glewInit();
    if (result != GLEW_OK)
    {
      cout << "error: " << glewGetErrorString(result) << endl;
      exit(EXIT_FAILURE);
    }
  #endif

  // load the splines from the provided filename
  loadSplines(argv[1]);

  printf("Loaded %d spline(s).\n", numSplines);
  for(int i=0; i<numSplines; i++)
  {
    printf("Num control points in spline %d: %d.\n", i, splines[i].numControlPoints);
  }

  // Perform the initialization.
  initScene(argc, argv);

  // Sink forever into the GLUT loop.
  glutMainLoop();
}