#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include <cglm/cglm.h>
#include <cglm/struct.h>

#include <log.h>

#include "MacroDatabase.h"

#if defined(WINDOWS) || defined(_WIN32) || defined(_WIN64)
  #define APP_WINDOWS true
  #define CLEAR_CONSOLE_COMMAND "cls"
#else
    // Assume POSIX
    #define APP_POSIX true
    #define CLEAR_CONSOLE_COMMAND "clear"
#endif

#ifdef _DEBUG
  #define APP_DEBUG true
#endif

#define CLEAR_CONSOLE(...) system(CLEAR_CONSOLE_COMMAND)
#define ENDL "\n"

typedef struct {
  MacroDatabase_t *_db;

  char *_inputBuffer;
  uint32_t _ibCount;
  uint32_t _ibCapacity;

  _Bool _running;

  char *_chatHistory;
  uint32_t _chCount;
  uint32_t _chCapacity;

  // void **element;
  // uint32_t _elementCount;
  // uint32_t _elementCapacity;
} ConsoleApp_t;

void ConsoleApp_clearInputBuffer(ConsoleApp_t *self) {
  if (self->_ibCount > 0) {
    memset(self->_inputBuffer, 0, self->_ibCount);
  }

  self->_ibCount = 1;
  self->_inputBuffer[0] = '\0';
}

ConsoleApp_t *ConsoleApp_create() {
  ConsoleApp_t *self = malloc(sizeof(ConsoleApp_t));
  *self = (ConsoleApp_t) {
    ._db = MacroDatabase_create(),

    ._ibCapacity = DEFAULT_BUF_CAP,
    ._ibCount = 0,
    ._inputBuffer = malloc(DEFAULT_BUF_CAP),

    ._chCapacity = DEFAULT_BUF_CAP,
    ._chCount = 0,
    ._chatHistory = malloc(DEFAULT_BUF_CAP),

    ._running = true
  };

  ConsoleApp_clearInputBuffer(self);
  memset(self->_chatHistory, '\0', self->_chCapacity);

  return self;
}

void ConsoleApp_chatHistoryResize(ConsoleApp_t *self, uint32_t newLength) {
  self->_chCount = newLength;

  uint32_t oldCapacity = self->_chCapacity;
  while (self->_chCapacity < self->_chCapacity) {
    self->_chCapacity <<= 1;
  }

  self->_chatHistory = realloc(self->_chatHistory, self->_chCapacity);
  memset(&self->_chatHistory[oldCapacity - 1], '\0', self->_chCapacity - oldCapacity);
}

void ConsoleApp_chatHistoryPush(ConsoleApp_t *self, const char *msg, uint32_t msgLength) {
  uint32_t oldLength = self->_chCount;

  ConsoleApp_chatHistoryResize(self, self->_chCount + msgLength);

  _memccpy(&self->_chatHistory[oldLength], msg, '\0', msgLength - 1);

  self->_chatHistory[self->_chCount - 1] = '\n';
  self->_chatHistory[self->_chCount] = '\0';
}

void ConsoleApp_processCommand(ConsoleApp_t *self) {
  if (strcmp(self->_inputBuffer, "exit") == 0) {
    self->_running = false;
  }

  ConsoleApp_clearInputBuffer(self);
}

void ConsoleApp_pushCharToInputBuffer(ConsoleApp_t *self, char newChar) {
  self->_inputBuffer[self->_ibCount - 1] = newChar;
  self->_ibCount++;

  while (self->_ibCapacity < self->_ibCount * sizeof(char)) {
    self->_ibCount <<= 1;
  }

  self->_inputBuffer = realloc(self->_inputBuffer, self->_ibCapacity);
  self->_inputBuffer[self->_ibCount - 1] = '\0';
}

void ConsoleApp_render(ConsoleApp_t *self) {
  CLEAR_CONSOLE();

  printf("%s", self->_chatHistory);
  printf("Command: %s", self->_inputBuffer);
  MacroDatabase_logToConsole(self->_db);
}

