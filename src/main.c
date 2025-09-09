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

#include <ft2build.h>
#include FT_FREETYPE_H

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <log.h>

#include "MacroDatabase.h"
#include "Common.h"

// UNUSED
typedef struct __StrView_t {
  size_t count;
  UC_t *str;
} StrView_t;

// UNUSED
typedef struct __Str_t {
  size_t count;
  size_t cap;
  UC_t *str;
} Str_t;

#define STR_TO_VIEW(str) ((StrView_t) { .count = str.count, .str = str.str })

Str_t *Str_create() {
  return NULL;
}

size_t __Str_getCharSize(const char *src) {
  return -1;
}

// Literal assumed to be utf8
Str_t *Str_createFromLiteral(const char *src) {
  Str_t *self = malloc(sizeof(Str_t));
  *self = (Str_t) {
    .count = 0,
    .str = NULL,
    .cap = 1,
  };
  
  self->count = strlen(src);

  DEBUG_ASSERT(self->count > 0, "Invalid string supplied");

  while (self->cap < self->count) {
    self->cap <<= 1;
  }

  // self->str = malloc(self->)
  return NULL;
}

typedef struct __GlobalUBData_t {
  mat4 projectionView;
} _GlobalUBData_t;

typedef struct __Transform_t {
  vec2 position;
  vec2 scale;
  float rotation;
} Transform_t;

#define TRANSFORM_INIT (Transform_t) {   \
    .position = { 0.f, 0.f },            \
    .scale = {1.f, 1.f},                 \
    .rotation = 0.f                      \
  }

void Transform_toMat4(Transform_t *self, mat4 matrix) {
  glm_mat4_identity(matrix);

  glm_rotate(
    matrix, 
    (self->rotation / 180.f)* GLM_PI, 
    (vec3){0, 0, 1}
  );

  glm_translate(matrix, self->position);

  glm_scale(matrix, self->scale);

}

void Transform_copy(Transform_t *self, Transform_t other) {
  memcpy(self->position, other.position, sizeof(vec2));
  memcpy(self->scale, other.scale, sizeof(vec2));
  memcpy(&self->rotation, &other.rotation, sizeof(float));
}

typedef struct __LocalUBData2D_t {
  mat4 model;
  vec4 color;
} _LocalUBData2D_t;

typedef uint32_t u32vec2[2];

typedef struct __Glyph_t {
  UC_t character;
  vec2 size;
  vec2 bearing;

  _Bool isWhitespace;

  GLuint glTextureHandle;
  u32vec2 advance;
} _Glyph_t;

typedef union _SizeVec2_t {
  struct {
    size_t x, y;
  };

  struct {
    size_t width, height;
  };

  size_t ptr[2];
} SizeVec2_t;

typedef struct __AppInfo_t {
  const char *name;
  SizeVec2_t size;
} AppInfo_t;

#define DEFAULT_APP_NAME "macro"
#define DEFAULT_WINDOW_WIDTH 1024
#define DEFAULT_WINDOW_HEIGHT 1024

#define APP_INFO_INIT (AppInfo_t) {                          \
    .name = DEFAULT_APP_NAME,                                \
    .size = { DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT }, \
  }

typedef enum __UiElType_t {
  UI_EL_TYPE_CONTAINER = 0,
  UI_EL_TYPE_TEXT = 1,
  UI_EL_TYPE_BUTTON = 2,
  UI_EL_TYPE_INPUT = 3
} UiElType_t;

#define DEFAULT_CHILD_CAP ((size_t)1 << 7)

// TODO: implement atlas

typedef enum __UI_SIZE_FLAG_T {
  UI_SIZE_FLAG_REAL = 0,
  UI_SIZE_FLAG_FILL_WIDTH = 1, // NOT SUPPORTED
  UI_SIZE_FLAG_FILL_HEIGHT = 2 // NOT SUPPORTED
} UI_SIZE_FLAG_T;

typedef struct __UiSize_t {
  UI_SIZE_FLAG_T flag;

  union {
    vec2 dimentions;
    struct {
      float width, height;
    };
  };
} UiSize_t;

typedef struct __UI_t {
  struct __UI_t *children, *parent;
  size_t childCount, childCap;

  UiElType_t _type;
  void *_unique;

  vec2 _size;
  vec2 _pos, _globalPos;

  vec4 color;
  vec4 _color;
  mat4 _matrix;
} UI_t;

typedef struct __UiInfo_t {
  UiElType_t type;

  vec2 position, size;
  vec4 color;

  UI_t *parent;
} UiInfo_t;

void __UI_calculateMatrix(UI_t *self) {
  glm_mat4_identity(self->_matrix);
  
  glm_translate(self->_matrix, (vec3){
      self->_globalPos[0],
      self->_globalPos[1],
      0.f
    }
  );

  glm_scale(self->_matrix, (vec3) { 
      self->_size[0], 
      self->_size[1], 
      1.0f
    }
  );
}

void __UI_updateMatrix(UI_t *self) {
  // Update globals to locals
  memcpy(self->_globalPos, self->_pos, sizeof(self->_globalPos));

  if (self->parent != NULL) {
    glm_vec2_add(self->parent->_globalPos, self->_globalPos, self->_globalPos);
  }

  __UI_calculateMatrix(self);
  for (UI_t *child = self->children;
    child < &self->children[self->childCount]; child++) {
      __UI_updateMatrix(child);
  }
}

void UI_setPosition(UI_t *self, vec2 newPosition) {
  memcpy(self->_pos, newPosition, sizeof(vec2));
  __UI_updateMatrix(self);
}

void UI_setSize(UI_t *self, vec2 newSize) {
  DEBUG_ASSERT(
    newSize[0] < 1.01f && newSize[1] < 1.01f &&
    newSize[0] > 0.f && newSize[1] > 0.f,
    "Size cannot exceed 1.f or be less than 0.f"
  );

  memcpy(self->_size, newSize, sizeof(vec2));
  __UI_updateMatrix(self);
}

Result_t UI_init(UI_t *self, UiInfo_t *info) {
  self->childCap = DEFAULT_CHILD_CAP;
  self->children = malloc(self->childCap);
  self->childCount = 0;

  self->_type = info->type;
  self->parent = info->parent;

  memcpy(self->_pos, info->position, sizeof(vec2));
  memcpy(self->_size, info->size, sizeof(vec2));
  memcpy(self->color, info->color, sizeof(vec4));
  memcpy(self->_color, info->color, sizeof(vec4));

  __UI_updateMatrix(self);
  return EXIT_SUCCESS;
}

