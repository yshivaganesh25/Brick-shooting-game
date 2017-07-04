#include <iostream>
#include <cmath>
#include <fstream>
#include <vector>
#include <string.h>
#include <cstring>
#include <string>
#include <pthread.h>

#include <stdlib.h>
#include <assert.h>
#include <ao/ao.h>


#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace std;

static const int BUF_SIZE = 4096;

struct WavHeader {
    char id[4]; //should contain RIFF
    int32_t totalLength;
    char wavefmt[8];
    int32_t format; // 16 for PCM
    int16_t pcm; // 1 for PCM
    int16_t channels;
    int32_t frequency;
    int32_t bytesPerSecond;
    int16_t bytesByCapture;
    int16_t bitsPerSample;
    char data[4]; // "data"
    int32_t bytesInData;
};

struct VAO {
    GLuint VertexArrayID;
    GLuint VertexBuffer;
    GLuint ColorBuffer;

    GLenum PrimitiveMode;
    GLenum FillMode;
    int NumVertices;

    float x,y,z,inclination,angle; // for mirror
    bool active,reflection,drag;
    int brickcount;
    int color;

};
typedef struct VAO VAO;

struct GLMatrices {
	glm::mat4 projection;
	glm::mat4 model;
	glm::mat4 view;
	GLuint MatrixID;
} Matrices;

GLuint programID;

/* Function to load Shaders - Use it as it is */
GLuint LoadShaders(const char * vertex_file_path,const char * fragment_file_path) {

	// Create the shaders
	GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

	// Read the Vertex Shader code from the file
	std::string VertexShaderCode;
	std::ifstream VertexShaderStream(vertex_file_path, std::ios::in);
	if(VertexShaderStream.is_open())
	{
		std::string Line = "";
		while(getline(VertexShaderStream, Line))
			VertexShaderCode += "\n" + Line;
		VertexShaderStream.close();
	}

	// Read the Fragment Shader code from the file
	std::string FragmentShaderCode;
	std::ifstream FragmentShaderStream(fragment_file_path, std::ios::in);
	if(FragmentShaderStream.is_open()){
		std::string Line = "";
		while(getline(FragmentShaderStream, Line))
			FragmentShaderCode += "\n" + Line;
		FragmentShaderStream.close();
	}

	GLint Result = GL_FALSE;
	int InfoLogLength;

	// Compile Vertex Shader
	printf("Compiling shader : %s\n", vertex_file_path);
	char const * VertexSourcePointer = VertexShaderCode.c_str();
	glShaderSource(VertexShaderID, 1, &VertexSourcePointer , NULL);
	glCompileShader(VertexShaderID);

	// Check Vertex Shader
	glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> VertexShaderErrorMessage(InfoLogLength);
	glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
	fprintf(stdout, "%s\n", &VertexShaderErrorMessage[0]);

	// Compile Fragment Shader
	printf("Compiling shader : %s\n", fragment_file_path);
	char const * FragmentSourcePointer = FragmentShaderCode.c_str();
	glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer , NULL);
	glCompileShader(FragmentShaderID);

	// Check Fragment Shader
	glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> FragmentShaderErrorMessage(InfoLogLength);
	glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]);
	fprintf(stdout, "%s\n", &FragmentShaderErrorMessage[0]);

	// Link the program
	fprintf(stdout, "Linking program\n");
	GLuint ProgramID = glCreateProgram();
	glAttachShader(ProgramID, VertexShaderID);
	glAttachShader(ProgramID, FragmentShaderID);
	glLinkProgram(ProgramID);

	// Check the program
	glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
	glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> ProgramErrorMessage( max(InfoLogLength, int(1)) );
	glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
	fprintf(stdout, "%s\n", &ProgramErrorMessage[0]);

	glDeleteShader(VertexShaderID);
	glDeleteShader(FragmentShaderID);

	return ProgramID;
}

static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}

void quit(GLFWwindow *window)
{
    glfwDestroyWindow(window);
    glfwTerminate();
//    exit(EXIT_SUCCESS);
}


/* Generate VAO, VBOs and return VAO handle */
struct VAO* create3DObject (GLenum primitive_mode, int numVertices, const GLfloat* vertex_buffer_data, const GLfloat* color_buffer_data, GLenum fill_mode=GL_FILL)
{
    struct VAO* vao = new struct VAO;
    vao->PrimitiveMode = primitive_mode;
    vao->NumVertices = numVertices;
    vao->FillMode = fill_mode;

    // Create Vertex Array Object
    // Should be done after CreateWindow and before any other GL calls
    glGenVertexArrays(1, &(vao->VertexArrayID)); // VAO
    glGenBuffers (1, &(vao->VertexBuffer)); // VBO - vertices
    glGenBuffers (1, &(vao->ColorBuffer));  // VBO - colors

    glBindVertexArray (vao->VertexArrayID); // Bind the VAO
    glBindBuffer (GL_ARRAY_BUFFER, vao->VertexBuffer); // Bind the VBO vertices
    glBufferData (GL_ARRAY_BUFFER, 3*numVertices*sizeof(GLfloat), vertex_buffer_data, GL_STATIC_DRAW); // Copy the vertices into VBO
    glVertexAttribPointer(
                          0,                  // attribute 0. Vertices
                          3,                  // size (x,y,z)
                          GL_FLOAT,           // type
                          GL_FALSE,           // normalized?
                          0,                  // stride
                          (void*)0            // array buffer offset
                          );

    glBindBuffer (GL_ARRAY_BUFFER, vao->ColorBuffer); // Bind the VBO colors
    glBufferData (GL_ARRAY_BUFFER, 3*numVertices*sizeof(GLfloat), color_buffer_data, GL_STATIC_DRAW);  // Copy the vertex colors
    glVertexAttribPointer(
                          1,                  // attribute 1. Color
                          3,                  // size (r,g,b)
                          GL_FLOAT,           // type
                          GL_FALSE,           // normalized?
                          0,                  // stride
                          (void*)0            // array buffer offset
                          );

    return vao;
}

/* Generate VAO, VBOs and return VAO handle - Common Color for all vertices */
struct VAO* create3DObject (GLenum primitive_mode, int numVertices, const GLfloat* vertex_buffer_data, const GLfloat red, const GLfloat green, const GLfloat blue, GLenum fill_mode=GL_FILL)
{
    GLfloat* color_buffer_data = new GLfloat [3*numVertices];
    for (int i=0; i<numVertices; i++) {
        color_buffer_data [3*i] = red;
        color_buffer_data [3*i + 1] = green;
        color_buffer_data [3*i + 2] = blue;
    }

    return create3DObject(primitive_mode, numVertices, vertex_buffer_data, color_buffer_data, fill_mode);
}

/* Render the VBOs handled by VAO */
void draw3DObject (struct VAO* vao)
{
    // Change the Fill Mode for this object
    glPolygonMode (GL_FRONT_AND_BACK, vao->FillMode);

    // Bind the VAO to use
    glBindVertexArray (vao->VertexArrayID);

    // Enable Vertex Attribute 0 - 3d Vertices
    glEnableVertexAttribArray(0);
    // Bind the VBO to use
    glBindBuffer(GL_ARRAY_BUFFER, vao->VertexBuffer);

    // Enable Vertex Attribute 1 - Color
    glEnableVertexAttribArray(1);
    // Bind the VBO to use
    glBindBuffer(GL_ARRAY_BUFFER, vao->ColorBuffer);

    // Draw the geometry !
    glDrawArrays(vao->PrimitiveMode, 0, vao->NumVertices); // Starting from vertex 0; 3 vertices total -> 1 triangle
}

/**************************
 * Customizable functions *
 **************************/