void ConsoleApp_run(ConsoleApp_t *self) {
  char inputChar = '\0';

  ConsoleApp_render(self);

  while (self->_running && (inputChar = getc(stdin))) {
    if (inputChar == '\n') {
      ConsoleApp_chatHistoryPush(self, self->_inputBuffer, self->_ibCount);
      ConsoleApp_processCommand(self);
    } else {
      ConsoleApp_pushCharToInputBuffer(self, inputChar);
    }

    ConsoleApp_render(self);
  }
}

void ConsoleApp_destroy(ConsoleApp_t *self) {
  MacroDatabase_destroy(self->_db);

  free(self->_inputBuffer);
  free(self);
}

typedef struct __GlobalUBData_t {
  mat4 projectionView;
} _GlobalUBData_t;

typedef struct _App_t {
  GLFWwindow *_wnd;
  _Bool _running;

  double _lastTime;
  double _deltaTime;

  GLuint _shader;
  GLuint _quadVAO, _quadVBO, _quadEBO;

  vec2 camera;
  vec2 _cameraTarget;

  GLuint _globalUB;
  _GlobalUBData_t _globalUBData;

  vec2 _mouseStart;
} App_t;

#define DEFAULT_WINDOW_WIDTH 1024
#define DEFAULT_WINDOW_HEIGHT 512

#include "Common.h"

#define VISIBLE_HEIGHT_SIZE 4.f
#define BASE_SPEED 10.f

void _App_windowCloseCallback(GLFWwindow* window) {
  App_t *app = glfwGetWindowUserPointer(window);

  glfwSetWindowShouldClose(window, GLFW_TRUE);
  app->_running = false;
}

void  _App_calculateMouseTransform(App_t *app) {
  double mx, my;
  glfwGetCursorPos(app->_wnd, &mx, &my);

  int fbfW = 0, fbfH = 0;
  glfwGetFramebufferSize(app->_wnd, &fbfW, &fbfH);

  float aspect = fbfW / (float)fbfH;

  mat4 projection = GLM_MAT4_IDENTITY_INIT;

  float frameWidth = VISIBLE_HEIGHT_SIZE;
  float frameHeight = frameWidth / aspect;

  float left = -frameWidth / 2.0f;  // e.g., -5.0
  float right = frameWidth / 2.0f;  // e.g., 5.0
  float bottom = -frameHeight / 2.0f; // e.g., -3.75 (for aspect = 4/3)
  float top = frameHeight / 2.0f;    // e.g., 3.75

  glm_ortho(
    left, right,
    bottom, top, 
    -1.f, 1.f, 
  projection
  );

  vec3 start = {app->_mouseStart[0], app->_mouseStart[1], 0};
  vec3 end = {(float)mx, (float)my, 0};

  vec3 startProd;
  vec3 endProd;


  glm_mat4_mulv3(projection, start, 1.0, startProd);
  glm_mat4_mulv3(projection, end, 1.0, endProd);

  vec3 delta;
  glm_vec3_sub(start, end, delta);
  glm_vec3_scale(delta, 0.001f, delta);

  glm_vec2_sub(app->camera, (vec2) {delta[0], -delta[1]}, app->_cameraTarget);
}

void _App_mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
  if (button != GLFW_MOUSE_BUTTON_1) {
    return;
  }

  App_t *app = glfwGetWindowUserPointer(window);
  if (action == GLFW_PRESS) {
    double my, mx;
    glfwGetCursorPos(window, &mx, &my);
    app->_mouseStart[0] = (float)mx;
    app->_mouseStart[1] = (float)my;
  } else if (action == GLFW_RELEASE) {
    _App_calculateMouseTransform(app);
  }
}

void _App_keyInputCallback(GLFWwindow* window,
    int key, int scancode, int action, int mods) {
  App_t *app = glfwGetWindowUserPointer(window);

  if (action != GLFW_PRESS)
    return;
  
  if (key != GLFW_KEY_ESCAPE)
    return;

  glfwSetWindowShouldClose(window, GLFW_TRUE);
  app->_running = false;
}

#define VERT_FILE_NAME "vert.glsl"
#define FRAG_FILE_NAME "frag.glsl"