UI_t *UI_addChild(UI_t *self, UiInfo_t *info) {
  self->childCount++;
  while (self->childCount * sizeof(UI_t) > self->childCap) {
    self->childCap <<= 1;
  }
  self->children = realloc(self->children, self->childCap);

  UI_t *child = &self->children[self->childCount - 1];
  info->parent = self;
  UI_init(child, info);

  return child;
}

void UI_destroy(UI_t *self) {
  for (UI_t *child = self->children;
    child < &self->children[self->childCount]; child++) {
      UI_destroy(child);
  }

  switch (self->_type) {
    case UI_EL_TYPE_BUTTON:
    case UI_EL_TYPE_CONTAINER:
      free(self->_unique);
      self->_unique = NULL;
      break;
    default:
      break;
  }

  free(self->children);
}

typedef enum __ContainerFlag_t {
  CONTAINER_FLAG_NONE = 0,
  CONTAINER_FLAG_ORDER_VERTICAL = 1,
  CONTAINER_FLAG_ORDER_HORIZONTAL = 2
} ContainerFlag_t;

typedef struct __UiContainer_t {
  ContainerFlag_t flags;
  // TODO: float _offsetAccumulation;
} UiContainer_t;

typedef struct __UiContainerInfo_t {
  ContainerFlag_t flags;
  vec4 color;

  vec2 position, size;
} UiContainerInfo_t;

void __UI_initContainer(UI_t *self, UiContainerInfo_t *specInfo) {
  UiContainer_t *unique = (self->_unique = malloc(sizeof(UiContainer_t)));
  *unique = (UiContainer_t) {
    .flags = specInfo->flags
  };
}

UI_t *UI_addChildContainer(UI_t *self, UiContainerInfo_t *specInfo) {
  UiInfo_t genInfo = {
    .type = UI_EL_TYPE_CONTAINER,
    .parent = NULL,
  };

  DEBUG_ASSERT(
    sizeof(genInfo.color) == sizeof(specInfo->color), 
    "genInfo.color and specInfo->color must be of same type"
  );
  memcpy(genInfo.color, specInfo->color, sizeof(genInfo.color));

  DEBUG_ASSERT(
    sizeof(genInfo.position) == sizeof(specInfo->position), 
    "genInfo.position and specInfo->position must be of same type"
  );
  memcpy(genInfo.position, specInfo->position, sizeof(genInfo.position));

  DEBUG_ASSERT(
    sizeof(genInfo.size) == sizeof(specInfo->size), 
    "genInfo.size and specInfo->size must be of same type"
  );
  memcpy(genInfo.size, specInfo->size, sizeof(genInfo.size));

  UI_t *child = UI_addChild(self, &genInfo);
  __UI_initContainer(child, specInfo);

  return child;
}

typedef struct __App_t {
  struct __Draw_t {
    FT_Library _ft;
    FT_Face _ftFace;

    // TODO: implement flatShader
    GLuint _texShader, _flatShader;
    GLuint _quadVAO, _quadVBO, _quadEBO;

    mat4 _projection, _view;

    GLuint _globalUB, _localUB;
    _GlobalUBData_t _globalUBData;

    // Currently linear search
    _Glyph_t *_glyphs;
    size_t _glyphCap, _glyphCount;
  } _draw;

  GLFWwindow *_wnd;
  _Bool _running;

  double _lastTime;
  double _deltaTime;

  _Bool _camMoving;
  vec2 camera, _camNew, _camStart;

  vec2 _mouseStart;
  vec2 _spritePosition;

  char *input;
  size_t _inCount, _inCap;

  UI_t _uiRoot;

  AppInfo_t info;
} App_t;

typedef void(*UiCBCK_t)(App_t *, UI_t *);

typedef struct __UiButton_t {
  vec4 onHoverColor;
  UiCBCK_t onClick;
} UiButton_t;

typedef struct __UiButtonInfo_t {
  vec4 color;
  vec4 onHoverColor;
  UiCBCK_t onClick;

  vec2 position, size;
} UiButtonInfo_t;

void __UI_initButton(UI_t *self, UiButtonInfo_t *specInfo) {
  UiButton_t *unique = (self->_unique = malloc(sizeof(UiButton_t)));
  *unique = (UiButton_t) {
    .onClick = specInfo->onClick,
  };

  DEBUG_ASSERT(
    sizeof(unique->onHoverColor) == sizeof(specInfo->onHoverColor),
    "UiButton_t.onHoverColor must be of same type as UiButtonInfo_t.onHoverColor - vec4(cglm)"
  );
  memcpy(unique->onHoverColor, specInfo->onHoverColor, sizeof(unique->onHoverColor));
}

UI_t *UI_addChildButton(UI_t *self, UiButtonInfo_t *specInfo) {
  UiInfo_t genInfo = {
    .type = UI_EL_TYPE_BUTTON,
    .parent = NULL,
  };

  DEBUG_ASSERT(
    sizeof(genInfo.color) == sizeof(specInfo->color), 
    "genInfo.color and specInfo->color must be of same type"
  );
  memcpy(genInfo.color, specInfo->color, sizeof(genInfo.color));

  DEBUG_ASSERT(
    sizeof(genInfo.position) == sizeof(specInfo->position), 
    "genInfo.position and specInfo->position must be of same type"
  );
  memcpy(genInfo.position, specInfo->position, sizeof(genInfo.position));

  DEBUG_ASSERT(
    sizeof(genInfo.size) == sizeof(specInfo->size), 
    "genInfo.size and specInfo->size must be of same type"
  );
  memcpy(genInfo.size, specInfo->size, sizeof(genInfo.size));

  UI_t *child = UI_addChild(self, &genInfo);
  __UI_initButton(child, specInfo);

  return child;
}

_Bool UI_isHovered(UI_t* self, vec2 mouseWorldPos) {
  vec2 bottomLeft = {0};
  vec2 topRight = {0};
  glm_vec2_copy(self->_globalPos, bottomLeft);
  glm_vec2_copy(self->_globalPos, topRight);

  vec2 halfSize = {
    self->_size[0] / 2.f,
    self->_size[1] / 2.f
  };

  topRight[0] += halfSize[0];
  bottomLeft[0] -= halfSize[0];

  bottomLeft[1] -= halfSize[1];
  topRight[1] += halfSize[1];

  _Bool hovered = mouseWorldPos[0] < topRight[0] &&
    mouseWorldPos[0] > bottomLeft[0];
  hovered = hovered && 
    (mouseWorldPos[1] < topRight[1] && mouseWorldPos[1] > bottomLeft[1]);

  return hovered;
}