VAO *bask1, *bask2, *cannon_base, *cannon_gun, *brick, *segment,*laser,*line;
vector<VAO*> circle,lasers,mirrors,bricks;
vector<VAO*> baskcircle1,baskcircle2,smcircle,b2circle,b1circle;

float triangle_rot_dir = 1;
bool triangle_rot_status = true;
float triangle_rotation = 1.0;
bool cannon_gun_rot_status = true;
float cannon_gun_rot_dir = 1;
float cannon_gun_rotation = 0;
float camera_rotation_angle = 90;
float bask1_rotation = 0;
int new_laser_index = -1;
bool cannon_active = true;
double latest_cannonfire_time = 0;
int totalbrickcount = 0;
float brick_falling_frequency = 0.25;
double xpos, ypos;
float mxpos,mypos;
bool mouse_left_drag = false,mouse_right_drag = false;
int drag_basket;
float bx = 10.0f,bnx = -10.0f,by = 10.0f, bny = -10.0f;
bool ctrlflag = false,altflag=false;
bool gameover = false;
int redbrickshit = 0,greenbrickshit = 0;
int score = 0;
int fbwidth = 600,fbheight = 600;



void *playaudio (void* parg)
{
  const char* filepath = (const char*)parg;
  ao_device* device;
  ao_sample_format format;
  int defaultDriver;
  WavHeader header;

  std::ifstream file;
  file.open(filepath, std::ios::binary | std::ios::in);

  file.read(header.id, sizeof(header.id));
  assert(!std::memcmp(header.id, "RIFF", 4)); //is it a WAV file?
  file.read((char*)&header.totalLength, sizeof(header.totalLength));
  file.read(header.wavefmt, sizeof(header.wavefmt)); //is it the right format?
  assert(!std::memcmp(header.wavefmt, "WAVEfmt ", 8));
  file.read((char*)&header.format, sizeof(header.format));
  file.read((char*)&header.pcm, sizeof(header.pcm));
  file.read((char*)&header.channels, sizeof(header.channels));
  file.read((char*)&header.frequency, sizeof(header.frequency));
  file.read((char*)&header.bytesPerSecond, sizeof(header.bytesPerSecond));
  file.read((char*)&header.bytesByCapture, sizeof(header.bytesByCapture));
  file.read((char*)&header.bitsPerSample, sizeof(header.bitsPerSample));
  file.read(header.data, sizeof(header.data));
  file.read((char*)&header.bytesInData, sizeof(header.bytesInData));

  ao_initialize();

  defaultDriver = ao_default_driver_id();

  memset(&format, 0, sizeof(format));
  format.bits = header.format;
  format.channels = header.channels;
  format.rate = header.frequency;
  format.byte_format = AO_FMT_LITTLE;

  device = ao_open_live(defaultDriver, &format, NULL);
  if (device == NULL) {
      std::cout << "Unable to open driver" << std::endl;

  }

  char* buffer = (char*)malloc(BUF_SIZE * sizeof(char));

  // determine how many BUF_SIZE chunks are in file
  int fSize = header.bytesInData;
  int bCount = fSize / BUF_SIZE;

  for (int i = 0; i < bCount; ++i) {
      file.read(buffer, BUF_SIZE);
      ao_play(device, buffer, BUF_SIZE);
  }

  int leftoverBytes = fSize % BUF_SIZE;
  //std::cout << leftoverBytes;
  file.read(buffer, leftoverBytes);
  memset(buffer + leftoverBytes, 0, BUF_SIZE - leftoverBytes);
  ao_play(device, buffer, BUF_SIZE);
  ao_close(device);
  ao_shutdown();


}
void playwav (const char* x)
{
  pthread_t      tid;  // thread ID
  pthread_attr_t attr; // thread attribute

  // set thread detachstate attribute to DETACHED
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  // create the thread
  const char* parg = x;
  pthread_create(&tid, &attr, playaudio, (void *)parg);

}
/*executed when something is pressed*/