#ifdef APP_DEBUG
void APIENTRY _App_OpenGL_debugMsgCallback(GLenum source, GLenum type, GLuint id,
  GLenum severity, GLsizei length, const GLchar *message, 
  const void *userParam) {
  // App_t *app = (App_t *)userParam;

  const char *sourceName = "API";
  switch (source) {
    case GL_DEBUG_SOURCE_OTHER:
      sourceName = "OTHER";
      break;
    case GL_DEBUG_SOURCE_APPLICATION:
      sourceName = "APP";
      break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:
      sourceName = "THIRD PARTY";
      break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
      sourceName = "SHADER";
      break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
      sourceName = "WINDOW";
      break;
    case GL_DEBUG_SOURCE_API:
    default:
      break;
  }

  const char *typeName = "Unknown";
  switch (type) {
  case GL_DEBUG_TYPE_ERROR:
    typeName = "Error";
    break;
  case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
    typeName = "Deprecated";
    break;
  case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
    typeName = "Undefined";
    break;
  case GL_DEBUG_TYPE_PORTABILITY:
    typeName = "Portability";
    break;
  case GL_DEBUG_TYPE_PERFORMANCE:
    typeName = "Performance";
    break;
  case GL_DEBUG_TYPE_MARKER:
    typeName = "Marker";
    break;
  case GL_DEBUG_TYPE_PUSH_GROUP:
    typeName = "Push Group";
    break;
  case GL_DEBUG_TYPE_POP_GROUP:
    typeName = "Pop Group";
    break;
  case GL_DEBUG_TYPE_OTHER:
  default:
    typeName = "Other";
    break;
  }

  switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
      log_error("[OpenGL (%s) -> %s (id: %i)] %s" ENDL,
        typeName, sourceName, id, message
      );
      break;
    case GL_DEBUG_SEVERITY_MEDIUM:
      log_warn("[OpenGL (%s) -> %s (id: %i)] %s" ENDL,
        typeName, sourceName, id, message
      );
      break;
    case GL_DEBUG_SEVERITY_LOW:
    case GL_DEBUG_SEVERITY_NOTIFICATION:
      log_info("[OpenGL (%s) -> %s (id: %i)] %s" ENDL,
        typeName, sourceName, id, message
      );
    default:
      break;
  }

}
#endif

GLint __intToNextPow2(GLint val) {
  GLint out = 1;
  
  while (out < val) {
    out <<= 1;
  }

  return out;
}

GLint __intPow2ScaleToNext(GLint prev, GLint val) {
  while (prev < val) {
    prev <<= 1;
  }

  return prev;
}

Result_t _OpenGL_validateAndLogError(GLuint shader, const char *shaderFileName,
  GLchar **p_charBuff, GLint *p_buffSize) {
  GLint shaderStatus = 0;

  glGetShaderiv(shader, GL_COMPILE_STATUS, &shaderStatus);
  if (shaderStatus != GL_TRUE) {
    GLint requiredLogLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &requiredLogLength);

    GLint greaterSize = requiredLogLength > *p_buffSize ? 
      requiredLogLength : *p_buffSize;

    *p_buffSize = __intPow2ScaleToNext(*p_buffSize, greaterSize);
    
    *p_charBuff = realloc(*p_charBuff, *p_buffSize);

    glGetShaderInfoLog(shader, *p_buffSize, NULL, *p_charBuff);

    log_error("(%u : %s) compilation error: \"%s\"" ENDL,
      shader,
      shaderFileName,
      *p_charBuff
    );

    return RESULT_FAIL;
  }

  return RESULT_SUCCESS;
}

#ifdef APP_DEBUG
#define OPENGL_VALIDATE_AND_LOG_ERROR(sha, fname, p_cbuf, p_bufSize) _OpenGL_validateAndLogError(sha, fname, p_cbuf, p_bufSize)
#else
#define OPENGL_VALIDATE_AND_LOG_ERROR(sha, fname, p_cbuf, p_bufSize)
#endif

void _App_OpenGlCleanup(App_t *app) {
  glDeleteProgram(app->_shader);

  glDeleteBuffers(1, &app->_quadEBO);
  glDeleteBuffers(1, &app->_quadVBO);

  glDeleteVertexArrays(1, &app->_quadVAO);

  glDeleteBuffers(1, &app->_globalUB);
}