void UI_processMouseInput(UI_t* self, vec2 mouseWorldPos) {
  for (UI_t *child = self->children;
    child < &self->children[self->childCount]; child++) {
      UI_processMouseInput(child, mouseWorldPos);
  }

  _Bool hovered = UI_isHovered(self, mouseWorldPos);

  switch (self->_type) {
    case UI_EL_TYPE_BUTTON: {
      UiButton_t *unique = self->_unique;

      if (hovered) {
        memcpy(self->_color, unique->onHoverColor, sizeof(self->_color));
      } else {
        memcpy(self->_color, self->color, sizeof(self->_color));
      }
      
      break;
    }
    default:
      break;
  }
}

#define FPS_LIMIT 144
#define FRAME_TIME (1.0 / FPS_LIMIT)

_Bool __App_frameTimeElapsed(App_t *app) {
  double newTime = glfwGetTime();
  double delta = newTime - app->_lastTime;
  if (delta < FRAME_TIME) {
    return false;
  }

  app->_deltaTime = delta;
  app->_lastTime = newTime;
  return true;
}

#define BASE_SPEED 2.f

void _App_wndCloseCBCK(GLFWwindow* window) {
  App_t *app = glfwGetWindowUserPointer(window);

  glfwSetWindowShouldClose(window, GLFW_TRUE);
  app->_running = false;
}

#define CAMERA_MOVE_STRENGTH 50.f

inline void _App_getMouseScreenPosition(App_t *app, vec2 result) {
  double my, mx;
  glfwGetCursorPos(app->_wnd, &mx, &my);

  int fbW = 0, fbH = 0;
  glfwGetFramebufferSize(app->_wnd, &fbW, &fbH);

  result[0] = (2.f * mx - fbW) / fbW ;
  result[1] = -(2.f * my) / fbH;
}

inline void _App_getMouseScreenNormalizedPosition(App_t *app, vec2 result) {
  int fbW = 0, fbH = 0;
  glfwGetFramebufferSize(app->_wnd, &fbW, &fbH);
  float aspect = fbW / (float)fbH;

  // Normalize for aspect
  _App_getMouseScreenPosition(app, result);
  result[0] *= aspect;
}

inline void _App_getMouseScreenNormalizedCentered(App_t *app, vec2 result) {
  int fbW = 0, fbH = 0;
  glfwGetFramebufferSize(app->_wnd, &fbW, &fbH);
  float aspect = fbW / (float)fbH;

  // Normalize for aspect
  _App_getMouseScreenPosition(app, result);
  result[0] *= aspect;
  result[1] += 1.0f;
}

inline void _App_getMouseWorldPosition(App_t *app, vec2 result) {
  int fbW = 0, fbH = 0;
  glfwGetFramebufferSize(app->_wnd, &fbW, &fbH);
  float aspect = fbW / (float)fbH;

  vec2 screenPosition = {0};
  // Normalize for aspect
  _App_getMouseScreenPosition(app, screenPosition);
  screenPosition[0] *= aspect;

  mat4 camInv = GLM_MAT4_IDENTITY_INIT;
  glm_mat4_inv(app->_draw._view, camInv);

  vec3 cursorPos = {0, 0, 0};
  glm_mat4_mulv3(
    camInv,
    (vec3) {screenPosition[0], screenPosition[1], 0.f}, 1.f,
    cursorPos
  );

  result[0] = cursorPos[0];
  result[1] = cursorPos[1];
}

void _App_UI_onClick(App_t *app, UI_t* self, vec2 mouseWorldPos) {
  for (UI_t *child = self->children;
    child < &self->children[self->childCount]; child++) {
      _App_UI_onClick(app, child, mouseWorldPos);
  }

  _Bool hovered = UI_isHovered(self, mouseWorldPos);

  if (!hovered)
    return;

  switch (self->_type) {
    case UI_EL_TYPE_BUTTON: {
      UiButton_t *unique = self->_unique;

      if (unique->onClick == NULL)
        break;

      unique->onClick(app, self);
      break;
    }
    default:
      break;
  }
}

void  _App_updateCamera(App_t *app) {
  if (app->_camMoving) {
    vec2 cPos = {0};
    _App_getMouseWorldPosition(app, cPos);

    vec2 delta = {0};
    glm_vec2_sub(cPos, app->_mouseStart, delta);
    
    glm_vec2_scale(delta, -CAMERA_MOVE_STRENGTH * app->_deltaTime, delta);
    glm_vec2_sub(app->_camStart, delta, app->_camNew);
  }

  glm_vec2_lerp(
    (vec2) { app->camera[0], app->camera[1] },
    app->_camNew,
    app->_deltaTime,
    app->camera
  );
}

void _App_wndMouseBtnCBCK(GLFWwindow* window, int button, int action, int mods) {
  if (button != GLFW_MOUSE_BUTTON_1) {
    return;
  }

  App_t *app = glfwGetWindowUserPointer(window);
  if (action == GLFW_PRESS) {
    _App_getMouseWorldPosition(app, app->_mouseStart);

    memcpy(app->_camStart, app->camera, sizeof(vec2));
    memcpy(app->_camNew, app->camera, sizeof(vec2));
    app->_camMoving = true;
  } else {
    app->_camMoving = false;
  }

  if (button != GLFW_MOUSE_BUTTON_1)
    return;

  if (action != GLFW_RELEASE)
    return;

  vec2 cPos = {0};
  _App_getMouseScreenNormalizedCentered(app, cPos);
  _App_UI_onClick(app, &app->_uiRoot, cPos);
}

void _App_wndInputCBCK(GLFWwindow* window,
    int key, int scancode, int action, int mods) {
  App_t *app = glfwGetWindowUserPointer(window);

  if (action != GLFW_PRESS)
    return;

  if (key == GLFW_KEY_BACKSPACE) {
    // TODO: implement removing chars
  }
  
  if (key != GLFW_KEY_ESCAPE)
    return;

  glfwSetWindowShouldClose(window, GLFW_TRUE);
  app->_running = false;
}

#define SHADER_DIR "glsl\\"
#define VERT_FILE_NAME "vert_default_2d.glsl"
#define FRAG_FILE_NAME "frag_default_2d.glsl"

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

int32_t __intToNextPow2(int32_t val) {
  int32_t out = 1;
  
  while (out < val) {
    out <<= 1;
  }

  return out;
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

    while (*p_buffSize < greaterSize) {
      *p_buffSize <<= 1;
    }
    
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
  glDeleteProgram(app->_draw._texShader);
  glDeleteProgram(app->_draw._flatShader);

  glDeleteBuffers(1, &app->_draw._quadEBO);
  glDeleteBuffers(1, &app->_draw._quadVBO);

  glDeleteVertexArrays(1, &app->_draw._quadVAO);

  glDeleteBuffers(1, &app->_draw._globalUB);
}

