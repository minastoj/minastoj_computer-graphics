/* **************************
 * CSCI 420
 * Assignment 3 Raytracer
 * Name: Mina Stojanovic
 * *************************
*/

#ifdef WIN32
  #include <windows.h>
#endif

#if defined(WIN32) || defined(linux)
  #include <GL/gl.h>
  #include <GL/glut.h>
#elif defined(__APPLE__)
  #include <OpenGL/gl.h>
  #include <GLUT/glut.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
  #define strcasecmp _stricmp
#endif

#include <imageIO.h>

#include <glm/glm.hpp>

#define MAX_TRIANGLES 20000
#define MAX_SPHERES 100
// #define MAX_LIGHTS 100
#define MAX_LIGHTS 1000 // bc of soft shadows

char * filename = NULL;

//different display modes
#define MODE_DISPLAY 1
#define MODE_JPEG 2

int mode = MODE_DISPLAY;

//you may want to make these smaller for debugging purposes
#define WIDTH 640
#define HEIGHT 480

//the field of view of the camera
#define fov 60.0

unsigned char buffer[HEIGHT][WIDTH][3];

struct Vertex
{
  double position[3];
  double color_diffuse[3];
  double color_specular[3];
  double normal[3];
  double shininess;
};

struct Triangle
{
  Vertex v[3];

  float GetVertexPosByAxis(int vertex, int axis)
  {
    // axis 0 = x, 1 = y, 2 = z
    return v[vertex].position[axis];
  }

  float GetVertexNormalByAxis(int vertex, int axis)
  {
    // axis 0 = x, 1 = y, 2 = z
    return v[vertex].normal[axis];
  }

  float GetVertexSpecularByAxis(int vertex, int axis)
  {
    // axis 0 = r, 1 = g, 2 = b
    return v[vertex].color_specular[axis];
  }

  float GetVertexColorByAxis(int vertex, int axis)
  {
    // axis 0 = r, 1 = g, 2 = b
    return v[vertex].color_diffuse[axis];
  }

  float GetVertexShineness(int vertex)
  {
    return v[vertex].shininess;
  }
};

struct Sphere
{
  double position[3];
  double color_diffuse[3];
  double color_specular[3];
  double shininess;
  double radius;
};

struct Light
{
  double position[3];
  double color[3];
};

struct Ray
{
  glm::vec3 origin;
  glm::vec3 direction;

  Ray()
  {
    origin = glm::vec3(0.0f, 0.0f, 0.0f);
    direction = glm::vec3(0.0f, 0.0f, 0.0f);
  }

  Ray(glm::vec3 o, glm::vec3 d)
  {
    origin = o;
    direction = d;
  }
};

Triangle triangles[MAX_TRIANGLES];
Sphere spheres[MAX_SPHERES];
Light lights[MAX_LIGHTS];
Light softLights[MAX_LIGHTS]; // used for soft shadows
double ambient_light[3];

int num_triangles = 0;
int num_spheres = 0;
int num_lights = 0;
int num_soft_lights = 0; // used for soft shadows

void plot_pixel_display(int x,int y,unsigned char r,unsigned char g,unsigned char b);
void plot_pixel_jpeg(int x,int y,unsigned char r,unsigned char g,unsigned char b);
void plot_pixel(int x,int y,unsigned char r,unsigned char g,unsigned char b);

double MAX_DISPLACEMENT_RADIUS = 0.25; // used for randomization of lights for soft shadows

float clamp(float input)
{
  if(input < 0.0f)
  { 
    return 0.0f;
  }
  else if(input > 1.0f)
  {
    return 1.0f;
  }
  else
  {
    return input;
  }
}