Result_t _App_setupGl(App_t *app) {
#ifdef APP_DEBUG
  glEnable(GL_DEBUG_OUTPUT);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  glDebugMessageCallback(_App_OpenGL_debugMsgCallback, NULL);
#endif

  float vertices[] = {
      -0.5f, -0.5f, 0.0f,  // bottom left
        0.5f, -0.5f, 0.0f,  // bottom right
        0.5f,  0.5f, 0.0f,  // top right
      -0.5f,  0.5f, 0.0f   // top left
  };
  
  unsigned int indices[] = {
      0, 1, 2,   // first triangle
      2, 3, 0    // second triangle
  };

  glGenVertexArrays(1, &app->_quadVAO);
  glGenBuffers(1, &app->_quadVBO);
  glGenBuffers(1, &app->_quadEBO);
  glBindVertexArray(app->_quadVAO);

  glBindBuffer(GL_ARRAY_BUFFER, app->_quadVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->_quadEBO);
  glNamedBufferData(app->_quadEBO, sizeof(indices), indices, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
    (void*)0
  );
  glEnableVertexAttribArray(0);

  FILE *vertFile, *fragFile;
  GLint vertFileSize = 0, fragFileSize = 0;

  errno_t err = 0;
  if ((err = fopen_s(&vertFile, VERT_FILE_NAME, "r")) != 0 ||
    vertFile == NULL) {
    log_error(
      "Couldn't create/open the vertex shader file %s, error code: %i" ENDL,
      VERT_FILE_NAME,
      err
    );

    fclose(vertFile);
    return RESULT_FAIL;
  } else {
    log_info("Opened vertex shader file: %s" ENDL, VERT_FILE_NAME);
  }
  
  if ((err = fopen_s(&fragFile, FRAG_FILE_NAME, "r")) != 0 || 
    fragFile == NULL) {
    log_error(
      "Couldn't create/open the fragment shader file %s, error code: %i" ENDL,
      FRAG_FILE_NAME,
      err
    );

    fclose(vertFile);
    fclose(fragFile);
    return RESULT_FAIL;
  } else {
    log_info("Opened fragment shader file: %s" ENDL, FRAG_FILE_NAME);
  }

  char *charBuff = NULL;

  fseek(vertFile, 0L, SEEK_END);
  vertFileSize = ftell(vertFile) + 1;

  fseek(fragFile, 0L, SEEK_END);
  fragFileSize = ftell(fragFile) + 1;

  GLint charCount = fragFileSize > vertFileSize ?
    fragFileSize : vertFileSize;

  charCount = __intToNextPow2(charCount);
  charBuff = malloc(charCount);
  memset(charBuff, 0, charCount);

  rewind(vertFile);
  
  GLint readFileSize = (GLint)fread(charBuff,
    sizeof(char), 
    vertFileSize - 1, 
    vertFile
  );
  charBuff[vertFileSize - 1] = '\0';

  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &charBuff, &readFileSize);
  glCompileShader(vertexShader);
  fclose(vertFile);

#ifdef APP_DEBUG
  Result_t vertResult = OPENGL_VALIDATE_AND_LOG_ERROR(vertexShader, 
    VERT_FILE_NAME, &charBuff, &charCount
  );

  if (!vertResult) {
    log_error("Failed to compile vertex shader (%u : %s)" ENDL,
      vertexShader,
      VERT_FILE_NAME
    );
    return RESULT_FAIL;
  } else {
    log_info("Successfully loaded vertex shader (%u : %s)" ENDL,
      vertexShader,
      VERT_FILE_NAME
    );
  }
#endif

  rewind(fragFile);
  memset(charBuff, 0, charCount);
  readFileSize = (GLint)fread(charBuff,
    sizeof(char),
    fragFileSize - 1, 
    fragFile
  );
  charBuff[fragFileSize - 1] = '\0';

  GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &charBuff, &readFileSize);
  glCompileShader(fragmentShader);
  fclose(fragFile);