void keyboard (GLFWwindow* window, int key, int scancode, int action, int mods)
{
     // Function is called first on GLFW_PRESS.

    if (action == GLFW_RELEASE) {
        switch (key) {
            case GLFW_KEY_LEFT_CONTROL:
                ctrlflag = false;
                break;
            case GLFW_KEY_LEFT_ALT:
                altflag = false;
                break;


            default:
                break;
        }
    }
    else if ((action == GLFW_REPEAT)||(action == GLFW_PRESS)) {
        switch (key) {
            case GLFW_KEY_ESCAPE:
                quit(window);
                break;

            case GLFW_KEY_LEFT_CONTROL:
                ctrlflag = true;
                break;
            case GLFW_KEY_LEFT_ALT:
                altflag = true;
                break;

            case GLFW_KEY_SPACE :
                if ((action == GLFW_PRESS) && (cannon_active))
                {
                  new_laser_index = (new_laser_index + 1) % lasers.size();
                  lasers[new_laser_index]->inclination = cannon_gun_rotation;
                  lasers[new_laser_index]->x = cannon_gun->x + 2*cos(lasers[new_laser_index]->inclination*M_PI/180.0f);
                  lasers[new_laser_index]->y = cannon_gun->y + 2*sin(lasers[new_laser_index]->inclination*M_PI/180.0f);
                  lasers[new_laser_index]->z = cannon_gun->z;
                  lasers[new_laser_index]->active = true;
                  cannon_active = false;
                  latest_cannonfire_time = glfwGetTime();
                }
                //playaudio("laser.wav");
                break;

            case GLFW_KEY_M :
                brick_falling_frequency -= 0.1;
                if (brick_falling_frequency < 0.05)
                {
                  brick_falling_frequency = 0.05;
                }
                break;

            case GLFW_KEY_N :
                brick_falling_frequency += 0.1;
                if (brick_falling_frequency > 0.45)
                {
                  brick_falling_frequency = 0.45;
                }
                break;

            case GLFW_KEY_S :

                if (cannon_gun->y <= -5.5)
                {
                  cannon_gun->y = -5.5;
                }
                cannon_gun->y = cannon_gun->y <= -5.5 ? -5.5 : cannon_gun->y  - 0.5;

                for(int j = 0 ; j < circle.size() ; j++)
                {
                  circle[j]->y = circle[j]->y <= -5.5 ? -5.5 : circle[j]->y  - 0.5;
                }
                for(int j = 0 ; j < smcircle.size() ; j++)
                {
                  smcircle[j]->y = smcircle[j]->y <= -5.5 ? -5.5 : smcircle[j]->y  - 0.5;
                }

                break;

            case GLFW_KEY_F :

                cannon_gun->y = cannon_gun->y >=8 ? 8 : cannon_gun->y  + 0.5;

                for(int j = 0 ; j < circle.size() ; j++)
                {
                  circle[j]->y = circle[j]->y >=8 ? 8 : circle[j]->y  + 0.5;
                }
                for(int j = 0 ; j < smcircle.size() ; j++)
                {
                  smcircle[j]->y = smcircle[j]->y >=8 ? 8 : smcircle[j]->y  + 0.5;
                }

                break;

            case GLFW_KEY_A :
                cannon_gun_rot_status = true;
                cannon_gun_rot_dir = 1;
                if ( ((cannon_gun_rotation < 90 )&&(cannon_gun_rotation > -90))
                     || ((cannon_gun_rotation == 90) && (cannon_gun_rot_dir == -1))
                     || ((cannon_gun_rotation == -90) && (cannon_gun_rot_dir == 1))
                   )
                {
                    cannon_gun_rotation = cannon_gun_rotation + cannon_gun_rot_dir*cannon_gun_rot_status;
                }
                break;

            case GLFW_KEY_D :
                cannon_gun_rot_status = true;
                cannon_gun_rot_dir = -1;
                if ( ((cannon_gun_rotation < 90 )&&(cannon_gun_rotation > -90))
                     || ((cannon_gun_rotation == 90) && (cannon_gun_rot_dir == -1))
                     || ((cannon_gun_rotation == -90) && (cannon_gun_rot_dir == 1))
                   )
                {
                    cannon_gun_rotation = cannon_gun_rotation + cannon_gun_rot_dir*cannon_gun_rot_status;
                }
                break;

            case GLFW_KEY_LEFT :
                if (altflag)
                {
                  bask1->x -= 0.5;
                  for (int j = 0 ; j < baskcircle1.size(); j ++)
                  {
                    baskcircle1[j]->x -= 0.5;
                  }
                  for (int j = 0 ; j < b1circle.size(); j ++)
                  {
                    b1circle[j]->x -= 0.5;
                  }
                  if (bask1->x <= -8.5)
                  {
                    bask1->x = -8.5;
                    for (int j = 0 ; j < baskcircle1.size(); j ++)
                    {
                      baskcircle1[j]->x = -8.5;
                    }
                    for (int j = 0 ; j < b1circle.size(); j ++)
                    {
                      b1circle[j]->x = -8.5;
                    }
                  }
                }
                else if (ctrlflag)
                {
                  bask2->x -= 0.5;
                  for (int j = 0 ; j < baskcircle2.size(); j ++)
                  {
                    baskcircle2[j]->x -= 0.5;
                  }
                  for (int j = 0 ; j < b2circle.size(); j ++)
                  {
                    b2circle[j]->x -= 0.5;
                  }
                  if (bask2->x <= -8.5)
                  {
                    bask2->x = -8.5;
                    for (int j = 0 ; j < baskcircle2.size(); j ++)
                    {
                      baskcircle2[j]->x = -8.5;
                    }
                    for (int j = 0 ; j < b2circle.size(); j ++)
                    {
                      b2circle[j]->x = -8.5;
                    }
                  }
                }
                else
                {
                  if (bnx - 1 >= -10)
                  {
                    bx--;
                    bnx--;
                  }
                  Matrices.projection = glm::ortho(bnx, bx, bny, by, 0.1f, 500.0f);
                }

                break;
            case GLFW_KEY_RIGHT :

                if (altflag)
                {
                  bask1->x += 0.5;
                  for (int j = 0 ; j < baskcircle1.size(); j ++)
                  {
                    baskcircle1[j]->x += 0.5;
                  }
                  for (int j = 0 ; j < b1circle.size(); j ++)
                  {
                    b1circle[j]->x += 0.5;
                  }
                  if (bask1->x >= 8.5)
                  {
                    for (int j = 0 ; j < baskcircle1.size(); j ++)
                    {
                      baskcircle1[j]->x = 8.5;
                    }
                    for (int j = 0 ; j < b1circle.size(); j ++)
                    {
                      b1circle[j]->x = 8.5;
                    }
                    bask1->x = 8.5;
                  }
                }
                else if (ctrlflag)
                {
                  bask2->x += 0.5;
                  for (int j = 0 ; j < baskcircle2.size(); j ++)
                  {
                    baskcircle2[j]->x += 0.5;
                  }
                  for (int j = 0 ; j < b2circle.size(); j ++)
                  {
                    b2circle[j]->x += 0.5;
                  }
                  if (bask2->x >= 8.5)
                  {
                    bask2->x = 8.5;
                    for (int j = 0 ; j < baskcircle2.size(); j ++)
                    {
                      baskcircle2[j]->x = 8.5;
                    }
                    for (int j = 0 ; j < b2circle.size(); j ++)
                    {
                      b2circle[j]->x = 8.5;
                    }
                  }
                }
                else
                {
                  if (bx + 1 <= 10 )
                  {
                    bx++;
                    bnx++;
                  }
                  Matrices.projection = glm::ortho(bnx, bx, bny, by, 0.1f, 500.0f);
                }

                break;

            case GLFW_KEY_UP :
                bnx = bnx >= -5 ? -5 : bnx + 1;
                bny = bny >= -5 ? -5 : bny + 1;
                bx = bx <= 5 ? 5 : bx - 1;
                by = by <= 5 ? 5 : by - 1;
                Matrices.projection = glm::ortho(bnx, bx, bny, by, 0.1f, 500.0f);
                break;

            case GLFW_KEY_DOWN :

                bnx = bnx <= -10 ? -10 : bnx - 1;
                bny = bny <= -10 ? -10 : bny - 1;
                bx = bx >= 10 ? 10 : bx + 1;
                by = by >= 10 ? 10 : by + 1;
                Matrices.projection = glm::ortho(bnx, bx, bny, by, 0.1f, 500.0f);
                break;

            default:
                break;
        }
    }
}

/* Executed for character input (like in text boxes) */
void keyboardChar (GLFWwindow* window, unsigned int key)
{
	switch (key) {
		case 'Q':
		case 'q':
            quit(window);
            break;
		default:
			break;
	}
}

float parse (float x)
{
  float integer,frac = modf(x, &integer);
//  cout << integer << "+ " << frac << endl;
  if (frac >= 0.75 )
  {
    return integer + 1;
  }
  else if (frac < 0.75 && frac >=0.25)
  {
    return integer + 0.5;
  }
  else if (frac < 0.25 && frac >= -0.25)
  {
    return integer;
  }
  else if (frac < -0.25 && frac >= -0.75)
  {
    return integer - 0.5;
  }
  else if ( frac <= -0.75)
  {
    return integer - 1;
  }
}