Ray ShootRay(int x, int y)
{
  Ray ray; // origin is automatically set to (0, 0, 0) & direction is (1, 1, 1) --> need to change direction

  float aspectRatio = float(WIDTH) / float(HEIGHT);
  float fovDegrees = (fov / 2.0f) * (M_PI / 180.0f);
  float tanOfAngle = std::tan(fovDegrees);

  // calculate direction of the ray in terms of screen
  float screenXDir = (2 * ((x + 0.5) / float(WIDTH)) - 1) * aspectRatio * tanOfAngle;
  float screenYDir =  (2 * ((y + 0.5) / float(HEIGHT)) - 1) * tanOfAngle;
  
  // change the ray direction from (1, 1, 1) to (screenXDir, screenYDir, -1)
  glm::vec3 direction = glm::vec3(screenXDir, screenYDir, -1.0f);
  direction = glm::normalize(direction);

  ray.direction = glm::vec3(screenXDir, screenYDir, -1.0f); // set the direction
  ray.direction = glm::normalize(ray.direction); // normalize the direction

  return ray;
}

bool CheckSphere(Ray ray, Sphere sphere, glm::vec3& intersection)
{
  // a = dx^2 + dy^2 + dz^2
  float aComp =  ray.direction.x * ray.direction.x + ray.direction.y * ray.direction.y + ray.direction.z * ray.direction.z;
  
  // b = 2 * (dx * (x - x0) + (y - y0) * dy + (z - z0) * dz)
  float bComp = 2 * (ray.direction.x * (ray.origin.x - sphere.position[0]) + 
  ray.direction.y * (ray.origin.y - sphere.position[1]) + 
  ray.direction.z * (ray.origin.z - sphere.position[2]));

  // c = (x - x0)^2 + (y - y0)^2 + (z - z0)^2 - r^2
  float cComp = ((ray.origin.x - sphere.position[0]) * (ray.origin.x - sphere.position[0])) +
  ((ray.origin.y - sphere.position[1]) * (ray.origin.y - sphere.position[1])) +
  ((ray.origin.z - sphere.position[2]) * (ray.origin.z - sphere.position[2])) - (sphere.radius * sphere.radius);

  float discrim = bComp * bComp - (4 * aComp * cComp); // discriminant

  if(discrim < 0)
  {
    return false;
  }
  else
  {
    float t0 = (-bComp + sqrt(discrim)) / 2;
    float t1 = (-bComp - sqrt(discrim)) / 2;

    if(t0 < 0 || t1 < 0)
    {
      return false;
    }
    else
    {
      float t = t0 < t1 ? t0 : t1;
      intersection = ray.origin + (ray.direction * t);
      return true;
    }
  }
}

bool CheckPlane(Ray ray, Triangle triangle, glm::vec3& intersection)
{
  // get the vertices of the triangle
  glm::vec3 P1(triangle.GetVertexPosByAxis(0, 0), triangle.GetVertexPosByAxis(0, 1), triangle.GetVertexPosByAxis(0, 2));
  glm::vec3 P2(triangle.GetVertexPosByAxis(1, 0), triangle.GetVertexPosByAxis(1, 1), triangle.GetVertexPosByAxis(1, 2));
  glm::vec3 P3(triangle.GetVertexPosByAxis(2, 0), triangle.GetVertexPosByAxis(2, 1), triangle.GetVertexPosByAxis(2, 2));

  // get edges for finding normal
  glm::vec3 E1 = P2 - P1;
  glm::vec3 E2 = P3 - P1;

  // find surface normal vector
  glm::vec3 N = glm::cross(E1, E2);
  N = glm::normalize(N);
  
  // find denominator = normal * ray direction
  float denom = glm::dot(N, ray.direction);

  if(denom == 0) // if the denominator is 0, the ray is parallel to the plane --> no intersection
  {
    return false;
  }

  glm::vec3 distance = P1 - ray.origin; // distance = P1 - ray origin
  float dCoeff = glm::dot(distance, N); // dCoeff = distance * normal

  // since denom != 0, t = dCoeff / denom
  float t = dCoeff / denom;

  if(t < 0) // if t is negative, the intersection is behind the ray
  {
    return false;
  }
  else
  {
    intersection = ray.origin + (ray.direction * t);
    return true;
  }
}