#ifdef APP_DEBUG
  Result_t fragResult = OPENGL_VALIDATE_AND_LOG_ERROR(fragmentShader,
    FRAG_FILE_NAME, &charBuff, &charCount
  );

  if (!fragResult) {
    log_error("Failed to compile fragment shader (%u : %s)" ENDL,
      fragmentShader, FRAG_FILE_NAME
    );
    return RESULT_FAIL;
  } else {
    log_info("Successfully loaded fragment shader (%u : %s)" ENDL,
      fragmentShader, FRAG_FILE_NAME
    );
  }
#endif

  app->_shader = glCreateProgram();
  glAttachShader(app->_shader, vertexShader);
  glAttachShader(app->_shader, fragmentShader);
  glLinkProgram(app->_shader);

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

#ifdef APP_DEBUG
  GLint linkStatus = GL_FALSE;
  glGetProgramiv(app->_shader, GL_LINK_STATUS, &linkStatus);
  if (linkStatus != GL_TRUE) {
    GLint requiredLogLength = 0;
    glGetProgramiv(app->_shader, GL_INFO_LOG_LENGTH,
      &requiredLogLength
    );

    GLint greaterSize = requiredLogLength > charCount ? 
      requiredLogLength : charCount;

    charCount = __intPow2ScaleToNext(charCount, greaterSize);
    
    charBuff = realloc(charBuff, charCount);

    glGetShaderInfoLog(app->_shader, charCount, NULL,
      charBuff);

    log_error("(%u - %s : %s) link error: \"%s\"" ENDL,
      app->_shader,
      VERT_FILE_NAME,
      FRAG_FILE_NAME,
      charBuff
    );
  } else {
    log_info("Successfully linked shader program (%u - %s : %s)" ENDL,
      app->_shader,
      VERT_FILE_NAME,
      FRAG_FILE_NAME
    );
  }
#endif
  
  glCreateBuffers(1, &app->_globalUB);
  glNamedBufferData(app->_globalUB, sizeof(_GlobalUBData_t),
    &app->_globalUBData, GL_STATIC_DRAW
  );

  return RESULT_SUCCESS;
}

Result_t App_create(App_t** p_app) {
  *p_app = malloc(sizeof(App_t));

  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

#ifdef APP_DEBUG
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

  GLFWwindow *window;
  if (!(window = glfwCreateWindow(1024, 512, "MacroApp", NULL, NULL))) {
    return RESULT_FAIL;
  }

  **p_app = (App_t) {
    ._wnd = window,
    ._running = true,

    .camera = {0, 0},
    ._cameraTarget = {0, 0},

    ._shader = 0,

    ._quadEBO = 0,
    ._quadVAO = 0,
    ._quadVBO = 0
  };

  glm_mat4_identity((*p_app)->_globalUBData.projectionView);

  glfwSetWindowUserPointer(window, *p_app);
  glfwSetWindowCloseCallback(window, _App_windowCloseCallback);
  glfwSetKeyCallback(window, _App_keyInputCallback);
  glfwSetMouseButtonCallback(window, _App_mouseButtonCallback);

  glfwMakeContextCurrent(window);
  int glVersion = 0;
  if (glVersion = gladLoadGL(glfwGetProcAddress)) {
    log_info("Loaded OpenGL %d.%d" ENDL, GLAD_VERSION_MAJOR(glVersion),
      GLAD_VERSION_MINOR(glVersion)
    );
  } else {
    log_error("Failed to initialize OpenGL context" ENDL);
    return RESULT_FAIL;
  }

  if (_App_setupGl(*p_app) != RESULT_SUCCESS) {
    log_error("Failed to setup OpenGL resources" ENDL);
    return RESULT_FAIL;
  }

  return RESULT_SUCCESS;
}

void _App_render(App_t *app) {
  glfwMakeContextCurrent(app->_wnd);
  glClear(GL_COLOR_BUFFER_BIT);

  glUseProgram(app->_shader);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, app->_globalUB); 
  glBindVertexArray(app->_quadVAO);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

  glfwSwapBuffers(app->_wnd);
}