/* Executed when a mouse button is pressed/released */
void mouseButton (GLFWwindow* window, int button, int action, int mods)
{
    switch (button) {
        case GLFW_MOUSE_BUTTON_LEFT:

            if (action == GLFW_PRESS)
            {
              mouse_left_drag = true;

              if (xpos >= -9 && ypos > -7.5 && cannon_active
                  && (sqrt((cannon_gun->x-xpos)*(cannon_gun->x-xpos)
                  +(cannon_gun->y-ypos)*(cannon_gun->y-ypos)) > 1)
                  )
              {
                cannon_gun_rotation = atan((double)(ypos - cannon_gun->y)/
                                              (xpos - cannon_gun->x))*180/M_PI;
                new_laser_index = (new_laser_index + 1) % lasers.size();
                lasers[new_laser_index]->inclination = cannon_gun_rotation;
                lasers[new_laser_index]->x = cannon_gun->x + 2*cos(lasers[new_laser_index]->inclination*M_PI/180.0f);
                lasers[new_laser_index]->y = cannon_gun->y + 2*sin(lasers[new_laser_index]->inclination*M_PI/180.0f);
                lasers[new_laser_index]->z = cannon_gun->z;
                lasers[new_laser_index]->active = true;
                cannon_active = false;
                latest_cannonfire_time = glfwGetTime();

                playwav("laser.wav");

              }


            }

            if (action == GLFW_RELEASE)
            {
            //  cout << "released" << endl;

              mouse_left_drag = false;

              if (cannon_gun->drag == true)
              {
                cannon_gun->drag = false;
                cannon_gun->y = parse(cannon_gun->y);
                //cout << "final " << cannon_gun->y <<endl;
                for (int j = 0 ; j < circle.size() ; j++)
                {
                  circle[j]->drag = false;
                  circle[j]->y = parse(circle[j]->y);
                }
                for (int j = 0 ; j < smcircle.size() ; j++)
                {
                  smcircle[j]->drag = false;
                  smcircle[j]->y = parse(smcircle[j]->y);
                }
              }
              if (bask1->drag == true)
              {
                bask1->drag = false;
                bask1->x = parse(bask1->x);
                for ( int j = 0 ;j < baskcircle1.size() ; j++)
                {
                  baskcircle1[j]->drag = false;
                  baskcircle1[j]->x = parse(baskcircle1[j]->x);
                }
                for ( int j = 0 ;j < b1circle.size() ; j++)
                {
                  b1circle[j]->drag = false;
                  b1circle[j]->x = parse(b1circle[j]->x);
                }

              }

              if (bask2->drag == true)
              {
                bask2->drag = false;
                bask2->x = parse(bask2->x);
                for ( int j = 0 ;j < baskcircle2.size() ; j++)
                {
                  baskcircle2[j]->drag = false;
                  baskcircle2[j]->x = parse(baskcircle2[j]->x);
                }
                for ( int j = 0 ;j < b2circle.size() ; j++)
                {
                  b2circle[j]->drag = false;
                  b2circle[j]->x = parse(b2circle[j]->x);
                }

              }

            }

            break;

        case GLFW_MOUSE_BUTTON_RIGHT:
            if (action == GLFW_PRESS)
            {
              mouse_right_drag = true;
              mxpos = xpos;
              mypos = ypos;

            }
            if (action == GLFW_RELEASE)
            {
                mouse_right_drag = false;
            }
            break;
        default:
            break;
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
  if (yoffset == 1)
  {
    bnx = bnx >= -5 ? -5 : bnx + 1;
    bny = bny >= -5 ? -5 : bny + 1;
    bx = bx <= 5 ? 5 : bx - 1;
    by = by <= 5 ? 5 : by - 1;
    Matrices.projection = glm::ortho(bnx, bx, bny, by, 0.1f, 500.0f);
  }
  else if (yoffset == -1)
  {
    bnx = bnx <= -10 ? -10 : bnx - 1;
    bny = bny <= -10 ? -10 : bny - 1;
    bx = bx >= 10 ? 10 : bx + 1;
    by = by >= 10 ? 10 : by + 1;
    Matrices.projection = glm::ortho(bnx, bx, bny, by, 0.1f, 500.0f);
  }
}

void cursor_pos_callback(GLFWwindow* window, double x, double y)
{

  xpos = x/(fbwidth/20.0) -10;
  ypos = -(y/(fbheight/20.0) -10);
  //cout << fbheight <<" "<<  fbwidth <<endl;
}



/* Executed when window is resized to 'width' and 'height' */
/* Modify the bounds of the screen here in glm::ortho or Field of View in glm::Perspective */
void reshapeWindow (GLFWwindow* window, int width, int height)
{
    fbwidth=width, fbheight=height;
    /* With Retina display on Mac OS X, GLFW's FramebufferSize
     is different from WindowSize */
    glfwGetFramebufferSize(window, &fbwidth, &fbheight);

	GLfloat fov = 90.0f;

	// sets the viewport of openGL renderer
	glViewport (0, 0, (GLsizei) fbwidth, (GLsizei) fbheight);

	// set the projection matrix as perspective
	/* glMatrixMode (GL_PROJECTION);
	   glLoadIdentity ();
	   gluPerspective (fov, (GLfloat) fbwidth / (GLfloat) fbheight, 0.1, 500.0); */
	// Store the projection matrix in a variable for future use
    // Perspective projection for 3D views
    // Matrices.projection = glm::perspective (fov, (GLfloat) fbwidth / (GLfloat) fbheight, 0.1f, 500.0f);

    // Ortho projection for 2D views
    Matrices.projection = glm::ortho(bnx, bx, bny, by, 0.1f, 500.0f);
}



VAO* createsegment(GLfloat v1, GLfloat v2 , GLfloat v3,
                    GLfloat v4, GLfloat v5, GLfloat v6,
                    GLfloat c1, GLfloat c2, GLfloat c3,
                    GLfloat c4, GLfloat c5, GLfloat c6,
                    GLfloat c7, GLfloat c8, GLfloat c9,
                    float x,float y,float z, GLfloat r
                  )
{
  GLfloat vertex_buffer_data [] = {
    v1, v2,v3, // vertex 0
    v4,r,v6, // vertex 1
    r*sin(10*M_PI/180.0f),r*cos(10*M_PI/180.0f),0, // vertex 2
  };

  GLfloat color_buffer_data [] = {
    c1,c2 ,c3, // color 0
    c4,c5,c6, // color 1
    c7,c8,c9, // color 2
  };

  // create3DObject creates and returns a handle to a VAO that can be used later
  segment = create3DObject(GL_TRIANGLES, 3, vertex_buffer_data, color_buffer_data, GL_FILL);
  segment->x = x;
  segment->y = y;
  segment->z = z;
  segment->drag = false;

  return segment;
}


void createcirclebottom1()
{
  for (int i = 0 ; i < 37 ; i++)
  {
    b1circle.push_back(createsegment(0,0,0, 0,1.5,0,
      1,0,0, 1,0,0, 1,0,0, 5,-9.5,0, 1.5));
  }
}

void createcirclebottom2()
{
  for (int i = 0; i < 37; i++)
  {
    b2circle.push_back(createsegment(0,0,0, 0,1.5,0, 0,1,0, 0,1,0, 0,1,0, -5,-9.5,0, 1.5));
  }
}

void createsmallcircle()
{
  for (int i = 0 ; i < 37 ; i++)
  {
    smcircle.push_back(createsegment(0,0,0, 0,0.5,0, 0.8,0.3,1, 0.8,0.3,1, 0.8,0.3,1, -9,0,0, 0.5));
  }
}

void createcircle ()
{
  int i = 0;
  for( i = 0 ; i < 37 ; i++)
  {
    circle.push_back(createsegment(0,0,0, 0,1,0, 0.6,0.2,0, 0.6,0.2,0, 0.6,0.2,0, -9,0,0, 1));
  }
}

void createbaskcircle ()
{
  int i = 0;
  for ( i = 0 ; i < 37 ; i++)
  {
    baskcircle1.push_back(createsegment(0,0,0, 0,1.5,0, 0,0,0,0,0,0,0,0,0, 5,-8,0, 1.5));
    baskcircle2.push_back(createsegment(0,0,0, 0,1.5,0, 0,0,0,0,0,0,0,0,0, -5,-8,0, 1.5));
  }
}


// Creates the triangle object used in this sample code
VAO* createTriangle (
                      GLfloat v1, GLfloat v2 , GLfloat v3,
                      GLfloat v4, GLfloat v5, GLfloat v6,
                      GLfloat v7, GLfloat v8, GLfloat v9,
                      GLfloat c1, GLfloat c2, GLfloat c3,
                      GLfloat c4, GLfloat c5, GLfloat c6,
                      GLfloat c7, GLfloat c8, GLfloat c9,
                      float x,float y,float z
)
{
  VAO* triangle;
  GLfloat vertex_buffer_data [] = {
    v1,v2,v3,
    v4,v5,v6,
    v7,v8,v9,
  };

  GLfloat color_buffer_data [] = {
    c1,c2,c3,
    c4,c5,c6,
    c7,c8,c9,
  };

  triangle = create3DObject(GL_TRIANGLES, 3, vertex_buffer_data, color_buffer_data, GL_LINE );
  triangle->x = x;
  triangle->y = y;
  triangle->z = z;
  triangle->drag = false;

  return triangle;

}


void createline ()
{
  line = createTriangle(-10,0,0, 0,0,0, 10,0,0, 0,0,0,  0,0,0, 0,0,0,  0,-7.5 , 0 );
}


void createlasers()
{
  for (int i = 0 ; i < 20 ; i++)
  {
    VAO* laser = createTriangle(0,0,0, 1,0,0, 1,0,0, 1,0,0, 1,0,0, 1,0,0, 0, 0, 0);
    laser->active = false;
    laser->reflection = false;
    lasers.push_back(laser);
  }
}


VAO* createrectangle (GLfloat v1, GLfloat v2 , GLfloat v3,
                      GLfloat v4, GLfloat v5, GLfloat v6,
                      GLfloat v7, GLfloat v8, GLfloat v9,
                      GLfloat v10, GLfloat v11, GLfloat v12,
                      GLfloat c1, GLfloat c2, GLfloat c3,
                      GLfloat c4, GLfloat c5, GLfloat c6,
                      GLfloat c7, GLfloat c8, GLfloat c9,
                      GLfloat c10, GLfloat c11, GLfloat c12,
                      float x,float y,float z
                    )
{
  VAO* rectangle;

  GLfloat vertex_buffer_data [] = {
    v1,v2,v3, // vertex 1
    v4,v5,v6, // vertex 2
    v7, v8,v9, // vertex 3

    v7, v8,v9, // vertex 3
    v10, v11,v12, // vertex 4
    v1,v2,v3  // vertex 1
  };

  GLfloat color_buffer_data1 [] = {
    c1,c2,c3, // vertex 1
    c4,c5,c6, // vertex 2
    c7, c8,c9, // vertex 3

    c7, c8,c9, // vertex 3
    c10, c11,c12, // vertex 4
    c1,c2,c3  // vertex 1
 };

 rectangle = create3DObject(GL_TRIANGLES, 6, vertex_buffer_data, color_buffer_data1, GL_FILL);
 rectangle->x = x;
 rectangle->y = y;
 rectangle->z = z;
 rectangle->active = true;
 rectangle->drag = false;

 return rectangle;

}

VAO* createmirror(float x, float y, float z,float tita)
{
  VAO* mirror = createrectangle(-1,-0.2,0 ,1,-0.2,0 ,1,0,0 ,-1,0,0,
                    0.6,0.7,0.6, 0.6,0.7,0.6, 0.6,0.7,0.6, 0.6,0.7,0.6, x,y,z);
  mirror->angle = tita;
  return mirror;
}

void createmirrors()
{
  mirrors.push_back(createmirror(0,0,0,30));
  mirrors.push_back(createmirror(0,5,0,150));
  mirrors.push_back(createmirror(5,-5,0,45));
}

VAO* createbrick (int colour, GLfloat X)
{
  VAO* Brick;
  if (colour == 2)//blue
  {
    Brick = createrectangle(0,0,0, 0.7,0,0, 0.7,0.7,0,  0,0.7,0,
                              0,1,0, 0,1,0, 0,1,0, 0,1,0, X,10.0,0);
    Brick->color = 2;
  }
  else if (colour == 1)//red
  {
    Brick = createrectangle(0,0,0, 0.7,0,0, 0.7,0.7,0,  0,0.7,0,
                              1,0,0, 1,0,0, 1,0,0, 1,0,0, X,10.0,0);
      Brick->color = 1;
  }
  else if (colour == 0)//black
  {
    Brick = createrectangle(0,0,0, 0.7,0,0, 0.7,0.7,0,  0,0.7,0,
                              0,0,0, 0,0,0, 0,0,0, 0,0,0, X,10.0,0);
    Brick->color = 0;
  }
  return Brick;

}

void createbricks ()
{
  int Y = rand()%8 + 1;
  //cout << "Y =" <<Y << endl;
  vector<GLfloat> a;
  for (int j =0 ; j < Y ; j++)
  {
    GLfloat X = rand()%17 - 7;
    int colour = rand()%3;
    //cout << "X = " << X <<endl;
    int k = 0;
    for ( k = 0 ; k < a.size(); k++)
    {
      if (a[k] == X)
      {
        break;
      }
    }
    int u = 0;
    for ( u =0 ; u < mirrors.size();u++)
    {
      if ( X <= mirrors[u]->x + fabs(cos(mirrors[u]->angle*M_PI/180.0f)) &&
           X >= mirrors[u]->x - fabs(cos(mirrors[u]->angle*M_PI/180.0f))-0.7
          )
          {
            break;
          }
    }
    if ( k == a.size() && u == mirrors.size())
    {
      bricks.push_back(createbrick(colour,X));
      a.push_back(X);
    }

  }

  totalbrickcount += Y;


}


void createcannon ()
{
  cannon_gun = createrectangle( 0,-0.2,0, 2,-0.2,0, 2, 0.2,0, 0, 0.2,0,
                                0.8,0.3,1, 0.8,0.3,1, 0.8,0.3,1, 0.8,0.3,1, -9.0,0.0,0.0 );

}

// Creates the rectangle object used in this sample code
void createbaskets ()
{
  bask1 = createrectangle(-1.5,-0.5,0, 1.5,-0.5,0, 1.5, 1,0, -1.5,1,0,
                          1,0,0, 1,0,0, 1,0,0, 1,0,0, 5.0,-9.0,0.0 );
  bask1->active = true;
  bask1->brickcount = 0;
  bask2 = createrectangle(-1.5,-0.5,0, 1.5,-0.5,0, 1.5, 1,0, -1.5,1,0,
                          0,1,0, 0,1,0, 0,1,0, 0,1,0, -5.0,-9.0,0.0 );
  bask2->active = true;
  bask2->brickcount = 0;
}

void checkcollisionbtwbaskets()
{
  if (fabs(bask1->x - bask2->x) < 3)
  {
    bask1->active = false;
    bask2->active = false;
    //cout << "not active " << endl;
  }
  else
  {
    bask1->active = true;
    bask2->active = true;
    //cout << "active" << endl;
  }
}

VAO* checkcollisionbtwbrickbasket(VAO* Brick)
{
  if (Brick->y == -8.0 )
  {
    if (((Brick->x >= bask2->x - 1.5)&&(Brick->x <=bask2->x + 0.8))
          &&(bask2->active == true)&&(Brick->active == true))
    {
      if (Brick->color == 2)
      {
        Brick->active = false;
        bask2->brickcount++;
        score += 20;
        cout << "score " << score << endl;
      }
      else if (Brick->color == 0)
      {
        gameover = true;
      }

    }

    if (((Brick->x >= bask1->x - 1.5)&&(Brick->x <=bask1->x + 0.8))
        &&(bask1->active == true) && (Brick->active == true))
    {
      if ( Brick->color == 1)
      {
        Brick->active = false;
        bask1->brickcount++;
        score += 20;
        cout << "score " << score << endl;
      }
      else if (Brick->color == 0)
      {
        gameover = true;
      }

    }

    //cout << "bask2 = " << bask2->brickcount <<"bask1 = "<< bask1->brickcount << endl;
  }

  return Brick;
}

bool checkcollisionwithwalls( VAO* Laser)
{
  float x = Laser->x + 1*cos(Laser->inclination*M_PI/180.0f);
  float y = Laser->y + 1*sin(Laser->inclination*M_PI/180.0f);

  if (( x > 10) || (x < -10) || (y > 10) || (y < -7.5))
  {
    return true;
  }
  else
  {
    return false;
  }
}

bool checkintersection (float x1 , float y1,float x2, float y2, float x3, float y3, float x4 , float y4)
{
  bool statement = ( (
                        ((y3-y1)*(x2-x1)-(y2-y1)*(x3-x1))*
                        ((y4-y1)*(x2-x1)-(y2-y1)*(x4-x1)) < 0
                     ) &&
                     (
                       ((y1-y3)*(x4-x3)-(y4-y3)*(x1-x3))*
                       ((y2-y3)*(x4-x3)-(y4-y3)*(x2-x3)) < 0
                     )
                   );
  if (statement)
  {
    return true;
  }
  else
  {
    return false;
  }
}

VAO* checkcollisionbtwlaserbrick(VAO* Laser)
{
  for (int j = 0; j < bricks.size(); j++)
  {
    if ((bricks[j]->active == true) &&
        (bricks[j]->x > Laser->x -1.7) && (bricks[j]->x < Laser->x + 1) &&
        (bricks[j]->y > Laser->y -1.7) && (bricks[j]->y < Laser->y + 1)
       )
    {
      bool one = checkintersection(Laser->x,Laser->y,
                                  Laser->x + 1*cos(Laser->inclination*M_PI/180.0f),
                                  Laser->y + 1*sin(Laser->inclination*M_PI/180.0f),
                                  bricks[j]->x,bricks[j]->y,bricks[j]->x ,bricks[j]->y + 0.7);
      bool two = checkintersection(Laser->x,Laser->y,
                                  Laser->x + 1*cos(Laser->inclination*M_PI/180.0f),
                                  Laser->y + 1*sin(Laser->inclination*M_PI/180.0f),
                                  bricks[j]->x,bricks[j]->y,bricks[j]->x + 0.7 ,bricks[j]->y);
      bool three = checkintersection(Laser->x,Laser->y,
                                  Laser->x + 1*cos(Laser->inclination*M_PI/180.0f),
                                  Laser->y + 1*sin(Laser->inclination*M_PI/180.0f),
                                  bricks[j]->x + 0.7,bricks[j]->y+0.7,bricks[j]->x ,bricks[j]->y + 0.7);
      bool four = checkintersection(Laser->x,Laser->y,
                                  Laser->x + 1*cos(Laser->inclination*M_PI/180.0f),
                                  Laser->y + 1*sin(Laser->inclination*M_PI/180.0f),
                                  bricks[j]->x+0.7,bricks[j]->y+0.7,bricks[j]->x + 0.7 ,bricks[j]->y);

      if (one || two || three || four)
      {
        Laser->active = false;
        bricks[j]->active = false;
        if (bricks[j]->color == 1)
        {
          redbrickshit++;
          score -= 5;
          cout << "score " << score << endl;
        }
        else if (bricks[j]->color == 2)
        {
          greenbrickshit++;
          score -= 5;
          cout << "score " << score << endl;
        }
        else if (bricks[j]->color == 0)
        {
          score += 50;
          cout << "score " << score << endl;
        }
        if (redbrickshit > 5 || greenbrickshit > 5)
        {
          gameover = true;
        }
        break;
      }


    }
  }

  return Laser;
}

VAO* checkcollisionwithmirrors(VAO* Laser)
{
  float x1 = Laser->x;
  float y1 = Laser->y;
  float x2 = Laser->x + 1*cos(Laser->inclination*M_PI/180.0f);
  float y2 = Laser->y + 1*sin(Laser->inclination*M_PI/180.0f);

  int j = 0;
  for (j = 0; j < mirrors.size(); j++)
  {
    float x = mirrors[j]->x;
    float y = mirrors[j]->y;

    float x3 = x + 1*cos(mirrors[j]->angle*M_PI/180.0f);
    float y3 = y + 1*sin(mirrors[j]->angle*M_PI/180.0f);

    float x4 = x - 1*cos(mirrors[j]->angle*M_PI/180.0f);
    float y4 = y - 1*sin(mirrors[j]->angle*M_PI/180.0f);

    bool statement = ( (
                          ((y3-y1)*(x2-x1)-(y2-y1)*(x3-x1))*
                          ((y4-y1)*(x2-x1)-(y2-y1)*(x4-x1)) < 0
                       ) &&
                       (
                         ((y1-y3)*(x4-x3)-(y4-y3)*(x1-x3))*
                         ((y2-y3)*(x4-x3)-(y4-y3)*(x2-x3)) < 0
                       )
                     );



    if (statement && Laser->reflection == false)
    {//cout << "one" <<endl;

      Laser->reflection = true;

      Laser->x = ((x1*y2-y1*x2)*(x3-x4)-(x1-x2)*(x3*y4-y3*x4))/((x1-x2)*(y3-y4)-(y1-y2)*(x3-x4));

      Laser->y = ((x1*y2-y1*x2)*(y3-y4)-(y1-y2)*(x3*y4-y3*x4))/((x1-x2)*(y3-y4)-(y1-y2)*(x3-x4));

      Laser->inclination = 2*mirrors[j]->angle - Laser->inclination;
      //playwav("reflection.wav");
      return Laser;
    }

  }

  return Laser;
}



/* Render the scene with openGL */
/* Edit this function according to your assignment */
void draw ()
{

  // use the loaded shader program
  // Don't change unless you know what you are doing
  glUseProgram (programID);

  // Eye - Location of camera. Don't change unless you are sure!!
  glm::vec3 eye ( 5*cos(camera_rotation_angle*M_PI/180.0f), 0, 5*sin(camera_rotation_angle*M_PI/180.0f) );
  // Target - Where is the camera looking at.  Don't change unless you are sure!!
  glm::vec3 target (0, 0, 0);
  // Up - Up vector defines tilt of camera.  Don't change unless you are sure!!
  glm::vec3 up (0, 1, 0);

  // Compute Camera matrix (view)
  // Matrices.view = glm::lookAt( eye, target, up ); // Rotating Camera for 3D
  //  Don't change unless you are sure!!
  Matrices.view = glm::lookAt(eye, target, up); // Fixed camera for 2D (ortho) in XY plane

  // Compute ViewProject matrix as view/camera might not be changed for this frame (basic scenario)
  //  Don't change unless you are sure!!
  glm::mat4 VP = Matrices.projection * Matrices.view;

  // Send our transformation to the currently bound shader, in the "MVP" uniform
  // For each model you render, since the MVP will be different (at least the M part)
  //  Don't change unless you are sure!!
  glm::mat4 MVP;	// MVP = Projection * View * Model

  // Load identity to model matrix

  if (!gameover)
  {
    if (mouse_right_drag)
    {
      if (xpos < mxpos)
      {
        if (  bnx-(mxpos-xpos)>= -10 )
        {
          bnx = bnx-(mxpos-xpos);
          bx = bx-(mxpos-xpos);
          Matrices.projection = glm::ortho(bnx, bx, bny, by, 0.1f, 500.0f);
        }

      }
      else if (xpos > mxpos)
      {
        if (bx + (xpos-mxpos) <= 10)
        {
          bnx = bnx + (xpos-mxpos);
          bx = bx + (xpos-mxpos);
          Matrices.projection = glm::ortho(bnx, bx, bny, by, 0.1f, 500.0f);
        }

      }
    }

    if ((sqrt((cannon_gun->x-xpos)*(cannon_gun->x-xpos)
        +(cannon_gun->y-ypos)*(cannon_gun->y-ypos)) < 1) && mouse_left_drag)
    {
        cannon_gun->drag = true;
        cannon_gun->y = ypos > 8 ? 8 : (ypos < -5.5 ? -5.5 : ypos);
        for (int j = 0 ; j < circle.size() ; j++)
        {
          circle[j]->drag = true;
          circle[j]->y = ypos > 8 ? 8 : (ypos < -5.5 ? -5.5 : ypos);
        }
        for (int j = 0 ; j < smcircle.size() ; j++)
        {
          smcircle[j]->drag = true;
          smcircle[j]->y = ypos > 8 ? 8 : (ypos < -5.5 ? -5.5 : ypos);
        }
    }

    if (bask1->active == false && bask2->active == false)
    {
      if (mouse_left_drag)
      {
        if (bask1->drag == true)
        {
          drag_basket = 1;
        }
        else if (bask2->drag == true)
        {
          drag_basket = 2;
        }
      }
      else
      {
        if (sqrt((bask1->x-xpos)*(bask1->x-xpos)+(bask1->y-ypos)*(bask1->y-ypos)) >
        sqrt((bask2->x-xpos)*(bask2->x-xpos)+(bask2->y-ypos)*(bask2->y-ypos)))
        {
          drag_basket = 2;
        }
        else
        {
          drag_basket = 1;
        }

      }

    }

    if ((mouse_left_drag)&&(xpos < bask1->x+1.5)&&(bask1->active || (!bask1->active && drag_basket == 1))&&
        (xpos > bask1->x-1.5)&&(ypos < bask1->y+0.5)&&(ypos > bask1->y - 0.5))
    {

        bask1->drag = true;
        bask1->x = xpos > 8.5 ? 8.5 : (xpos < -8.5 ? -8.5 : xpos);
        for ( int j = 0 ;j < baskcircle1.size() ; j++)
        {
          baskcircle1[j]->drag = true;
          baskcircle1[j]->x = xpos > 8.5 ? 8.5 : (xpos < -8.5 ? -8.5 : xpos);
        }
        for ( int j = 0 ;j < b1circle.size() ; j++)
        {
          b1circle[j]->drag = true;
          b1circle[j]->x = xpos > 8.5 ? 8.5 : (xpos < -8.5 ? -8.5 : xpos);
        }

    }

    if ((mouse_left_drag)&&(xpos < bask2->x+1.5)&&(bask2->active || (!bask2->active && drag_basket == 2))&&
        (xpos > bask2->x-1.5)&&(ypos < bask2->y+0.5)&&(ypos > bask2->y - 0.5))
    {

        bask2->drag = true;
        bask2->x = xpos > 8.5 ? 8.5 : (xpos < -8.5 ? -8.5 : xpos);
        for ( int j = 0 ;j < baskcircle2.size() ; j++)
        {
          baskcircle2[j]->drag = true;
          baskcircle2[j]->x = xpos > 8.5 ? 8.5 : (xpos < -8.5 ? -8.5 : xpos);
        }
        for ( int j = 0 ;j < b2circle.size() ; j++)
        {
          b2circle[j]->drag = true;
          b2circle[j]->x = xpos > 8.5 ? 8.5 : (xpos < -8.5 ? -8.5 : xpos);
        }
    }


    for ( int j = 0 ; j < circle.size()  ; j++)
    {
      Matrices.model = glm::mat4(1.0f);
      glm::mat4 translatesegment = glm::translate (glm::vec3(circle[j]->x, circle[j]->y, circle[j]->z)); // glTranslatef
      glm::mat4 rotatesegment = glm::rotate((float)(M_PI+j*10*M_PI/180.0f), glm::vec3(0,0,1));  // rotate about vector (1,0,0)
      Matrices.model *= (translatesegment*rotatesegment);
      MVP = VP * Matrices.model; // MVP = p * V * M
      glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
      draw3DObject(circle[j]);
    }

    for ( int j = 0 ; j < smcircle.size()  ; j++)
    {
      Matrices.model = glm::mat4(1.0f);
      glm::mat4 translatesegment = glm::translate (glm::vec3(smcircle[j]->x, smcircle[j]->y, smcircle[j]->z)); // glTranslatef
      glm::mat4 rotatesegment = glm::rotate((float)(M_PI+j*10*M_PI/180.0f), glm::vec3(0,0,1));  // rotate about vector (1,0,0)
      Matrices.model *= (translatesegment*rotatesegment);
      MVP = VP * Matrices.model; // MVP = p * V * M
      glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
      draw3DObject(smcircle[j]);
    }

    Matrices.model = glm::mat4(1.0f);
    glm::mat4 translatecannon_gun = glm::translate (glm::vec3( cannon_gun->x, cannon_gun->y, cannon_gun->z));        // glTranslatef
    glm::mat4 transformcannon_gun = glm::rotate((float)(cannon_gun_rotation*M_PI/180.0f), glm::vec3(0,0,1)); // rotate about vector (-1,1,1)
    Matrices.model *= (translatecannon_gun*transformcannon_gun);
    MVP = VP * Matrices.model;
    glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
    draw3DObject(cannon_gun);


    for (int j = 0 ; j < lasers.size();j++)
    {
      if (lasers[j]->active == true)
      {

        Matrices.model = glm::mat4(1.0f);
        glm::mat4 translatelaser = glm::translate (glm::vec3(lasers[j]->x, lasers[j]->y, lasers[j]->z)); // glTranslatef
        glm::mat4 rotatelaser = glm::rotate((float)(lasers[j]->inclination*M_PI/180.0f), glm::vec3(0,0,1));  // rotate about vector (1,0,0)
        Matrices.model *= translatelaser * rotatelaser;
        MVP = VP * Matrices.model; // MVP = p * V * M
        glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
        draw3DObject(lasers[j]);
        if (checkcollisionwithwalls(lasers[j]))
        {
          lasers[j]->active = false;
          lasers[j]->reflection = false;
        }

        lasers[j] = checkcollisionwithmirrors(lasers[j]);
        lasers[j] = checkcollisionbtwlaserbrick(lasers[j]);

      }

    }



    for (int j = 0 ; j < mirrors.size(); j++)
    {
      Matrices.model = glm::mat4(1.0f);
      glm::mat4 translatemirror = glm::translate (glm::vec3(mirrors[j]->x, mirrors[j]->y, mirrors[j]->z));        // glTranslatef
      glm::mat4 rotatemirror = glm::rotate((float)(mirrors[j]->angle*M_PI/180.0f), glm::vec3(0,0,1)); // rotate about vector (-1,1,1)
      Matrices.model *= (translatemirror*rotatemirror);
      MVP = VP * Matrices.model;
      glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
      draw3DObject(mirrors[j]);

    }





    for ( int j = 0 ; j < baskcircle1.size()  ; j++)
    {
      Matrices.model = glm::mat4(1.0f);
      glm::mat4 translatesegment = glm::translate (glm::vec3(baskcircle1[j]->x, baskcircle1[j]->y, baskcircle1[j]->z)); // glTranslatef
      glm::mat4 rotatesegment = glm::rotate((float)(M_PI+j*10*M_PI/180.0f), glm::vec3(0,0,1));
        glm::mat4 rotatesegment2 = glm::rotate((float)(80*M_PI/180.0f), glm::vec3(-1,0,0));  // rotate about vector (1,0,0)
      Matrices.model *= (translatesegment*rotatesegment2*rotatesegment);
      MVP = VP * Matrices.model; // MVP = p * V * M
      glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
      draw3DObject(baskcircle1[j]);
    }

    for ( int j = 0 ; j < baskcircle2.size()  ; j++)
    {
      Matrices.model = glm::mat4(1.0f);
      glm::mat4 translatesegment = glm::translate (glm::vec3(baskcircle2[j]->x, baskcircle2[j]->y, baskcircle2[j]->z)); // glTranslatef
      glm::mat4 rotatesegment = glm::rotate((float)(M_PI+j*10*M_PI/180.0f), glm::vec3(0,0,1));
        glm::mat4 rotatesegment2 = glm::rotate((float)(80*M_PI/180.0f), glm::vec3(-1,0,0));  // rotate about vector (1,0,0)
      Matrices.model *= (translatesegment*rotatesegment2*rotatesegment);
      MVP = VP * Matrices.model; // MVP = p * V * M
      glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
      draw3DObject(baskcircle2[j]);
    }



    for ( int j = 0 ; j < b1circle.size()  ; j++)
    {
      Matrices.model = glm::mat4(1.0f);
      glm::mat4 translatesegment = glm::translate (glm::vec3(b1circle[j]->x, b1circle[j]->y, b1circle[j]->z)); // glTranslatef
      glm::mat4 rotatesegment = glm::rotate((float)(M_PI+j*10*M_PI/180.0f), glm::vec3(0,0,1));
        glm::mat4 rotatesegment2 = glm::rotate((float)(80*M_PI/180.0f), glm::vec3(-1,0,0));  // rotate about vector (1,0,0)
      Matrices.model *= (translatesegment*rotatesegment2*rotatesegment);
      MVP = VP * Matrices.model; // MVP = p * V * M
      glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
      draw3DObject(b1circle[j]);
    }

    for ( int j = 0 ; j < b2circle.size()  ; j++)
    {
      Matrices.model = glm::mat4(1.0f);
      glm::mat4 translatesegment = glm::translate (glm::vec3(b2circle[j]->x, b2circle[j]->y, b2circle[j]->z)); // glTranslatef
      glm::mat4 rotatesegment = glm::rotate((float)(M_PI+j*10*M_PI/180.0f), glm::vec3(0,0,1));
      glm::mat4 rotatesegment2 = glm::rotate((float)(80*M_PI/180.0f), glm::vec3(-1,0,0));  // rotate about vector (1,0,0)
      Matrices.model *= (translatesegment*rotatesegment2*rotatesegment);
      MVP = VP * Matrices.model; // MVP = p * V * M
      glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
      draw3DObject(b2circle[j]);
    }

    Matrices.model = glm::mat4(1.0f);
    glm::mat4 translateline = glm::translate (glm::vec3(line->x, line->y, line->z));        // glTranslatef
    Matrices.model *= (translateline);
    MVP = VP * Matrices.model;
    glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
    draw3DObject(line);


    Matrices.model = glm::mat4(1.0f);
    glm::mat4 translatebask1 = glm::translate (glm::vec3(bask1->x, bask1->y, bask1->z));        // glTranslatef
    Matrices.model *= (translatebask1);
    MVP = VP * Matrices.model;
    glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
    draw3DObject(bask1);

    Matrices.model = glm::mat4(1.0f);
    glm::mat4 translatebask2 = glm::translate (glm::vec3(bask2->x, bask2->y, bask2->z));        // glTranslatef
    Matrices.model *= (translatebask2);
    MVP = VP * Matrices.model;
    glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
    draw3DObject(bask2);

    checkcollisionbtwbaskets();

    for (int j = 0; j < bricks.size(); j++)
    {
      if (bricks[j]->active == true )
      {
        Matrices.model = glm::mat4(1.0f);
        glm::mat4 translatebrick = glm::translate (glm::vec3(bricks[j]->x, bricks[j]->y, bricks[j]->z));        // glTranslatef
        Matrices.model *= (translatebrick);
        MVP = VP * Matrices.model;
        glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
        draw3DObject(bricks[j]);

        bricks[j] = checkcollisionbtwbrickbasket(bricks[j]);
      }

    }

  }

}

/* Initialise glfw window, I/O callbacks and the renderer to use */
/* Nothing to Edit here */
GLFWwindow* initGLFW (int width, int height)
{
    GLFWwindow* window; // window desciptor/handle

    glfwSetErrorCallback(error_callback);
    if (!glfwInit()) {
//        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(width, height, "Laser Shooting Game", NULL, NULL);

    if (!window) {
        glfwTerminate();
//        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
    glfwSwapInterval( 1 );

    /* --- register callbacks with GLFW --- */

    /* Register function to handle window resizes */
    /* With Retina display on Mac OS X GLFW's FramebufferSize
     is different from WindowSize */
    glfwSetFramebufferSizeCallback(window, reshapeWindow);
    glfwSetWindowSizeCallback(window, reshapeWindow);

    /* Register function to handle window close */
    glfwSetWindowCloseCallback(window, quit);

    /* Register function to handle keyboard input */
    glfwSetKeyCallback(window, keyboard);      // general keyboard input
    glfwSetCharCallback(window, keyboardChar);  // simpler specific character handling

    /* Register function to handle mouse click */
    glfwSetMouseButtonCallback(window, mouseButton);// mouse button clicks

    glfwSetCursorPosCallback(window, cursor_pos_callback);//get cursorposition

    glfwSetScrollCallback(window, scroll_callback);//scroll position



    return window;
}

/* Initialize the OpenGL rendering properties */
/* Add all the models to be created here */
void initGL (GLFWwindow* window, int width, int height)
{
    /* Objects should be created before any other gl function and shaders */
	// Create the models
	 // Generate the VAO, VBOs, vertices data & copy into the array buffer
	createbaskets ();
  createcannon();
  createbricks();
  createcircle();
  createbaskcircle();
  createsmallcircle();
  createcirclebottom2();
  createcirclebottom1();
  createlasers();
  createline();
  createmirrors();

	// Create and compile our GLSL program from the shaders
	programID = LoadShaders( "Sample_GL.vert", "Sample_GL.frag" );
	// Get a handle for our "MVP" uniform
	Matrices.MatrixID = glGetUniformLocation(programID, "MVP");


	reshapeWindow (window, width, height);

    // Background color of the scene
	glClearColor (1.0f, 1.0f, 0.9f, 0.0f); // R, G, B, A
	glClearDepth (1.0f);

	glEnable (GL_DEPTH_TEST);
	glDepthFunc (GL_LEQUAL);

    cout << "VENDOR: " << glGetString(GL_VENDOR) << endl;
    cout << "RENDERER: " << glGetString(GL_RENDERER) << endl;
    cout << "VERSION: " << glGetString(GL_VERSION) << endl;
    cout << "GLSL: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;
}

int main (int argc, char** argv)
{
	int width = 600;
	int height = 600;

    GLFWwindow* window = initGLFW(width, height);

	  initGL (window, width, height);

    double last_brick_update_time = glfwGetTime(),
           last_laser_update_time = glfwGetTime(),
           last_brick_creation_time = glfwGetTime(),
           current_time;




    /* Draw in loop */
    while (!glfwWindowShouldClose(window)) {

        // OpenGL Draw commands
        // clear the color and depth in the frame buffer
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        draw();
        //ao_play(device, buffer, BUF_SIZE);

        if (gameover)
        {

          cout << "Game Over final score " << score << endl;
          const char* audio = "gameover.wav";
          playaudio((void*) audio);
          quit(window);
          return 0;

        }

        // Swap Frame Buffer in double buffering
        glfwSwapBuffers(window);

        // Poll for Keyboard and mouse events
        glfwPollEvents();

        // Control based on time (Time based transformation like 5 degrees rotation every 0.5s)

        current_time = glfwGetTime(); // Time in seconds

        if (current_time > latest_cannonfire_time + 1)
        {
          cannon_active = true;
        }

        if ((current_time - last_laser_update_time) >= 0.01)
        {
          // atleast 0.5s elapsed since last frame
            // do something every 0.5 seconds ..
            for (int j = 0; j < lasers.size(); j++)
            {
              if (lasers[j]->active == true)
              {
                lasers[j]->x += 0.5*cos(lasers[j]->inclination*M_PI/180.0f);
                lasers[j]->y += 0.5*sin(lasers[j]->inclination*M_PI/180.0f);
                if (lasers[j]->reflection == true)
                {
                  lasers[j]->reflection = false;
                }
              }
            }

            last_laser_update_time = current_time;
        }

        if ((current_time - last_brick_update_time) >= brick_falling_frequency)
        {
          // atleast 0.5s elapsed since last frame
            // do something every 0.5 seconds ..
            for (int j =0 ; j<bricks.size() ; j++)
            {

              if (bricks[j]->active == true)
              {
                bricks[j]->y -= 0.25;
              }

            }

            last_brick_update_time = current_time;
        }

        if ((current_time - last_brick_creation_time) >= 5)
        {
          createbricks();
          last_brick_creation_time = current_time;
        }


    }


    glfwTerminate();
//    exit(EXIT_SUCCESS);
}