bool CheckTriangle(Ray ray, Triangle triangle, glm::vec3& intersection)
{
  if(CheckPlane(ray, triangle, intersection)) // if we intersect with the plane, do the following steps to check triangle
  {
    // get the vertices of the triangle
    glm::vec3 P1(triangle.GetVertexPosByAxis(0, 0), triangle.GetVertexPosByAxis(0, 1), triangle.GetVertexPosByAxis(0, 2));
    glm::vec3 P2(triangle.GetVertexPosByAxis(1, 0), triangle.GetVertexPosByAxis(1, 1), triangle.GetVertexPosByAxis(1, 2));
    glm::vec3 P3(triangle.GetVertexPosByAxis(2, 0), triangle.GetVertexPosByAxis(2, 1), triangle.GetVertexPosByAxis(2, 2));

    // get all 3 edges
    glm::vec3 E1 = P2 - P1;
    glm::vec3 E2 = P3 - P2;
    glm::vec3 E3 = P1 - P3;

    // find normal (again)
    glm::vec3 N = glm::cross(E1, P3 - P1);
    N = glm::normalize(N);

    // test that the intersection is within the triangle
    glm::vec3 C1 = glm::cross(E1, intersection - P1);
    glm::vec3 C2 = glm::cross(E2, intersection - P2);
    glm::vec3 C3 = glm::cross(E3, intersection - P3);

    // dot product of the 3 cross products with normal
    float NdotC1 = glm::dot(N, C1);
    float NdotC2 = glm::dot(N, C2);
    float NdotC3 = glm::dot(N, C3);

    // if any of the dot products are negative, then the intersection is outside the triangle
    if(NdotC1 < 0 || NdotC2 < 0 || NdotC3 < 0)
    {
      return false;
    }
    else
    {
      return true;
    }
  }
  else // if it doesn't intersect with the plane, it certainly doesn't intersect with anything else
  {
    return false;
  }
}

float FindArea(glm::vec3 P1, glm::vec3 P2, glm::vec3 P3)
{
  // get 2 necessary edges for finding normal
  glm::vec3 E1 = P2 - P1;
  glm::vec3 E2 = P3 - P1;

  // find normal
  glm::vec3 N = glm::cross(E1, E2);
  N = glm::normalize(N);

  float area = 0;

  if(glm::dot(N, glm::vec3(0, 0, 1)) != 0) // if the normal is not parallel to the z-axis
  {
    area = 0.5 * ((P2.x - P1.x) * (P3.y - P1.y) - (P3.x - P1.x) * (P2.y - P1.y));
  }
  else
  {
    area =  0.5 * ((P2.x - P1.x) * (P3.z - P1.z) - (P3.x - P1.x) * (P2.z - P1.z));
  }

  return area;
}