typedef struct _Vertex2D_t {
  vec2 position;
  vec2 tex;
} Vertex2D_t;

Result_t __Draw_loadShaderStringFromFiles(GLuint *p_program, const char *vert, const char *frag) {
  FILE *vertFile, *fragFile;
  GLint vertFileSize = 0, fragFileSize = 0;

  errno_t err = 0;
  if ((err = fopen_s(&vertFile, vert, "rb")) != 0 ||
    vertFile == NULL) {
    log_error(
      "Couldn't create/open the vertex shader file %s, error code: %i" ENDL,
      vert,
      err
    );

    return RESULT_FAIL;
  } else {
    log_info("Opened vertex shader file: %s" ENDL, vert);
  }
  
  if ((err = fopen_s(&fragFile, frag, "rb")) != 0 || 
    fragFile == NULL) {
    log_error(
      "Couldn't create/open the fragment shader file %s, error code: %i" ENDL,
      frag,
      err
    );

    fclose(vertFile);
    return RESULT_FAIL;
  } else {
    log_info("Opened fragment shader file: %s" ENDL, frag);
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
    vert, &charBuff, &charCount
  );

  if (!vertResult) {
    log_error("Failed to compile vertex shader (%u : %s)" ENDL,
      vertexShader,
      vert
    );

    return RESULT_FAIL;
  } else {
    log_info("Successfully loaded vertex shader (%u : %s)" ENDL,
      vertexShader,
      vert
    );
  }
#endif

  rewind(fragFile);
  // memset(charBuff, 0, charCount);
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
    frag, &charBuff, &charCount
  );

  if (!fragResult) {
    log_error("Failed to compile fragment shader (%u : %s)" ENDL,
      fragmentShader, frag
    );
    return RESULT_FAIL;
  } else {
    log_info("Successfully loaded fragment shader (%u : %s)" ENDL,
      fragmentShader, frag
    );
  }
#endif

  *p_program = glCreateProgram();
  glAttachShader(*p_program, vertexShader);
  glAttachShader(*p_program, fragmentShader);
  glLinkProgram(*p_program);

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

#ifdef APP_DEBUG
  GLint linkStatus = GL_FALSE;
  glGetProgramiv(*p_program, GL_LINK_STATUS, &linkStatus);
  if (linkStatus != GL_TRUE) {
    GLint requiredLogLength = 0;
    glGetProgramiv(*p_program, GL_INFO_LOG_LENGTH,
      &requiredLogLength
    );

    GLint greaterSize = requiredLogLength > charCount ? 
      requiredLogLength : charCount;

    while (charCount < greaterSize) {
      charCount <<= 1;
    }
    
    charBuff = realloc(charBuff, charCount);

    glGetShaderInfoLog(*p_program, charCount, NULL,
      charBuff);

    log_error("(%u - %s : %s) link error: \"%s\"" ENDL,
      p_program,
      vert,
      frag,
      charBuff
    );

    return RESULT_FAIL;
  } else {
    log_info("Successfully linked shader program (%u - %s : %s)" ENDL,
      *p_program,
      vert,
      frag
    );
  }
#endif
  return RESULT_SUCCESS;
}

Result_t _App_initOpenGL(App_t *app) {
#ifdef APP_DEBUG
  glEnable(GL_DEBUG_OUTPUT);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  glDebugMessageCallback(_App_OpenGL_debugMsgCallback, NULL);
#endif

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  Vertex2D_t vertices[] = {
    { .position = {-0.5f, -0.5f}, .tex = {0, 1} },  // bottom left
    { .position = { 0.5f, -0.5f}, .tex = {1, 1} },  // bottom right
    { .position = { 0.5f,  0.5f}, .tex = {1, 0} },  // top right
    { .position = {-0.5f,  0.5f}, .tex = {0, 0} }   // top left
  };
  
  unsigned int indices[] = {
      0, 1, 2,   // first triangle
      2, 3, 0    // second triangle
  };

  glCreateVertexArrays(1, &app->_draw._quadVAO);
  
  glCreateBuffers(1, &app->_draw._quadVBO);
  glNamedBufferData(app->_draw._quadVBO, sizeof(vertices),
    vertices, GL_STATIC_DRAW
  );
  glVertexArrayVertexBuffer(app->_draw._quadVAO, 0, app->_draw._quadVBO,
    0, sizeof(Vertex2D_t)
  );

  glCreateBuffers(1, &app->_draw._quadEBO);
  glNamedBufferData(app->_draw._quadEBO, sizeof(indices),
    indices, GL_STATIC_DRAW
  );
  glVertexArrayElementBuffer(app->_draw._quadVAO, app->_draw._quadEBO);

  glVertexArrayAttribFormat(app->_draw._quadVAO, 0, 3, GL_FLOAT,
    GL_FALSE, offsetof(Vertex2D_t, position)
  );
  glVertexArrayAttribBinding(app->_draw._quadVAO, 0, 0);
  glEnableVertexArrayAttrib(app->_draw._quadVAO, 0);

  glVertexArrayAttribFormat(app->_draw._quadVAO, 1, 2, GL_FLOAT,
    GL_FALSE, offsetof(Vertex2D_t, tex)
  );
  glVertexArrayAttribBinding(app->_draw._quadVAO, 1, 0);
  glEnableVertexArrayAttrib(app->_draw._quadVAO, 1);

  __Draw_loadShaderStringFromFiles(
    &app->_draw._texShader, 
    SHADER_DIR VERT_FILE_NAME, 
    SHADER_DIR FRAG_FILE_NAME
  );

  __Draw_loadShaderStringFromFiles(
    &app->_draw._flatShader,
    SHADER_DIR "vert_flat_2d.glsl",
    SHADER_DIR "frag_flat_2d.glsl"
  );
  
  glCreateBuffers(1, &app->_draw._globalUB);
  glNamedBufferData(app->_draw._globalUB, sizeof(_GlobalUBData_t),
    &app->_draw._globalUBData, GL_STATIC_DRAW
  );

  glCreateBuffers(1, &app->_draw._localUB);
  glNamedBufferData(app->_draw._localUB,
    sizeof(_LocalUBData2D_t), NULL, GL_STATIC_DRAW
  );

  return RESULT_SUCCESS;
}