void _App_update(App_t *app)
{
  // MOVE CAMERA AND COMPONENTS

  vec2 offset = {0, 0};
  _Bool hasMoved = false;

  if (glfwGetKey(app->_wnd, GLFW_KEY_W) == GLFW_PRESS) {
    hasMoved = true;
    offset[1] -= 1;
  }

  if (glfwGetKey(app->_wnd, GLFW_KEY_S) == GLFW_PRESS) {
    hasMoved = true;
    offset[1] += 1;
  }

  if (glfwGetKey(app->_wnd, GLFW_KEY_A) == GLFW_PRESS) {
    hasMoved = true;
    offset[0] += 1;
  }
  
  if (glfwGetKey(app->_wnd, GLFW_KEY_D) == GLFW_PRESS) {
    hasMoved = true;
    offset[0] -= 1;
  }

  if (hasMoved) {
    float offsetMag = sqrtf(offset[0] * offset[0] + offset[1] * offset[1]);
    float offsetMagNorm = 1.f / offsetMag;
    float transformMultiplier = 
      (float)app->_deltaTime * BASE_SPEED * offsetMagNorm;

    offset[0] *= transformMultiplier;
    offset[1] *= transformMultiplier;


    glm_vec2_add(app->camera, offset, app->_cameraTarget);
  }

  
  int fbfW = 0, fbfH = 0;
  glfwGetFramebufferSize(app->_wnd, &fbfW, &fbfH);

  float aspect = fbfW / (float)fbfH;

  mat4 projection = GLM_MAT4_IDENTITY_INIT;
  mat4 view = GLM_MAT4_IDENTITY_INIT;

  float frameWidth = VISIBLE_HEIGHT_SIZE;
  float frameHeight = frameWidth / aspect;

  float left = -frameWidth / 2.0f;  // e.g., -5.0
  float right = frameWidth / 2.0f;  // e.g., 5.0
  float bottom = -frameHeight / 2.0f; // e.g., -3.75 (for aspect = 4/3)
  float top = frameHeight / 2.0f;    // e.g., 3.75

  glm_ortho(
    left, right,
    bottom, top, 
    -1.f, 1.f, 
  projection
  );

  glm_vec2_lerp(app->camera, app->_cameraTarget, app->_deltaTime, app->camera);
  
  glm_translate(view, (vec3) { app->camera[0], app->camera[1], 0.0 });

  glm_mat4_mul(projection, view, app->_globalUBData.projectionView);

  // GL CODE -> marked to find if this gets moved into its own function
  // as the number of buffers that are written to grows
  // maybe a function called "Update GL Buffers"
  glNamedBufferSubData(app->_globalUB, 0, 
    sizeof(_GlobalUBData_t), &app->_globalUBData
  );

}

void App_destroy(App_t *app) {
  _App_OpenGlCleanup(app);
  glfwDestroyWindow(app->_wnd);
  free(app);
}

#define FPS_LIMIT 144
#define FRAME_TIME (1.0 / FPS_LIMIT)

void App_run(App_t *app) {
  app->_lastTime = glfwGetTime();

  while (app->_running) {
    glfwPollEvents();
    double newTime = glfwGetTime();

    app->_deltaTime = newTime - app->_lastTime;
    if (app->_deltaTime < FRAME_TIME) {
      continue;
    }

    _App_update(app);
    _App_render(app);

    app->_lastTime = newTime;
  }
}

#define LOG_FILE_NAME "macro_runtime_log.txt"

int main(void) {
  FILE *logFile = NULL;

  errno_t err = 0;
  if ((err = fopen_s(&logFile, LOG_FILE_NAME, "w")) != 0 || logFile == NULL) {
    log_error(
      "Couldn't create/open the log file %s, error code: %i" ENDL,
      LOG_FILE_NAME,
      err
    );
  } else {
    log_info("Opened log file: %s" ENDL, LOG_FILE_NAME);
  }
  
  log_add_fp(logFile, 0);

  if (glfwInit() != GLFW_TRUE) {
    log_error("Failed to init GLFW");
  }

  App_t *app = NULL;
  if (App_create(&app) != RESULT_SUCCESS) {
    log_error("Failed to create window");

    glfwTerminate();
    return -1;
  }

  App_run(app);
  App_destroy(app);

  glfwTerminate();

  log_info("Program ended successfully" ENDL);
  fclose(logFile);
  return 0;
}