glm::vec3 FindSphereColor(Sphere sphere, glm::vec3 intersection, int sphereIndex)
{
  glm::vec3 color(0, 0, 0); // start with black

  // for(int l = 0; l < num_lights; l++)
  for(int l = 0; l < num_soft_lights; l++)
  {
    // glm::vec3 lightPos = glm::vec3(lights[l].position[0], lights[l].position[1], lights[l].position[2]);
    glm::vec3 lightPos = glm::vec3(softLights[l].position[0], softLights[l].position[1], softLights[l].position[2]);
    Ray shadow = Ray(intersection, lightPos - intersection); // create a shadow ray to the light
    shadow.direction = glm::normalize(shadow.direction); // normalize the shadow ray

    bool hitsObjects = false;

    // check if the shadow ray intersects with any spheres
    for(int i = 0; i < num_spheres; i++)
    {
      Sphere currS = spheres[i];
      glm::vec3 intersectS(-MAXFLOAT, -MAXFLOAT, -MAXFLOAT);

      if(CheckSphere(shadow, currS, intersectS) && i != sphereIndex) // if the shadow ray intersects with a sphere
      {
        // if the intersection is closer to the light than the sphere, the light is blocked
        if(glm::distance(intersectS, intersection) < glm::distance(lightPos, intersection))
        {
          hitsObjects = true;
          break;
        }
      }
    }

    // check if the shadow ray intersects with any triangles
    for(int j = 0; j < num_triangles; j++)
    {
      Triangle currT = triangles[j];
      glm::vec3 intersectT(-MAXFLOAT, -MAXFLOAT, -MAXFLOAT);

      if(CheckTriangle(shadow, currT, intersectT)) // if the shadow ray intersects with a triangle
      {
        // if the intersection is closer to the light than the triangle, the light is blocked
        if(glm::distance(intersectT, intersection) < glm::distance(lightPos, intersection))
        {
          hitsObjects = true;
          break;
        }
      }
    }

    if(!hitsObjects) // if the shadow ray doesn't hit anything, add the light to the color
    {
      glm::vec3 N = glm::normalize(intersection - glm::vec3(sphere.position[0], sphere.position[1], sphere.position[2])); // normal vector
      // N *= (1.0 / sphere.radius); // unit normal vector

      glm::vec3 L = lightPos - intersection; // light vector
      L = glm::normalize(L); // normalize light vector

      float LdotN = clamp(glm::dot(L, N)); // dot product of light and normal

      glm::vec3 R = (2 * LdotN * N) - L; // r = 2(l * n)n - l (reflection vector)
      R = glm::normalize(R); // normalize reflection vector

      float RdotV = clamp(glm::dot(R, glm::normalize(-intersection))); // dot product of reflection and vector back to camera
      float RdotVAlpha = pow(RdotV, sphere.shininess); // RdotV ^ alpha

      // glm::vec3 lightColor = glm::vec3(lights[l].color[0], lights[l].color[1], lights[l].color[2]);
      glm::vec3 lightColor = glm::vec3(softLights[l].color[0], softLights[l].color[1], softLights[l].color[2]);
      glm::vec3 kd = glm::vec3(sphere.color_diffuse[0], sphere.color_diffuse[1], sphere.color_diffuse[2]);
      glm::vec3 ks = glm::vec3(sphere.color_specular[0], sphere.color_specular[1], sphere.color_specular[2]);

      color += lightColor * (kd * LdotN + ks * RdotVAlpha);
      color = glm::clamp(color, 0.0f, 1.0f);
    }
  }

  return color;
}