#define FONT_PATH "fonts/NotoSans-SemiBold.ttf"
#define FONT_SIZE (1 << 7)
#define ATLAS_START_SIZE (SizeVec2_t) { 1 << 12, 1 << 12 }


_Glyph_t *_Draw_getGlyph(App_t *app, UC_t character) {
  if (character < app->_draw._glyphCount) {
    _Glyph_t *sample = &app->_draw._glyphs[character];

    if (sample->character == character) {
      return sample;
    }
  }

  size_t ucharInd = 0;
  for (;ucharInd < app->_draw._glyphCount; ucharInd++) {
    _Glyph_t *iglyph = &app->_draw._glyphs[ucharInd];
    if (iglyph->character == character) {
      return iglyph;
    }

    if (iglyph->character > character) {
      return NULL;
    }
  }

  return NULL;
}

_Glyph_t *_Draw_getGlyphOrLoad(App_t *app, UC_t character) {
  _Glyph_t *result = _Draw_getGlyph(app, character);
  if (result != NULL) {
    return result;
  }

  _Draw_loadGlyph(app, character);
  return _Draw_getGlyph(app, character);
}

Result_t _Draw_loadGlyph(App_t *app, UC_t character) {
    if (FT_Load_Char(app->_draw._ftFace, character, FT_LOAD_RENDER)) {
      log_warn("Failed to load char %c of font " FONT_PATH ENDL);
      return RESULT_FAIL;
    }

    // generate texture
    GLuint texture = 0;
    if (app->_draw._ftFace->glyph->bitmap.buffer == NULL)
      goto skip_glyph_texture_creation;

    glCreateTextures(GL_TEXTURE_2D, 1, &texture);
    glTextureStorage2D(texture, 1, GL_R8,
      app->_draw._ftFace->glyph->bitmap.width, app->_draw._ftFace->glyph->bitmap.rows
    );

    // set texture options
    glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTextureSubImage2D(texture, 0, 
      0, 0,
      app->_draw._ftFace->glyph->bitmap.width, app->_draw._ftFace->glyph->bitmap.rows,
      GL_RED, GL_UNSIGNED_BYTE, app->_draw._ftFace->glyph->bitmap.buffer
    );

skip_glyph_texture_creation:
    _Glyph_t glyph = {
      .character = character,
      .glTextureHandle = texture,
      .isWhitespace = app->_draw._ftFace->glyph->bitmap.buffer == NULL,
      .size = { 
        app->_draw._ftFace->glyph->bitmap.width, 
        app->_draw._ftFace->glyph->bitmap.rows 
      },
      .bearing = { 
        app->_draw._ftFace->glyph->bitmap_left, 
        app->_draw._ftFace->glyph->bitmap_top 
      },
      .advance = { 
        (uint32_t)app->_draw._ftFace->glyph->advance.x,
        (uint32_t)app->_draw._ftFace->glyph->advance.y 
      }
    };

    app->_draw._glyphCount++;

    while (app->_draw._glyphCap < app->_draw._glyphCount * sizeof(_Glyph_t)) {
      app->_draw._glyphCap <<= 1;
    }
    app->_draw._glyphs = realloc(app->_draw._glyphs, app->_draw._glyphCap);

    size_t ucharInd = 0;
    for (;ucharInd < app->_draw._glyphCount - 1; ucharInd++) {
      _Glyph_t *iglyph = &app->_draw._glyphs[ucharInd];
      if (iglyph->character == character) {
        log_error("Unicode character has already been loaded" ENDL);
        return RESULT_FAIL;
      }

      if (iglyph->character > character) {
        break;
      }
    }

    size_t spaceDiff = app->_draw._glyphCount - ucharInd - 1;
    if (spaceDiff > 0) {
      memcpy(
        &app->_draw._glyphs[ucharInd + 1],
        &app->_draw._glyphs[ucharInd],
        sizeof(_Glyph_t)
      );
    }

    app->_draw._glyphs[ucharInd] = glyph;

    return RESULT_SUCCESS;
}

#define ASCII_LOAD_LIMIT 128

Result_t _Draw_loadAsciiGlyphs(App_t *app) {
  if (FT_Init_FreeType(&app->_draw._ft) != 0) {
    log_error("Failed to init/load freetype library" ENDL);
    return RESULT_FAIL;
  }

  if (FT_New_Face(app->_draw._ft, FONT_PATH, 0, &app->_draw._ftFace)) {
    log_error("Failed to load font at: " FONT_PATH ENDL);
    return RESULT_FAIL;
  }

  FT_Set_Pixel_Sizes(app->_draw._ftFace, 0, FONT_SIZE);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  // TODO:
  // Create texture atlas optimisations

  app->_draw._glyphCount = 0,
  app->_draw._glyphCap = DEFAULT_BUF_CAP,
  app->_draw._glyphs = malloc(app->_draw._glyphCap),

  memset(app->_draw._glyphs, 0, app->_draw._glyphCap);
  for (UC_t character = 0; character < ASCII_LOAD_LIMIT; character++) {
    _Draw_loadGlyph(app, character);
  }

  return RESULT_SUCCESS;
}

void _App_cleanupTextRenderer(App_t *app) {
  FT_Done_Face(app->_draw._ftFace);
  FT_Done_FreeType(app->_draw._ft);

  for (size_t glyphIndex = 0; glyphIndex < app->_draw._glyphCount; glyphIndex++) {
    _Glyph_t *glyph = &app->_draw._glyphs[glyphIndex];
    glDeleteTextures(1, &glyph->glTextureHandle);
  }
  free(app->_draw._glyphs);
}

typedef struct __TextInfo_t {
  uint32_t fontSize;
  float horSpacing;
  float vertSpacing;
} TextInfo_t;

#define TEXT_INFO_INIT (TextInfo_t) { \
  .fontSize = 28,                     \
  .horSpacing = 0.2f,                 \
  .vertSpacing = 0.5f,                \
  }

#pragma region RIPPED_FROM_GITHUB //https://gist.github.com/tylerneylon/9773800

// This macro tests if a char is a continuation byte in utf8.
#define IS_CONT(x) (((x) & 0xc0) == 0x80)

// This returns the code point encoded at **s and advances *s to point to the
// next character. Thus it can easily be used in a loop.
#ifdef _MSC_VER
  #define COUNT_LEADING_BITS(x) __lzcnt(x)
#else
  #define COUNT_LEADING_BITS(x) __builtin_clz(x)
#endif

