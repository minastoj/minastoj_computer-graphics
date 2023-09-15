/*
  CSCI 420 Computer Graphics, USC
  Assignment 1: Height Fields with Shaders.
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

// create states that change based on which number is pressed
typedef enum { POINTS, LINES, TRIANGLES, SMOOTH } RENDER_STATE;
RENDER_STATE renderState = POINTS;

int pointsNumVertices = 0;
int linesNumVertices = 0;
int trianglesNumVertices = 0;

// Transformations of the terrain.
float terrainRotate[3] = { 0.0f, 0.0f, 0.0f }; 
// terrainRotate[0] gives the rotation around x-axis (in degrees)
// terrainRotate[1] gives the rotation around y-axis (in degrees)
// terrainRotate[2] gives the rotation around z-axis (in degrees)
float terrainTranslate[3] = { 0.0f, 0.0f, 0.0f };
float terrainScale[3] = { 1.0f, 1.0f, 1.0f };

const float scale = 0.15f;

// Width and height of the OpenGL window, in pixels.
int windowWidth = 1280;
int windowHeight = 720;
char windowTitle[512] = "CSCI 420 homework I";

// Stores the image loaded from disk.
ImageIO * heightmapImage;

// VBOs
GLuint pointsVertexPositionAndColorVBO;
GLuint linesVertexPositionAndColorVBO;
GLuint trianglesVertexPositionAndColorVBO;

// VBOs for smoothing
GLuint smoothCenterVBO;
GLuint smoothLeftNeighborVBO;
GLuint smoothRightNeighborVBO;
GLuint smoothUpNeighborVBO;
GLuint smoothDownNeighborVBO;

// VAOs
GLuint pointsVAO;
GLuint linesVAO;
GLuint trianglesVAO;

// VAO for smoothing
GLuint smoothVAO;

GLint smoothing; // tracks if we smooth or not
int screenshotCount = 0;

// CSCI 420 helper classes.
OpenGLMatrix matrix;
BasicPipelineProgram * pipelineProgram;

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

void displayFunc()
{
  // This function performs the actual rendering.

  // First, clear the screen.
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Set up the camera position, focus point, and the up vector.
  matrix.SetMatrixMode(OpenGLMatrix::ModelView);
  matrix.LoadIdentity();
  matrix.LookAt(0.0, 300.0, 5.0, 
                0.0, 0.0, 0.0, 
                0.0, -1.0, 0.0);

  // In here, you can do additional modeling on the object, such as performing translations, rotations and scales.
  matrix.Translate(terrainTranslate[0], terrainTranslate[1], terrainTranslate[2]);
  matrix.Rotate(terrainRotate[0], 1.0f, 0.0f, 0.0f);
  matrix.Rotate(terrainRotate[1], 0.0f, 1.0f, 0.0f);
  matrix.Rotate(terrainRotate[2], 0.0f, 0.0f, 1.0f);
  matrix.Scale(terrainScale[0], terrainScale[1], terrainScale[2]);

  // Read the current modelview and projection matrices.
  float modelViewMatrix[16];
  matrix.SetMatrixMode(OpenGLMatrix::ModelView);
  matrix.GetMatrix(modelViewMatrix);

  float projectionMatrix[16];
  matrix.SetMatrixMode(OpenGLMatrix::Projection);
  matrix.GetMatrix(projectionMatrix);

  // Bind the pipeline program.
  // If an incorrect pipeline program is bound, then the modelview and projection matrices
  // will be sent to the wrong pipeline program, causing shader 
  // malfunction (typically, nothing will be shown on the screen).
  // In this homework, there is only one pipeline program, and it is already bound.
  // So technically speaking, this call is redundant in hw1.
  // However, in more complex programs (such as hw2), there will be more than one
  // pipeline program. And so we will need to bind the pipeline program that we want to use.
  pipelineProgram->Bind(); // This call is redundant in hw1, but it is good to keep for consistency.

  // Upload the modelview and projection matrices to the GPU.
  pipelineProgram->SetModelViewMatrix(modelViewMatrix);
  pipelineProgram->SetProjectionMatrix(projectionMatrix);

  // Execute different kinds of rendering based on the state
  if(renderState == RENDER_STATE::POINTS)
  {
    glUniform1i(smoothing, 0); // Set the smoothing uniform variable to false
    glBindVertexArray(pointsVAO); // Bind the pointsVAO
    glDrawArrays(GL_POINTS, 0, pointsNumVertices); // Render the pointsVAO
  }
  else if(renderState == RENDER_STATE::LINES)
  {
    glUniform1i(smoothing, 0); // Set the smoothing uniform variable to false
    glBindVertexArray(linesVAO); // Bind the linesVAO
    glDrawArrays(GL_LINES, 0, linesNumVertices); // Render the linesVAO
  }
  else if(renderState == RENDER_STATE::TRIANGLES)
  {
    glUniform1i(smoothing, 0); // Set the smoothing uniform variable to false
    glBindVertexArray(trianglesVAO); // Bind the trianglesVAO
    glDrawArrays(GL_TRIANGLES, 0, trianglesNumVertices); // Render the trianglesVAO
  }
  else if(renderState == RENDER_STATE::SMOOTH)
  {
    glUniform1i(smoothing, 1); // Set the smoothing uniform variable to true
    glBindVertexArray(smoothVAO); // Bind the smoothVAO
    glDrawArrays(GL_TRIANGLES, 0, trianglesNumVertices); // Render the smoothVAO
  }

  // Swap the double-buffers.
  glutSwapBuffers();
}

void idleFunc()
{
  // Do some stuff... 

  // For example, here, you can save the screenshots to disk (to make the animation).
  // USE sprintf() to take screenshots for animation
  // char filename[128];
  // sprintf(filename, "screenshot%03d.jpg", screenshotCount);
  // saveScreenshot(filename);
  // screenshotCount++;

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

    case '1':
      renderState = POINTS;
    break;

    case '2':
      renderState = LINES;
    break;

    case '3':
      renderState = TRIANGLES;
    break;

    case '4':
      renderState = SMOOTH;
    break;
  }
}

void createVBOandVAO(vector<float>& positions, vector<float>& colors, int vertices, GLuint& passedVBO, GLuint& passedVAO)
{
  float* pPosition = positions.data(); // turning positions vector into a float array
  float* pColor = colors.data(); // turning colors vector into a float array

  // 1. Create the VBO
  glGenBuffers(1, &passedVBO);
  glBindBuffer(GL_ARRAY_BUFFER, passedVBO);

  // 2. Allocate an empty VBO of the correct size to hold positions and colors.
  const int numBytesInPositions = vertices * 3 * sizeof(float);
  const int numBytesInColors = vertices * 4 * sizeof(float);
  glBufferData(GL_ARRAY_BUFFER, numBytesInPositions + numBytesInColors, nullptr, GL_STATIC_DRAW);

  // 3. Write the position and color data into the VBO.
  glBufferSubData(GL_ARRAY_BUFFER, 0, numBytesInPositions, pPosition); // The VBO starts with positions.
  glBufferSubData(GL_ARRAY_BUFFER, numBytesInPositions, numBytesInColors, pColor); // The colors are written after the positions.

  // 4. Create the VAO
  glGenVertexArrays(1, &passedVAO);
  glBindVertexArray(passedVAO);
  glBindBuffer(GL_ARRAY_BUFFER, passedVBO); // The VBO that we bind here will be used in the glVertexAttribPointer calls below. If we forget to bind the VBO here, the program will malfunction.

  // 5. Set up the relationship between the "position" shader variable and the VAO.
  const GLuint locationOfPosition = glGetAttribLocation(pipelineProgram->GetProgramHandle(), "position"); // Obtain a handle to the shader variable "position".
  glEnableVertexAttribArray(locationOfPosition); // Must always enable the vertex attribute. By default, it is disabled.
  const int stride = 0; // Stride is 0, i.e., data is tightly packed in the VBO.
  const GLboolean normalized = GL_FALSE; // Normalization is off.
  glVertexAttribPointer(locationOfPosition, 3, GL_FLOAT, normalized, stride, (const void *)0); // The shader variable "position" receives its data from the currently bound VBO (i.e., vertexPositionAndColorVBO), starting from offset 0 in the VBO. There are 3 float entries per vertex in the VBO (i.e., x,y,z coordinates). 

  // Set up the relationship between the "color" shader variable and the VAO.
  const GLuint locationOfColor = glGetAttribLocation(pipelineProgram->GetProgramHandle(), "color"); // Obtain a handle to the shader variable "color".
  glEnableVertexAttribArray(locationOfColor); // Must always enable the vertex attribute. By default, it is disabled.
  glVertexAttribPointer(locationOfColor, 4, GL_FLOAT, normalized, stride, (const void *)(unsigned long)numBytesInPositions); // The shader variable "color" receives its data from the currently bound VBO (i.e., vertexPositionAndColorVBO), starting from offset "numBytesInPositions" in the VBO. There are 4 float entries per vertex in the VBO (i.e., r,g,b,a channels). 
}

void createMultipleVBOandVAO(vector<float>& positionsCenter, vector<float>& colorsCenter, vector<float>& positionsLeft, vector<float>& positionsRight, 
  vector<float>& positionsUp, vector<float>& positionsDown, int vertices, GLuint& passedCenterVBO, 
  GLuint& passedLeftVBO, GLuint& passedRightVBO, GLuint& passedUpVBO, GLuint& passedDownVBO, GLuint& passedVAO)
{
  // Create the Center VBO & bind to smoothVAO
  createVBOandVAO(positionsCenter, colorsCenter, vertices, passedCenterVBO, passedVAO);

  float* temp;
  GLuint& tempVBO = passedCenterVBO;

  for(int i = 0; i < 4; i++)
  {
    if(i == 0)
    {
      temp = positionsLeft.data();
      tempVBO = passedLeftVBO;
    }
    else if(i == 1)
    {
      temp = positionsRight.data();
      tempVBO = passedRightVBO;
    }
    else if(i == 2)
    {
      temp = positionsUp.data();
      tempVBO = passedUpVBO;
    }
    else if(i == 3)
    {
      temp = positionsDown.data();
      tempVBO = passedDownVBO;
    }

    // 1. Create the VBO
    glGenBuffers(1, &tempVBO);
    glBindBuffer(GL_ARRAY_BUFFER, tempVBO);

    // 2. Allocate an empty VBO of the correct size to hold positions
    const int numBytesInPositions = vertices * 3 * sizeof(float);
    glBufferData(GL_ARRAY_BUFFER, numBytesInPositions, nullptr, GL_STATIC_DRAW);

    // 3. Write the position data into the VBO.
    glBufferSubData(GL_ARRAY_BUFFER, 0, numBytesInPositions, temp); // The VBO starts with positions.

    // 4. Bind the VBO to VAO
    glBindVertexArray(passedVAO);
    glBindBuffer(GL_ARRAY_BUFFER, tempVBO); // The VBO that we bind here will be used in the glVertexAttribPointer calls below. If we forget to bind the VBO here, the program will malfunction.

    // 5. Set up the relationship between the shaderStiring shader variable and the VAO.
    GLuint locationOfPosition;
    if(i == 0)
    {
      locationOfPosition = glGetAttribLocation(pipelineProgram->GetProgramHandle(), "positionLeft"); // Obtain a handle to the shader variable "positionLeft".
    }
    else if(i == 1)
    {
      locationOfPosition = glGetAttribLocation(pipelineProgram->GetProgramHandle(), "positionRight"); // Obtain a handle to the shader variable "positionRight".
    }
    else if(i == 2)
    {
      locationOfPosition = glGetAttribLocation(pipelineProgram->GetProgramHandle(), "positionUp"); // Obtain a handle to the shader variable "positionUp".
    }
    else if(i == 3)
    {
      locationOfPosition = glGetAttribLocation(pipelineProgram->GetProgramHandle(), "positionDown"); // Obtain a handle to the shader variable "positionDown".
    }

    glEnableVertexAttribArray(locationOfPosition); // Must always enable the vertex attribute. By default, it is disabled.
    const int stride = 0; // Stride is 0, i.e., data is tightly packed in the VBO.
    const GLboolean normalized = GL_FALSE; // Normalization is off.
    glVertexAttribPointer(locationOfPosition, 3, GL_FLOAT, normalized, stride, (const void *)0); // The shader variable shaderString receives its data from the currently bound VBO (i.e., vertexPositionAndColorVBO), starting from offset 0 in the VBO. There are 3 float entries per vertex in the VBO (i.e., x,y,z coordinates). 
  }
}

void pushNeighborPoints(int row, int col, int hmapWidth, int hmapHeight,
  vector<float>& leftPositions, vector<float>& rightPositions, vector<float>& upPositions, vector<float>& downPositions)
{
  // if in first row
  if(row == 0)
  {
    // get down neighbor's height
    float downNeighbor = heightmapImage->getPixel(row + 1, col, 0);

    // push down neighbor's x, y, z into up vector
    upPositions.push_back((-hmapWidth / 2.0f) + (float) row + 1); // x
    upPositions.push_back(scale * downNeighbor); // y
    upPositions.push_back((-hmapHeight / 2.0f) + (float) col); // z

    // repeat into down vector
    downPositions.push_back((-hmapWidth / 2.0f) + (float) row + 1); // x
    downPositions.push_back(scale * downNeighbor); // y
    downPositions.push_back((-hmapHeight / 2.0f) + (float) col); // z
  }
  // if in the last row
  else if(row == hmapHeight - 1)
  {
    // get up neighbor's height
    float upNeighbor = heightmapImage->getPixel(row - 1, col, 0);

    // push up neighbor's x, y, z into down vector
    downPositions.push_back((-hmapWidth / 2.0f) + (float) row - 1); // x
    downPositions.push_back(scale * upNeighbor); // y
    downPositions.push_back((-hmapHeight / 2.0f) + (float) col); // z

    // repeat into up vector
    upPositions.push_back((-hmapWidth / 2.0f) + (float) row - 1); // x
    upPositions.push_back(scale * upNeighbor); // y
    upPositions.push_back((-hmapHeight / 2.0f) + (float) col); // z
  }
  // in the middle somewhere
  else
  {
    // get up neighbor's height
    float upNeighbor = heightmapImage->getPixel(row - 1, col, 0);

    // push up neighbor's x, y, z into up vector
    upPositions.push_back((-hmapWidth / 2.0f) + (float) row - 1); // x
    upPositions.push_back(scale * upNeighbor); // y
    upPositions.push_back((-hmapHeight / 2.0f) + (float) col); // z

    // get down neighbor's height
    float downNeighbor = heightmapImage->getPixel(row + 1, col, 0);

    // push down neighbor's x, y, z into down vector
    downPositions.push_back((-hmapWidth / 2.0f) + (float) row + 1); // x
    downPositions.push_back(scale * downNeighbor); // y
    downPositions.push_back((-hmapHeight / 2.0f) + (float) col); // z
  }

  // if in first column
  if(col == 0)
  {
    // get right neighbor's height
    float rightNeighbor = heightmapImage->getPixel(row, col + 1, 0);

    // push right neighbor's x, y, z into left vector
    leftPositions.push_back((-hmapWidth / 2.0f) + (float) row); // x
    leftPositions.push_back(scale * rightNeighbor); // y
    leftPositions.push_back((-hmapHeight / 2.0f) + (float) col + 1); // z

    // repeat into right vector
    rightPositions.push_back((-hmapWidth / 2.0f) + (float) row); // x
    rightPositions.push_back(scale * rightNeighbor); // y
    rightPositions.push_back((-hmapHeight / 2.0f) + (float) col + 1); // z
  }
  // if in the last column
  else if(col == hmapWidth - 1)
  {
    // get left neighbor's height
    float leftNeighbor = heightmapImage->getPixel(row, col - 1, 0);

    // push left neighbor's x, y, z into right vector
    rightPositions.push_back((-hmapWidth / 2.0f) + (float) row); // x
    rightPositions.push_back(scale * leftNeighbor); // y
    rightPositions.push_back((-hmapHeight / 2.0f) + (float) col - 1); // z

    // repeat into left vector
    leftPositions.push_back((-hmapWidth / 2.0f) + (float) row); // x
    leftPositions.push_back(scale * leftNeighbor); // y
    leftPositions.push_back((-hmapHeight / 2.0f) + (float) col - 1); // z
  }
  // in the middle somewhere
  else
  {
    // get left neighbor's height
    float leftNeighbor = heightmapImage->getPixel(row, col - 1, 0);

    // push left neighbor's x, y, z into left vector
    leftPositions.push_back((-hmapWidth / 2.0f) + (float) row); // x
    leftPositions.push_back(scale * leftNeighbor); // y
    leftPositions.push_back((-hmapHeight / 2.0f) + (float) col - 1); // z

    // get right neighbor's height
    float rightNeighbor = heightmapImage->getPixel(row, col + 1, 0);

    // push right neighbor's x, y, z into right vector
    rightPositions.push_back((-hmapWidth / 2.0f) + (float) row); // x
    rightPositions.push_back(scale * rightNeighbor); // y
    rightPositions.push_back((-hmapHeight / 2.0f) + (float) col + 1); // z
  }
}

void fillPoints(vector<float>& pointPositions, vector<float>& pointColors, int hmapHeight, int hmapWidth)
{
  // nested loop through the height and width of the image
  for(int i = 0; i < hmapHeight; i++)
  {
    for(int j = 0; j < hmapWidth; j++)
    {
      // get the height of the image at the current pixel
      float height = heightmapImage->getPixel(i, j, 0);

      // store x, y, z coordinates of the current vertex
      pointPositions.push_back((-hmapWidth / 2.0f) + (float) i); // x
      pointPositions.push_back(scale * height); // y
      pointPositions.push_back((-hmapHeight / 2.0f) + (float) j); // z

      // store r, g, b, a values of the current vertex (proportionate to height)
      pointColors.push_back(height / 255.0f); // r
      pointColors.push_back(height / 255.0f); // g
      pointColors.push_back(height / 255.0f); // b
      pointColors.push_back(1.0f); // a
    }
  }
  pointsNumVertices = hmapHeight * hmapWidth;
}

void fillLines(vector<float>& linePositions, vector<float>& lineColors, int hmapHeight, int hmapWidth)
{
  // nested loop through the height and width of the image
  for(int i = 0; i < hmapHeight; i++)
  {
    for(int j = 0; j < hmapWidth; j++)
    {
      // get the height of the image at the current pixel
      float height = heightmapImage->getPixel(i, j, 0);

      // if we are not at the last column, draw a line to the right
      if(j != hmapWidth - 1)
      {
        // store x, y, z coordinates of the current vertex
        linePositions.push_back((-hmapWidth / 2.0f) + (float) i); // x
        linePositions.push_back(scale * height); // y
        linePositions.push_back((-hmapHeight / 2.0f) + (float) j); // z

        // store r, g, b, a values of the current vertex (proportionate to height)
        lineColors.push_back(height / 255.0f); // r
        lineColors.push_back(height / 255.0f); // g
        lineColors.push_back(height / 255.0f); // b
        lineColors.push_back(1.0f); // a

        float heightRight = heightmapImage->getPixel(i, j + 1, 0);

        // store x, y, z coordinates of the right vertex
        linePositions.push_back((-hmapWidth / 2.0f) + (float) i); // x
        linePositions.push_back(scale * heightRight); // y
        linePositions.push_back((-hmapHeight / 2.0f) + (float) j + 1); // z

        // store r, g, b, a values of the right vertex (proportionate to height)
        lineColors.push_back(heightRight / 255.0f); // r
        lineColors.push_back(heightRight / 255.0f); // g
        lineColors.push_back(heightRight / 255.0f); // b
        lineColors.push_back(1.0f); // a
      }

      // if we are not at the last row, draw a line down
      if(i != hmapHeight - 1)
      {
        // store x, y, z coordinates of the current vertex
        linePositions.push_back((-hmapWidth / 2.0f) + (float) i); // x
        linePositions.push_back(scale * height); // y
        linePositions.push_back((-hmapHeight / 2.0f) + (float) j); // z

        // store r, g, b, a values of the current vertex (proportionate to height)
        lineColors.push_back(height / 255.0f); // r
        lineColors.push_back(height / 255.0f); // g
        lineColors.push_back(height / 255.0f); // b
        lineColors.push_back(1.0f); // a

        float heightDown = heightmapImage->getPixel(i + 1, j, 0);

        // store x, y, z coordinates of the down vertex
        linePositions.push_back((-hmapWidth / 2.0f) + (float) i + 1); // x
        linePositions.push_back(scale * heightDown); // y
        linePositions.push_back((-hmapHeight / 2.0f) + (float) j); // z

        // store r, g, b, a values of the down vertex (proportionate to height)
        lineColors.push_back(heightDown / 255.0f); // r
        lineColors.push_back(heightDown / 255.0f); // g
        lineColors.push_back(heightDown / 255.0f); // b
        lineColors.push_back(1.0f); // a
      }

      // if we are in the last row and column, just draw this vertex
      if(i == hmapHeight - 1 && j == hmapWidth - 1)
      {
        // store x, y, z coordinates of the current vertex
        linePositions.push_back((-hmapWidth / 2.0f) + (float) i); // x
        linePositions.push_back(scale * height); // y
        linePositions.push_back((-hmapHeight / 2.0f) + (float) j); // z

        // store r, g, b, a values of the current vertex (proportionate to height)
        lineColors.push_back(height / 255.0f); // r
        lineColors.push_back(height / 255.0f); // g
        lineColors.push_back(height / 255.0f); // b
        lineColors.push_back(1.0f); // a
      }
    }
  }
  linesNumVertices = linePositions.size() / 3; // 3 floats per vertex
}

void fillTriangles(vector<float>& trianglePositions, vector<float>& triangleColors, int hmapHeight, int hmapWidth,
  vector<float>& leftNeighbors, vector<float>& rightNeighbors, vector<float>& upNeighbors, vector<float>& downNeighbors)
{
  // nested loop through the height and width of the image
  for(int i = 0; i < hmapHeight; i++)
  {
    for(int j = 0; j < hmapWidth; j++)
    {
      float height = heightmapImage->getPixel(i, j, 0);

      // if we are in the first row or first column, make one triangle to the right and down
      if(i == 0 || j == 0)
      {
        // store x, y, z coordinates of the current vertex
        trianglePositions.push_back((-hmapWidth / 2.0f) + (float) i); // x
        trianglePositions.push_back(scale * height); // y
        trianglePositions.push_back((-hmapHeight / 2.0f) + (float) j); // z

        // store r, g, b, a values of the current vertex (proportionate to height)
        triangleColors.push_back(height / 255.0f); // r
        triangleColors.push_back(height / 255.0f); // g
        triangleColors.push_back(height / 255.0f); // b
        triangleColors.push_back(1.0f);

        pushNeighborPoints(i, j, hmapHeight, hmapWidth, leftNeighbors, rightNeighbors, upNeighbors, downNeighbors); // call a function to fill the neighbor vectors

        float heightRight = heightmapImage->getPixel(i, j + 1, 0);

        // store x, y, z coordinates of the right vertex
        trianglePositions.push_back((-hmapWidth / 2.0f) + (float) i); // x
        trianglePositions.push_back(scale * heightRight); // y
        trianglePositions.push_back((-hmapHeight / 2.0f) + (float) j + 1); // z

        // store r, g, b, a values of the right vertex (proportionate to height)
        triangleColors.push_back(heightRight / 255.0f); // r
        triangleColors.push_back(heightRight / 255.0f); // g
        triangleColors.push_back(heightRight / 255.0f); // b
        triangleColors.push_back(1.0f);

        pushNeighborPoints(i, j + 1, hmapHeight, hmapWidth, leftNeighbors, rightNeighbors, upNeighbors, downNeighbors); // call a function to fill the neighbor vectors

        float heightDown = heightmapImage->getPixel(i + 1, j, 0);

        // store x, y, z coordinates of the down vertex
        trianglePositions.push_back((-hmapWidth / 2.0f) + (float) i + 1); // x
        trianglePositions.push_back(scale * heightDown); // y
        trianglePositions.push_back((-hmapHeight / 2.0f) + (float) j); // z

        // store r, g, b, a values of the down vertex (proportionate to height)
        triangleColors.push_back(heightDown / 255.0f); // r
        triangleColors.push_back(heightDown / 255.0f); // g
        triangleColors.push_back(heightDown / 255.0f); // b
        triangleColors.push_back(1.0f);

        pushNeighborPoints(i + 1, j, hmapHeight, hmapWidth, leftNeighbors, rightNeighbors, upNeighbors, downNeighbors); // call a function to fill the neighbor vectors

        trianglesNumVertices += 3;

        continue;
      }

      // if we are in the last row or last column, make one triangle to the left and up
      if(i == hmapHeight - 1 || j == hmapWidth - 1)
      {
        // store x, y, z coordinates of the current vertex
        trianglePositions.push_back((-hmapWidth / 2.0f) + (float) i); // x
        trianglePositions.push_back(scale * height); // y
        trianglePositions.push_back((-hmapHeight / 2.0f) + (float) j); // z

        // store r, g, b, a values of the current vertex (proportionate to height)
        triangleColors.push_back(height / 255.0f); // r
        triangleColors.push_back(height / 255.0f); // g
        triangleColors.push_back(height / 255.0f); // b
        triangleColors.push_back(1.0f);

        pushNeighborPoints(i, j, hmapHeight, hmapWidth, leftNeighbors, rightNeighbors, upNeighbors, downNeighbors); // call a function to fill the neighbor vectors

        float heightLeft = heightmapImage->getPixel(i, j - 1, 0);

        // store x, y, z coordinates of the left vertex
        trianglePositions.push_back((-hmapWidth / 2.0f) + (float) i); // x
        trianglePositions.push_back(scale * heightLeft); // y
        trianglePositions.push_back((-hmapHeight / 2.0f) + (float) j - 1); // z

        // store r, g, b, a values of the left vertex (proportionate to height)
        triangleColors.push_back(heightLeft / 255.0f); // r
        triangleColors.push_back(heightLeft / 255.0f); // g
        triangleColors.push_back(heightLeft / 255.0f); // b
        triangleColors.push_back(1.0f);

        pushNeighborPoints(i, j - 1, hmapHeight, hmapWidth, leftNeighbors, rightNeighbors, upNeighbors, downNeighbors); // call a function to fill the neighbor vectors

        float heightUp = heightmapImage->getPixel(i - 1, j, 0);

        // store x, y, z coordinates of the up vertex
        trianglePositions.push_back((-hmapWidth / 2.0f) + (float) i - 1); // x
        trianglePositions.push_back(scale * heightUp); // y
        trianglePositions.push_back((-hmapHeight / 2.0f) + (float) j); // z

        // store r, g, b, a values of the up vertex (proportionate to height)
        triangleColors.push_back(heightUp / 255.0f); // r
        triangleColors.push_back(heightUp / 255.0f); // g
        triangleColors.push_back(heightUp / 255.0f); // b
        triangleColors.push_back(1.0f);

        pushNeighborPoints(i - 1, j, hmapHeight, hmapWidth, leftNeighbors, rightNeighbors, upNeighbors, downNeighbors); // call a function to fill the neighbor vectors

        trianglesNumVertices += 3;

        continue;
      }

      // otherwise, make two triangles to the right and down and left and up
      if(i > 0 && i < hmapHeight - 1 && j > 0 && j < hmapWidth - 1)
      {
        // TRIANGLE 1 -- RIGHT AND DOWN
        // store x, y, z coordinates of the current vertex
        trianglePositions.push_back((-hmapWidth / 2.0f) + (float) i); // x
        trianglePositions.push_back(scale * height); // y
        trianglePositions.push_back((-hmapHeight / 2.0f) + (float) j); // z

        // store r, g, b, a values of the current vertex (proportionate to height)
        triangleColors.push_back(height / 255.0f); // r
        triangleColors.push_back(height / 255.0f); // g
        triangleColors.push_back(height / 255.0f); // b
        triangleColors.push_back(1.0f);

        pushNeighborPoints(i, j, hmapHeight, hmapWidth, leftNeighbors, rightNeighbors, upNeighbors, downNeighbors); // call a function to fill the neighbor vectors

        float heightRight = heightmapImage->getPixel(i, j + 1, 0);

        // store x, y, z coordinates of the right vertex
        trianglePositions.push_back((-hmapWidth / 2.0f) + (float) i); // x
        trianglePositions.push_back(scale * heightRight); // y
        trianglePositions.push_back((-hmapHeight / 2.0f) + (float) j + 1); // z

        // store r, g, b, a values of the right vertex (proportionate to height)
        triangleColors.push_back(heightRight / 255.0f); // r
        triangleColors.push_back(heightRight / 255.0f); // g
        triangleColors.push_back(heightRight / 255.0f); // b
        triangleColors.push_back(1.0f);

        pushNeighborPoints(i, j + 1, hmapHeight, hmapWidth, leftNeighbors, rightNeighbors, upNeighbors, downNeighbors); // call a function to fill the neighbor vectors

        float heightDown = heightmapImage->getPixel(i + 1, j, 0);

        // store x, y, z coordinates of the down vertex
        trianglePositions.push_back((-hmapWidth / 2.0f) + (float) i + 1); // x
        trianglePositions.push_back(scale * heightDown); // y
        trianglePositions.push_back((-hmapHeight / 2.0f) + (float) j); // z

        // store r, g, b, a values of the down vertex (proportionate to height)
        triangleColors.push_back(heightDown / 255.0f); // r
        triangleColors.push_back(heightDown / 255.0f); // g
        triangleColors.push_back(heightDown / 255.0f); // b
        triangleColors.push_back(1.0f);

        pushNeighborPoints(i + 1, j, hmapHeight, hmapWidth, leftNeighbors, rightNeighbors, upNeighbors, downNeighbors); // call a function to fill the neighbor vectors

        // TRIANGLE 2 -- LEFT AND UP
        // store x, y, z coordinates of the current vertex
        trianglePositions.push_back((-hmapWidth / 2.0f) + (float) i); // x
        trianglePositions.push_back(scale * height); // y
        trianglePositions.push_back((-hmapHeight / 2.0f) + (float) j); // z

        // store r, g, b, a values of the current vertex (proportionate to height)
        triangleColors.push_back(height / 255.0f); // r
        triangleColors.push_back(height / 255.0f); // g
        triangleColors.push_back(height / 255.0f); // b
        triangleColors.push_back(1.0f);

        pushNeighborPoints(i, j, hmapHeight, hmapWidth, leftNeighbors, rightNeighbors, upNeighbors, downNeighbors); // call a function to fill the neighbor vectors

        float heightLeft = heightmapImage->getPixel(i, j - 1, 0);
        
        // store x, y, z coordinates of the left vertex
        trianglePositions.push_back((-hmapWidth / 2.0f) + (float) i); // x
        trianglePositions.push_back(scale * heightLeft); // y
        trianglePositions.push_back((-hmapHeight / 2.0f) + (float) j - 1); // z

        // store r, g, b, a values of the left vertex (proportionate to height)
        triangleColors.push_back(heightLeft / 255.0f); // r
        triangleColors.push_back(heightLeft / 255.0f); // g
        triangleColors.push_back(heightLeft / 255.0f); // b
        triangleColors.push_back(1.0f);

        pushNeighborPoints(i, j - 1, hmapHeight, hmapWidth, leftNeighbors, rightNeighbors, upNeighbors, downNeighbors); // call a function to fill the neighbor vectors

        float heightUp = heightmapImage->getPixel(i - 1, j, 0);

        // store x, y, z coordinates of the up vertex
        trianglePositions.push_back((-hmapWidth / 2.0f) + (float) i - 1); // x
        trianglePositions.push_back(scale * heightUp); // y
        trianglePositions.push_back((-hmapHeight / 2.0f) + (float) j); // z

        // store r, g, b, a values of the up vertex (proportionate to height)
        triangleColors.push_back(heightUp / 255.0f); // r
        triangleColors.push_back(heightUp / 255.0f); // g
        triangleColors.push_back(heightUp / 255.0f); // b
        triangleColors.push_back(1.0f);

        pushNeighborPoints(i - 1, j, hmapHeight, hmapWidth, leftNeighbors, rightNeighbors, upNeighbors, downNeighbors); // call a function to fill the neighbor vectors

        trianglesNumVertices += 6;

        continue;
      }
    }
  }
}

void initScene(int argc, char *argv[])
{
  // Load the image from a jpeg disk file into main memory.
  heightmapImage = new ImageIO();
  if (heightmapImage->loadJPEG(argv[1]) != ImageIO::OK)
  {
    cout << "Error reading image " << argv[1] << "." << endl;
    exit(EXIT_FAILURE);
  }

  // Set the background color.
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Black color.

  // Enable z-buffering (i.e., hidden surface removal using the z-buffer algorithm).
  glEnable(GL_DEPTH_TEST);

  // Create and bind the pipeline program. This operation must be performed BEFORE we initialize any VAOs.
  pipelineProgram = new BasicPipelineProgram;
  int ret = pipelineProgram->Init(shaderBasePath);
  if (ret != 0) 
  { 
    abort();
  }
  pipelineProgram->Bind();

  // Obtain a handle to the shader variable "smoothing".
  smoothing = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "smoothing"); // used to determine if we smooth the triangles or not

  int hmapHeight = heightmapImage->getHeight(); // height of the heightmap image
  int hmapWidth = heightmapImage->getWidth(); // width of the heightmap image

  vector<float> pointPositions; // will hold positions of vertices
  vector<float> pointColors; // will hold colors of vertices

  vector<float> linePositions; // will hold positions of vertices
  vector<float> lineColors; // will hold colors of vertices

  vector<float> trianglePositions; // will hold positions of vertices
  vector<float> triangleColors; // will hold colors of vertices

  vector<float> leftNeighbors; // will hold positions of left neighbors (triangle mode)
  vector<float> rightNeighbors; // will hold positions of right neighbors (triangle mode)
  vector<float> upNeighbors; // will hold positions of up neighbors (triangle mode)
  vector<float> downNeighbors; // will hold positions of down neighbors (triangle mode)

  // !---POINTS---!
  fillPoints(pointPositions, pointColors, hmapHeight, hmapWidth); // call a function to fill the points vectors

  // !---LINES---!
  fillLines(linePositions, lineColors, hmapHeight, hmapWidth); // call a function to fill the lines vectors

  // !---TRIANGLES---!
  fillTriangles(trianglePositions, triangleColors, hmapHeight, hmapWidth,
    leftNeighbors, rightNeighbors, upNeighbors, downNeighbors); // call a function to fill the triangles vectors & smoothing

  // !---POINTS VBO & VAO---!
  createVBOandVAO(pointPositions, pointColors, pointsNumVertices, pointsVertexPositionAndColorVBO, pointsVAO);

  // !--LINES VBO & VAO---!
  createVBOandVAO(linePositions, lineColors, linesNumVertices, linesVertexPositionAndColorVBO, linesVAO);

  // !---TRIANGLES VBO & VAO---!
  createVBOandVAO(trianglePositions, triangleColors, trianglesNumVertices, trianglesVertexPositionAndColorVBO, trianglesVAO);

  // !---SMOOTHING VBO & VAO---!
  createMultipleVBOandVAO(trianglePositions, triangleColors, leftNeighbors, rightNeighbors, upNeighbors, downNeighbors, trianglesNumVertices, 
    smoothCenterVBO, smoothLeftNeighborVBO, smoothRightNeighborVBO, smoothUpNeighborVBO, 
    smoothDownNeighborVBO, smoothVAO);

  // We don't need this data any more, as we have already uploaded it to the VBO. And so we can destroy it, to avoid a memory leak.
  vector<float> pointPos;
  vector<float> pointCol;
  vector<float> linePos;
  vector<float> lineCol;
  vector<float> trianglePos;
  vector<float> triangleCol;
  pointPositions.swap(pointPos);
  pointColors.swap(pointCol);
  linePositions.swap(linePos);
  lineColors.swap(lineCol);
  trianglePositions.swap(trianglePos);
  triangleColors.swap(triangleCol);

  // Check for any OpenGL errors.
  std::cout << "GL error: " << glGetError() << std::endl;
}

int main(int argc, char *argv[])
{
  if (argc != 2)
  {
    cout << "The arguments are incorrect." << endl;
    cout << "usage: ./hw1 <heightmap file>" << endl;
    exit(EXIT_FAILURE);
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

  // Perform the initialization.
  initScene(argc, argv);

  // Sink forever into the GLUT loop.
  glutMainLoop();
}