glm::vec3 FindTriangleColor(Triangle triangle, glm::vec3 intersection, int triangleIndex)
{
  glm::vec3 color(0, 0, 0); // start with black

  // for(int l = 0; l < num_lights; l++) // loop through the lights
  for(int l = 0; l < num_soft_lights; l++)
  {
    // glm::vec3 lightPos = glm::vec3(lights[l].position[0], lights[l].position[1], lights[l].position[2]);
    glm::vec3 lightPos = glm::vec3(softLights[l].position[0], softLights[l].position[1], softLights[l].position[2]);
    Ray shadow = Ray(intersection, lightPos - intersection); // create a shadow ray to the light
    shadow.direction = glm::normalize(shadow.direction); // normalize the shadow ray

    bool hitsObjects = false;

    // check if the shadow ray intersects with any spheres
    for(int i = 0; i < num_spheres; i++)
    {
      Sphere currS = spheres[i];
      glm::vec3 intersectS(-MAXFLOAT, -MAXFLOAT, -MAXFLOAT);

      if(CheckSphere(shadow, currS, intersectS)) // if the shadow ray intersects with a sphere
      {
        // if the intersection is closer to the light than the sphere, the light is blocked
        if(glm::distance(intersectS, intersection) < glm::distance(lightPos, intersection))
        {
          hitsObjects = true;
          break;
        }
      }
    }

    // check if the shadow ray intersects with any triangles
    for(int j = 0; j < num_triangles; j++)
    {
      Triangle currT = triangles[j];
      glm::vec3 intersectT(-MAXFLOAT, -MAXFLOAT, -MAXFLOAT);

      if(CheckTriangle(shadow, currT, intersectT) && j != triangleIndex) // if the shadow ray intersects with a triangle
      {
        // if the intersection is closer to the light than the triangle, the light is blocked
        if(glm::distance(intersectT, intersection) < glm::distance(lightPos, intersection))
        {
          hitsObjects = true;
          break;
        }
      }
    }

    // if the shadow ray doesn't hit anything, add the light to the color
    if(!hitsObjects)
    {
      // get the vertices of the triangle
      glm::vec3 C0(triangle.GetVertexPosByAxis(0, 0), triangle.GetVertexPosByAxis(0, 1), triangle.GetVertexPosByAxis(0, 2));
      glm::vec3 C1(triangle.GetVertexPosByAxis(1, 0), triangle.GetVertexPosByAxis(1, 1), triangle.GetVertexPosByAxis(1, 2));
      glm::vec3 C2(triangle.GetVertexPosByAxis(2, 0), triangle.GetVertexPosByAxis(2, 1), triangle.GetVertexPosByAxis(2, 2));

      // get all 3 edges
      glm::vec3 E1 = C1 - C0;
      glm::vec3 E2 = C2 - C1;
      glm::vec3 E3 = C0 - C2;
      
      float totalArea = FindArea(C0, C1, C2); // find the total area of the triangle (C0, C1, C2)
      float alpha = FindArea(intersection, C1, C2) / totalArea; // (C, C1, C2) / (C0, C1, C2)
      float beta = FindArea(C0, intersection, C2) / totalArea; // (C0, C, C2) / (C0, C1, C2)
      float gamma = 1 - (alpha + beta); 

      glm::vec3 triangleNormal; // interpolate normal
      triangleNormal.x = alpha * triangle.GetVertexNormalByAxis(0, 0) + beta * triangle.GetVertexNormalByAxis(1, 0) + gamma * triangle.GetVertexNormalByAxis(2, 0);
      triangleNormal.y = alpha * triangle.GetVertexNormalByAxis(0, 1) + beta * triangle.GetVertexNormalByAxis(1, 1) + gamma * triangle.GetVertexNormalByAxis(2, 1);
      triangleNormal.z = alpha * triangle.GetVertexNormalByAxis(0, 2) + beta * triangle.GetVertexNormalByAxis(1, 2) + gamma * triangle.GetVertexNormalByAxis(2, 2);
      triangleNormal = glm::normalize(triangleNormal); // normalize normal vector

      glm::vec3 specular; // interpolate specular color
      specular.x = alpha * triangle.GetVertexSpecularByAxis(0, 0) + beta * triangle.GetVertexSpecularByAxis(1, 0) + gamma * triangle.GetVertexSpecularByAxis(2, 0);
      specular.y = alpha * triangle.GetVertexSpecularByAxis(0, 1) + beta * triangle.GetVertexSpecularByAxis(1, 1) + gamma * triangle.GetVertexSpecularByAxis(2, 1);
      specular.z = alpha * triangle.GetVertexSpecularByAxis(0, 2) + beta * triangle.GetVertexSpecularByAxis(1, 2) + gamma * triangle.GetVertexSpecularByAxis(2, 2);

      glm::vec3 diffuse; // interpolate diffuse color
      diffuse.x = alpha * triangle.GetVertexColorByAxis(0, 0) + beta * triangle.GetVertexColorByAxis(1, 0) + gamma * triangle.GetVertexColorByAxis(2, 0);
      diffuse.y = alpha * triangle.GetVertexColorByAxis(0, 1) + beta * triangle.GetVertexColorByAxis(1, 1) + gamma * triangle.GetVertexColorByAxis(2, 1);
      diffuse.z = alpha * triangle.GetVertexColorByAxis(0, 2) + beta * triangle.GetVertexColorByAxis(1, 2) + gamma * triangle.GetVertexColorByAxis(2, 2);

      // interpolate shineness
      float shineness = alpha * triangle.GetVertexShineness(0) + beta * triangle.GetVertexShineness(1) + gamma * triangle.GetVertexShineness(2);

      // find reflection vector of triangleNormal and shadow direction
      float triNormaldotShadowDir = clamp(glm::dot(shadow.direction, triangleNormal)); // dot product of the normal and the shadow ray direction
      glm::vec3 reflect = (2 * triNormaldotShadowDir * triangleNormal) - shadow.direction; // r = 2(l * n)n - l (reflection vector)
      reflect = glm::normalize(reflect); // normalize reflection vector

      float reflMag = clamp(glm::dot(reflect, glm::normalize(-intersection))); // dot product of the reflection vector and the -intersection vector
      float reflMagToAlpha = pow(reflMag, shineness); // reflectionMagnitude ^ shineness

      // turn the current light's color into a vec3
      // glm::vec3 lightColor = glm::vec3(lights[l].color[0], lights[l].color[1], lights[l].color[2]);
      glm::vec3 lightColor = glm::vec3(softLights[l].color[0], softLights[l].color[1], softLights[l].color[2]);

      color += glm::vec3(lightColor * (diffuse * triNormaldotShadowDir + (specular * reflMagToAlpha)));
      color = glm::clamp(color, 0.0f, 1.0f); // clamp color values to be between 0 and 1
    }
  }

  return color;
}