int decode_code_point(char **s) {
    int k = **s ? COUNT_LEADING_BITS(~(**s << 24)) : 0; // Count # of leading 1 bits.
    int mask = (1 << (8 - k)) - 1;                 // All 1s with k leading 0s.
    int value = **s & mask;
    // k = 0 for one-byte code points; otherwise, k = #total bytes.
    for (++(*s), --k; k > 0 && IS_CONT(**s); --k, ++(*s)) {
        value <<= 6;
        value += (**s & 0x3F);
    }
    return value;
}

// This assumes that `code` is <= 0x10FFFF and ensures that nothing will be
// written at or beyond `end`. It advances *s so it's easy to use in a loop.
void encode_code_point(char **s, char *end, int code) {
    char val[4];
    int lead_byte_max = 0x7F;
    int val_index = 0;
    while (code > lead_byte_max) {
        val[val_index++] = (code & 0x3F) | 0x80;
        code >>= 6;
        lead_byte_max >>= (val_index == 1 ? 2 : 1);
    }
    val[val_index++] = (code & lead_byte_max) | (~lead_byte_max << 1);
    while (val_index-- && *s < end) {
        **s = val[val_index];
        (*s)++;
    }
}

// This returns 0 if no split was needed.
int split_into_surrogates(int code, int *surr1, int *surr2) {
    if (code <= 0xFFFF) return 0;
    *surr2 = 0xDC00 | (code & 0x3FF);        // Save the low 10 bits.
    code >>= 10;                             // Drop the low 10 bits.
    // If `code` now has low bits "uuu uuxx xxxx", then the bits of *surr are
    // "1101 10ww wwxx xxxx" where wwww = (uuuuu - 1).
    *surr1 = 0xD800 | ((code & 0x7FF) - 0x40);
    return 1;
}

// This expects to be used in a loop and see all code points in *code. Start
// *old at 0; this function updates *old for you - don't change it after
// initialization. This returns 0 when *code is the 1st of a surrogate pair;
// otherwise use *code as the final code point.
int join_from_surrogates(int *old, int *code) {
    if (*old) *code = (((*old & 0x3FF) + 0x40) << 10) + (*code & 0x3FF);
    *old = ((*code & 0xD800) == 0xD800 ? *code : 0);
    return !(*old);
}

#pragma endregion RIPPED_FROM_GITHUB

void _App_wndCharCBCK(GLFWwindow* window, unsigned int character) {
  App_t *app = glfwGetWindowUserPointer(window);

  char encoded[4];
  int lead_byte_max = 0x7F;
  int encoded_index = 0;
  while (character > lead_byte_max) {
      encoded[encoded_index++] = (character & 0x3F) | 0x80;
      character >>= 6;
      lead_byte_max >>= (encoded_index == 1 ? 2 : 1);
  }
  encoded[encoded_index++] = (character & lead_byte_max) | (~lead_byte_max << 1);

  // +1 for null terminator 
  app->_inCount += encoded_index;
  while (app->_inCap < app->_inCount) {
    app->_inCap <<= 1;
  }
  app->input = realloc(app->input, app->_inCap);

  while (encoded_index--) {
    app->input[app->_inCount - encoded_index - 1] = encoded[encoded_index];
  }

  app->input[app->_inCount] = '\0';
}

void _Draw_text(App_t *app, const unsigned char *text,
  const Transform_t transform, const TextInfo_t info) {
  glUseProgram(app->_draw._texShader);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, app->_draw._globalUB);
  glBindVertexArray(app->_draw._quadVAO);

  _LocalUBData2D_t ubData = {
    .color = COLOR_RED,
    .model = GLM_MAT4_IDENTITY_INIT
  };
  vec2 lPos = { transform.position[0], transform.position[1] };

  int wwidth = 0, wheight = 0;
  glfwGetFramebufferSize(app->_wnd, &wwidth, &wheight);
  const float normalizationFactor = info.fontSize / 
    ((float)FONT_SIZE * wheight);
  // DOESN'T TAKE X/Y SPACING INTO ACCOUNT
  const float scaleX = transform.scale[0] * normalizationFactor;
  const float scaleY = transform.scale[1] * normalizationFactor;

  uint32_t peakYAdvance = 0;
  float peakYBearing = 0;

  UC_t code_point = 0;
  while ((code_point = decode_code_point(&text)) != '\0') {
      // Print the code point and the bytes consumed
      // printf("Code point: U+%04X, Bytes: %zu, Character: ", code_point, bytes_consumed);
      // for (size_t i = 0; i < bytes_consumed; i++) {
      //     putchar(text[pos - bytes_consumed + i]);
      // }
      // putchar('\n');

      _Glyph_t *glyph = _Draw_getGlyphOrLoad(app, code_point);
      lPos[0] += ((glyph->advance[0] >> 6) / 2.f + info.horSpacing) * scaleX;

      if (glyph->isWhitespace || code_point == '\n')
        goto skip_glyph_rendering;

      vec3 glyphPosition = {
        0.f
      };

      Transform_t trans = {
        .rotation = transform.rotation,
        .position = {
          lPos[0] + glyph->bearing[0] * scaleX,
          lPos[1] - (glyph->size[1] / 2.f - glyph->bearing[1]) * scaleY
        },
        .scale = {
          glyph->size[0] * scaleX,
          glyph->size[1] * scaleY
        }
      };

      Transform_toMat4(&trans, ubData.model);

      glNamedBufferSubData(app->_draw._localUB, 0,
        sizeof(_LocalUBData2D_t), &ubData
      );
      glBindBufferBase(GL_UNIFORM_BUFFER, 1, app->_draw._localUB);
      glBindTextureUnit(0, glyph->glTextureHandle);
      glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
      
skip_glyph_rendering:
      lPos[0] += ((glyph->advance[0] >> 6) / 2.f) * scaleX;
      
      peakYAdvance = glyph->advance[1] > peakYAdvance ?
        glyph->advance[1] : peakYAdvance;

      peakYBearing = glyph->bearing[1] > peakYBearing ?
        glyph->bearing[1] : peakYBearing;
      if (code_point == '\n') {
        lPos[1] -= ((peakYAdvance >> 6) + peakYBearing + info.vertSpacing) * scaleY;
        lPos[1] -= scaleY * info.vertSpacing;

        lPos[0] = transform.position[0];
      }
  }
}