glm::vec3 FindIntersection(Ray ray)
{
  glm::vec3 closestSphere(-MAXFLOAT, -MAXFLOAT, -MAXFLOAT); // start with the largest distance possible
  int closestSphereIndex = -1;

  for(int i = 0; i < num_spheres; i++)
  {
    Sphere currS = spheres[i]; 
    glm::vec3 intersectS(0, 0, closestSphere.z);

    if(CheckSphere(ray, currS, intersectS)) // if the ray intersects the current sphere
    {
      // change the closest sphere if the current sphere is closer (i.e. z value is closer to 0)
      if(intersectS.z > closestSphere.z)
      {
        closestSphere = intersectS;
        closestSphereIndex = i;
      }
    }
  }

  glm::vec3 closestTriangle(-MAXFLOAT, -MAXFLOAT, -MAXFLOAT); // start with the largest distance possible
  int closestTriangleIndex = -1;

  for(int j = 0; j < num_triangles; j++)
  {
    Triangle currT = triangles[j]; 
    glm::vec3 intersectT(0, 0, closestTriangle.z);

    if(CheckTriangle(ray, currT, intersectT)) // if the ray intersects the current triangle
    {
      // change the closest sphere if the current sphere is closer (i.e. z value is closer to 0)
      if(intersectT.z > closestTriangle.z)
      {
        closestTriangle = intersectT;
        closestTriangleIndex = j;
      }
    }
  }

  glm::vec3 color = glm::vec3(1, 1, 1); // start with white

  if(closestSphereIndex != -1 && closestSphere.z >= closestTriangle.z) // there is a sphere closer to the cam than the triangle
  {
    // find the sphere color
    color = FindSphereColor(spheres[closestSphereIndex], closestSphere, closestSphereIndex);

    // add ambient light
    color += glm::vec3(ambient_light[0], ambient_light[1], ambient_light[2]);
    color = glm::clamp(color, 0.0f, 1.0f); // clamp the color to 0-1
  }
  else if(closestTriangleIndex != -1 && closestTriangle.z >= closestSphere.z) // there is a triangle closer to the cam than the sphere
  {
    // find the triangle color
    color = FindTriangleColor(triangles[closestTriangleIndex], closestTriangle, closestTriangleIndex);

    // add ambient light
    color += glm::vec3(ambient_light[0], ambient_light[1], ambient_light[2]);
    color = glm::clamp(color, 0.0f, 1.0f); // clamp the color to 0-1
  }

  return color;
}

// !!! MODIFY THIS FUNCTION !!!
void draw_scene()
{ 
  for(unsigned int x = 0; x < WIDTH; x++)
  {
    glPointSize(2.0);  
    glBegin(GL_POINTS);

    for(unsigned int y = 0; y < HEIGHT; y++)
    {
      Ray r = ShootRay(x, y); // (0, 0, 0) --> (x, y, -1)
      glm::vec3 color = FindIntersection(r);
      plot_pixel(x, y, (int)(color.x * 255.0f), (int)(color.y * 255.0f), (int)(color.z * 255.0f));
    }
    glEnd();
    glFlush();
  }
  printf("Done!\n"); fflush(stdout);
}

void plot_pixel_display(int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
  glColor3f(((float)r) / 255.0f, ((float)g) / 255.0f, ((float)b) / 255.0f);
  glVertex2i(x,y);
}

void plot_pixel_jpeg(int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
  buffer[y][x][0] = r;
  buffer[y][x][1] = g;
  buffer[y][x][2] = b;
}

void plot_pixel(int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
  plot_pixel_display(x,y,r,g,b);
  if(mode == MODE_JPEG)
    plot_pixel_jpeg(x,y,r,g,b);
}

void save_jpg()
{
  printf("Saving JPEG file: %s\n", filename);

  ImageIO img(WIDTH, HEIGHT, 3, &buffer[0][0][0]);
  if (img.save(filename, ImageIO::FORMAT_JPEG) != ImageIO::OK)
    printf("Error in Saving\n");
  else 
    printf("File saved Successfully\n");
}

void parse_check(const char *expected, char *found)
{
  if(strcasecmp(expected,found))
  {
    printf("Expected '%s ' found '%s '\n", expected, found);
    printf("Parse error, abnormal abortion\n");
    exit(0);
  }
}

void parse_doubles(FILE* file, const char *check, double p[3])
{
  char str[100];
  fscanf(file,"%s",str);
  parse_check(check,str);
  fscanf(file,"%lf %lf %lf",&p[0],&p[1],&p[2]);
  printf("%s %lf %lf %lf\n",check,p[0],p[1],p[2]);
}

void parse_rad(FILE *file, double *r)
{
  char str[100];
  fscanf(file,"%s",str);
  parse_check("rad:",str);
  fscanf(file,"%lf",r);
  printf("rad: %f\n",*r);
}

void parse_shi(FILE *file, double *shi)
{
  char s[100];
  fscanf(file,"%s",s);
  parse_check("shi:",s);
  fscanf(file,"%lf",shi);
  printf("shi: %f\n",*shi);
}