void _Draw_UI(App_t* app, UI_t *ui) {
  glm_mat4_copy(app->_draw._projection, app->_draw._globalUBData.projectionView);
  glNamedBufferSubData(app->_draw._globalUB, 0,
    sizeof(_GlobalUBData_t), &app->_draw._globalUBData
  );

  _LocalUBData2D_t ubData = {0};
  glm_mat4_copy(ui->_matrix, ubData.model);
  glm_vec4_copy(ui->_color, ubData.color);

  glNamedBufferSubData(app->_draw._localUB, 0,
    sizeof(_LocalUBData2D_t), &ubData
  );

  glUseProgram(app->_draw._flatShader);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, app->_draw._globalUB); 
  glBindBufferBase(GL_UNIFORM_BUFFER, 1, app->_draw._localUB);
  glBindVertexArray(app->_draw._quadVAO);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

  for (UI_t *child = ui->children;
    child < &ui->children[ui->childCount]; child++) {
    _Draw_UI(app, child);
  }
}

void _Draw_loadCamera(App_t *app, vec2 cameraPosition) {
  glm_mat4_identity(app->_draw._view);
  glm_translate(
    app->_draw._view,
    (vec3) { 
      cameraPosition[0], 
      cameraPosition[1], 
      0.0 
    }
  );

  glm_mat4_mul(
    app->_draw._projection,
    app->_draw._view,
    app->_draw._globalUBData.projectionView
  );

  glNamedBufferSubData(app->_draw._globalUB, 0, 
    sizeof(_GlobalUBData_t), &app->_draw._globalUBData
  );
}

void _App_render(App_t *app) {
  glfwMakeContextCurrent(app->_wnd);
  
  _Draw_loadCamera(app, app->camera);
  glClear(GL_COLOR_BUFFER_BIT);

  _LocalUBData2D_t spriteUB = {
    .color = {0.75, 0.0, 0.5, 1.0},
    .model = GLM_MAT4_IDENTITY_INIT
  };
  glm_translate(spriteUB.model, 
    (vec3) { app->_spritePosition[0], app->_spritePosition[1], 0 }
  );
  glm_scale(spriteUB.model, 
    (vec3) { 0.1f, 0.1f, 0 }
  );
  glNamedBufferSubData(app->_draw._localUB, 0, 
    sizeof(_LocalUBData2D_t), &spriteUB
  );

  glUseProgram(app->_draw._flatShader);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, app->_draw._globalUB); 
  glBindBufferBase(GL_UNIFORM_BUFFER, 1, app->_draw._localUB);
  glBindVertexArray(app->_draw._quadVAO);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

  spriteUB = (_LocalUBData2D_t) {
    .color = {0.75, 0.0, 0.5, 1.0},
    .model = GLM_MAT4_IDENTITY_INIT
  };

  // Drawing cursor
  vec3 cPos = {0};
  _App_getMouseWorldPosition(app, cPos);

  // INVESTIGATE
  cPos[1] += 1;

  glm_translate(spriteUB.model, cPos);
  glm_scale(spriteUB.model, (vec3) {0.025, 0.025, 1.f});

  glNamedBufferSubData(app->_draw._localUB, 0, 
    sizeof(_LocalUBData2D_t), &spriteUB
  );
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, app->_draw._globalUB); 
  glBindBufferBase(GL_UNIFORM_BUFFER, 1, app->_draw._localUB);
  glBindVertexArray(app->_draw._quadVAO);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

  
  char cPosBuffer[64];
  sprintf_s(cPosBuffer, sizeof(cPosBuffer) / sizeof(char), 
    "Cursor Pos: (%.1f, %.1f)", cPos[0], cPos[1]);
  
  _Draw_text(app, cPosBuffer, (Transform_t) {
      .position = {-0.5, -0.5},
      .rotation = 0.f,
      .scale = { 1.f, 1.f }
    },
    (TextInfo_t) {
      .fontSize = 100,
      .horSpacing = 10.f,
      .vertSpacing = 1.f
    }
  );
  // Drawing cursor

  _Draw_text(app, "This shit is Epic\n we goon to femboys twin", 
    (Transform_t) {
      .position = {0.5, 0.5},
      .rotation = -15.f,
      .scale = { 10.f, 1.f }
    },
    TEXT_INFO_INIT
  );

  _Draw_text(app, "Това е тест", 
    (Transform_t) {
      .position = {-0.5, -0.5},
      .rotation = 90.f,
      .scale = { 2.f, 2.f }
    },
    TEXT_INFO_INIT
  );

  _Draw_text(app, app->input, 
    (Transform_t) {
      .position = {0.f, 0.f},
      .rotation = 0.f,
      .scale = { 10.f, 10.f }
    },
    TEXT_INFO_INIT
  );

  char fpsBuffer[36];
  sprintf_s(fpsBuffer, sizeof(fpsBuffer) / sizeof(char), 
    "FPS: %.2f", (1.0 / app->_deltaTime));
  
  _Draw_text(app, fpsBuffer, (Transform_t) {
      .position = {0.5, 0.5},
      .rotation = 0.f,
      .scale = { 1.f, 1.f }
    },
    (TextInfo_t) {
      .fontSize = 100,
      .horSpacing = 10.f,
      .vertSpacing = 1.f
    }
  );

  _Draw_UI(app, &app->_uiRoot);
  glfwSwapBuffers(app->_wnd);
}

void __testButtonCallback(App_t  *app, UI_t *self) {
  log_info("Button clicked" ENDL);
  vec2 newPosition = {0};
  glm_vec2_copy(self->_pos, newPosition);
  newPosition[1] += 0.01;
  UI_setPosition(self, newPosition);
}

Result_t _App_initUI(App_t *app) {
  UiInfo_t info = {
    .color = COLOR_RED,
    .parent = NULL,
    .position = {0.f, 0.f},
    .size = {2.0, 0.25f},
    .type = UI_EL_TYPE_CONTAINER
  };

  UI_init(&app->_uiRoot, &info);

  UiContainerInfo_t containerInfo = {
    .color = COLOR_WHITE,
    .size = {1.f, 0.5},
    .position = {0},
    .flags = CONTAINER_FLAG_ORDER_VERTICAL
  };

  __UI_initContainer(&app->_uiRoot, &containerInfo);
  UI_addChildContainer(&app->_uiRoot, &containerInfo);

  UiButtonInfo_t buttonInfo = {
    .color = {0.7, 0.2f, 0.5, 1.0},
    .onHoverColor = {0.f, 1.f, 0.f, 1.f},
    .onClick = __testButtonCallback,
    .position = {0, 0},
    .size = {1.f, 0.5f}
  };
  UI_addChildButton(&app->_uiRoot, &buttonInfo);
  buttonInfo.size[0] = 0.5f;
  buttonInfo.size[1] = 0.25f;
  buttonInfo.position[0] += 0.5;
  UI_addChildButton(&app->_uiRoot, &buttonInfo);

  return RESULT_SUCCESS;
}