int loadScene(char *argv)
{
  FILE * file = fopen(argv,"r");
  int number_of_objects;
  char type[50];
  Triangle t;
  Sphere s;
  Light l;
  fscanf(file,"%i", &number_of_objects);

  printf("number of objects: %i\n",number_of_objects);

  parse_doubles(file,"amb:",ambient_light);

  for(int i=0; i<number_of_objects; i++)
  {
    fscanf(file,"%s\n",type);
    printf("%s\n",type);
    if(strcasecmp(type,"triangle")==0)
    {
      printf("found triangle\n");
      for(int j=0;j < 3;j++)
      {
        parse_doubles(file,"pos:",t.v[j].position);
        parse_doubles(file,"nor:",t.v[j].normal);
        parse_doubles(file,"dif:",t.v[j].color_diffuse);
        parse_doubles(file,"spe:",t.v[j].color_specular);
        parse_shi(file,&t.v[j].shininess);
      }

      if(num_triangles == MAX_TRIANGLES)
      {
        printf("too many triangles, you should increase MAX_TRIANGLES!\n");
        exit(0);
      }
      triangles[num_triangles++] = t;
    }
    else if(strcasecmp(type,"sphere")==0)
    {
      printf("found sphere\n");

      parse_doubles(file,"pos:",s.position);
      parse_rad(file,&s.radius);
      parse_doubles(file,"dif:",s.color_diffuse);
      parse_doubles(file,"spe:",s.color_specular);
      parse_shi(file,&s.shininess);

      if(num_spheres == MAX_SPHERES)
      {
        printf("too many spheres, you should increase MAX_SPHERES!\n");
        exit(0);
      }
      spheres[num_spheres++] = s;
    }
    else if(strcasecmp(type,"light")==0)
    {
      printf("found light\n");
      parse_doubles(file,"pos:",l.position);
      parse_doubles(file,"col:",l.color);

      if(num_lights == MAX_LIGHTS)
      {
        printf("too many lights, you should increase MAX_LIGHTS!\n");
        exit(0);
      }
      lights[num_lights++] = l;
    }
    else
    {
      printf("unknown type in scene description:\n%s\n",type);
      exit(0);
    }
  }

  // setting up the soft-shadows!
  int newLightsPerOrig = 100;
  
  // convert original light source(s) into newLightsPerOrigin # of lights in random locations with intensity of 1/newLightsPerOrigin
  for(int i = 0; i < num_lights; i++)
  {
    glm::vec3 originalLightPos = glm::vec3(lights[i].position[0], lights[i].position[1], lights[i].position[2]);
    glm::vec3 originalLightIntensity = glm::vec3(lights[i].color[0], lights[i].color[1], lights[i].color[2]);
    
    for(int j = 0; j < newLightsPerOrig; j++)
    {
      // get a random float from -MAX_DISPLACEMENT_RADIUS to MAX_DISPLACEMENT_RADIUS
      double randomXDisplacement = ((double(rand()) / double(RAND_MAX)) * (2 * MAX_DISPLACEMENT_RADIUS)) - MAX_DISPLACEMENT_RADIUS;
      double randomYDisplacement = ((double(rand()) / double(RAND_MAX)) * (2 * MAX_DISPLACEMENT_RADIUS)) - MAX_DISPLACEMENT_RADIUS;
      double randomZDisplacement = ((double(rand()) / double(RAND_MAX)) * (2 * MAX_DISPLACEMENT_RADIUS)) - MAX_DISPLACEMENT_RADIUS;

      // shift x and y position, but not z!!! (so we make sure light hits the objects)
      double randLightPos[]  = {originalLightPos.x + randomXDisplacement,
      originalLightPos.y + randomYDisplacement,
      originalLightPos.z + randomZDisplacement};

      double randLightIntensity[] = {(1/(float)newLightsPerOrig) * originalLightIntensity.x,
      (1/(float)newLightsPerOrig) * originalLightIntensity.y,
      (1/(float)newLightsPerOrig) * originalLightIntensity.z};

      Light randLight;
      randLight.position[0] = randLightPos[0];
      randLight.position[1] = randLightPos[1];
      randLight.position[2] = randLightPos[2];

      randLight.color[0] = randLightIntensity[0];
      randLight.color[1] = randLightIntensity[1];
      randLight.color[2] = randLightIntensity[2];

      softLights[i * newLightsPerOrig + j] = randLight;

      num_soft_lights++;
    }
  }

  return 0;
}

void display()
{}

void init()
{
  glMatrixMode(GL_PROJECTION);
  glOrtho(0,WIDTH,0,HEIGHT,1,-1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glClearColor(0,0,0,0);
  glClear(GL_COLOR_BUFFER_BIT);
}

void idle()
{
  //hack to make it only draw once
  static int once=0;
  if(!once)
  {
    draw_scene();
    if(mode == MODE_JPEG)
      save_jpg();
  }
  once=1;
}

int main(int argc, char ** argv)
{
  if ((argc < 2) || (argc > 3))
  {  
    printf ("Usage: %s <input scenefile> [output jpegname]\n", argv[0]);
    exit(0);
  }
  if(argc == 3)
  {
    mode = MODE_JPEG;
    filename = argv[2];
  }
  else if(argc == 2)
    mode = MODE_DISPLAY;

  glutInit(&argc,argv);
  loadScene(argv[1]);

  glutInitDisplayMode(GLUT_RGBA | GLUT_SINGLE);
  glutInitWindowPosition(0,0);
  glutInitWindowSize(WIDTH,HEIGHT);
  int window = glutCreateWindow("Ray Tracer");
  #ifdef __APPLE__
    // This is needed on recent Mac OS X versions to correctly display the window.
    glutReshapeWindow(WIDTH - 1, HEIGHT - 1);
  #endif
  glutDisplayFunc(display);
  glutIdleFunc(idle);
  init();
  glutMainLoop();
}