void _App_cleanupUI(App_t *app) {
  UI_destroy(&app->_uiRoot);
}

#define MOVEMENT_CUTOFF 0.5f

void _App_update(App_t *app)
{
  _App_updateCamera(app);

  vec2 offset = {0, 0};

  if (glfwGetKey(app->_wnd, GLFW_KEY_UP) == GLFW_PRESS)
    offset[1] += 1;

  if (glfwGetKey(app->_wnd, GLFW_KEY_DOWN) == GLFW_PRESS)
    offset[1] -= 1;

  if (glfwGetKey(app->_wnd, GLFW_KEY_LEFT) == GLFW_PRESS)
    offset[0] -= 1;
  
  if (glfwGetKey(app->_wnd, GLFW_KEY_RIGHT) == GLFW_PRESS)
    offset[0] += 1;

  float offsetMag = sqrtf(offset[0] * offset[0] + offset[1] * offset[1]);
  if (offsetMag > MOVEMENT_CUTOFF) {
    float offsetMagNorm = 1.f / offsetMag;
    float transformMultiplier = 
      (float)app->_deltaTime * offsetMagNorm;

    glm_vec2_scale(offset, transformMultiplier * BASE_SPEED, offset);
    glm_vec2_add(app->_spritePosition, offset, app->_spritePosition);
  }

  // vec2 newUiPos = {0, 0.1 * app->_deltaTime};
  // glm_vec2_add(app->_uiRoot.children[1]._pos, newUiPos, newUiPos);
  // UI_setPosition(
  //   &app->_uiRoot.children[1],
  //   newUiPos
  // );

  // TEMP//REMOVE
  vec2 cPos = {0};
  _App_getMouseScreenNormalizedCentered(app, cPos);
  UI_processMouseInput(&app->_uiRoot, cPos);
}

void App_destroy(App_t *app) {
  DEBUG_ASSERT(app != NULL, "App is set to null on App_destroy");

  _App_cleanupUI(app);
  _App_cleanupTextRenderer(app);
  _App_OpenGlCleanup(app);
  
  DEBUG_ASSERT(app->_wnd != NULL, "GLFWwindow app->_wnd is set to NULL, not initialized");
  glfwDestroyWindow(app->_wnd);
  
  free(app);
}

void App_run(App_t *app) {
  _App_getMouseWorldPosition(app, app->_mouseStart);

  while (app->_running) {
    if (!__App_frameTimeElapsed(app)) {
      continue;
    }

    glfwPollEvents();
    _App_render(app);
    _App_update(app);
  }
}

inline void __App_calculateProjection(App_t *app) {
  int fbfW = 0, fbfH = 0;
  glfwGetFramebufferSize(app->_wnd, &fbfW, &fbfH);

  float aspect =  fbfW / (float)fbfH;
  glm_ortho(
    -aspect, aspect,
    -1.f, 1.f,
    -1.f, 1.f,
    app->_draw._projection
  );
}

void _App_wndFbResizeCBCK(GLFWwindow *window, int width, int height) {
  App_t *app = glfwGetWindowUserPointer(window);

  glViewport(0, 0, width, height);
  __App_calculateProjection(app);
  __UI_updateMatrix(&app->_uiRoot);

  if (!__App_frameTimeElapsed(app))
    return;

  _App_update(app);
  _App_render(app);
}

Result_t App_create(App_t** p_app, AppInfo_t info) {
  *p_app = malloc(sizeof(App_t));

  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  // glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

#ifdef APP_DEBUG
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

  GLFWwindow *window;
  if (!(window = glfwCreateWindow(info.size.width, info.size.height, info.name, NULL, NULL))) {
    return RESULT_FAIL;
  }

  **p_app = (App_t) {
    ._wnd = window,
    ._running = true,

    .camera = {0, 0},
    ._camNew = {0, 0},
    ._camMoving = false,

    ._draw = (struct __Draw_t) {
      ._quadEBO = 0,
      ._quadVAO = 0,
      ._quadVBO = 0,

      ._texShader = 0,
      ._flatShader = 0,

      ._view = GLM_MAT4_IDENTITY_INIT,
      ._projection = GLM_MAT4_IDENTITY_INIT,

      ._globalUB = 0, ._localUB = 0,
      ._globalUBData = (_GlobalUBData_t) {
        .projectionView = GLM_MAT4_IDENTITY_INIT 
      }
    },

    ._spritePosition = {0, 0},
    ._mouseStart = {0, 0},

    .info = info,
    .input = malloc(DEFAULT_BUF_CAP),
    ._inCap = DEFAULT_BUF_CAP,
    ._inCount = 0,

    ._lastTime = 0.0,
    ._deltaTime = 0.0,
  };

  (*p_app)->input[0] = '\0';

  __App_calculateProjection(*p_app);

  glfwSetWindowUserPointer(window, *p_app);
  glfwSetWindowCloseCallback(window, _App_wndCloseCBCK);
  glfwSetKeyCallback(window, _App_wndInputCBCK);
  glfwSetMouseButtonCallback(window, _App_wndMouseBtnCBCK);
  glfwSetFramebufferSizeCallback(window, _App_wndFbResizeCBCK);
  glfwSetCharCallback(window, _App_wndCharCBCK);

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

  if (_App_initOpenGL(*p_app) != RESULT_SUCCESS) {
    log_error("Failed to setup OpenGL resources" ENDL);
    return RESULT_FAIL;
  }

  if (_Draw_loadAsciiGlyphs(*p_app) != RESULT_SUCCESS) {
    log_error("Failed to load glyphs" ENDL);
    return RESULT_FAIL;
  }

  if (_App_initUI(*p_app) != RESULT_SUCCESS) {
    log_error("Failed to initialize app ui" ENDL);
    return RESULT_FAIL;
  }

  return RESULT_SUCCESS;
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
    log_add_fp(logFile, 0);
    log_info("Opened log file: %s" ENDL, LOG_FILE_NAME);
  }

  if (glfwInit() != GLFW_TRUE) {
    log_error("Failed to init GLFW");
    glfwTerminate();

    fclose(logFile);
    return -1;
  }

  App_t *app = NULL;
  if (App_create(&app, APP_INFO_INIT) != RESULT_SUCCESS) {
    log_info("Program failed at app creation" ENDL);

    App_destroy(app);
    glfwTerminate();

    fclose(logFile);
    return -1;
  }

  App_run(app);
  App_destroy(app);

  glfwTerminate();

  log_info("Program ended successfully" ENDL);
  fclose(logFile);
  return 0;
}