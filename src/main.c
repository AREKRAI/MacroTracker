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

typedef struct __LocalUBData2D_t {
  mat4 model;
  vec4 color;
} _LocalUBData2D_t;

#define COLOR_WHITE {1.f, 1.f, 1.f, 1.f}
#define COLOR_RED {1.f, 0.f, 0.f, 1.f}

typedef uint32_t u32vec2[2];

typedef unsigned int UC_t;

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

typedef struct _App_t {
  struct __Draw_t {
    FT_Library _ft;
    FT_Face _ftFace;

    GLuint _shader;
    GLuint _quadVAO, _quadVBO, _quadEBO;

    mat4 _projection;

    GLuint _globalUB, _localUB;
    _GlobalUBData_t _globalUBData;

    _Glyph_t *_glyphs;
    size_t _glyphCap, _glyphCount;
  } _draw;

  GLFWwindow *_wnd;
  _Bool _running;

  double _lastTime;
  double _deltaTime;

  vec2 camera;
  vec2 _cameraTarget;

  vec2 _mouseStart;
  vec2 _spritePosition;
} App_t;

#define DEFAULT_WINDOW_WIDTH 1024
#define DEFAULT_WINDOW_HEIGHT 512

#include "Common.h"

#define BASE_SPEED 2.f

void _App_windowCloseCallback(GLFWwindow* window) {
  App_t *app = glfwGetWindowUserPointer(window);

  glfwSetWindowShouldClose(window, GLFW_TRUE);
  app->_running = false;
}

void _App_framebufferResizeCallback(GLFWwindow *window, int width, int height) {
  App_t *app = glfwGetWindowUserPointer(window);

  float aspect = width / (float)height;

  float left = -1.f;
  float right = 1.f;
  float bottom = -1.f / aspect;
  float top = 1.f / aspect;

  glm_ortho(
    left, right,
    bottom, top, 
    -1.f, 1.f, 
    app->_draw._projection
  );

  glViewport(0, 0, width, height);
}

void  _App_calculateMouseTransform(App_t *app) {
  double mx, my;
  glfwGetCursorPos(app->_wnd, &mx, &my);

  vec3 start = {app->_mouseStart[0], app->_mouseStart[1], 0};
  vec3 end = {(float)mx, (float)my, 0};

  vec3 startProd;
  vec3 endProd;


  glm_mat4_mulv3(app->_draw._projection, start, 1.0, startProd);
  glm_mat4_mulv3(app->_draw._projection, end, 1.0, endProd);

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
  }

  _App_calculateMouseTransform(app);
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
  glDeleteProgram(app->_draw._shader);

  glDeleteBuffers(1, &app->_draw._quadEBO);
  glDeleteBuffers(1, &app->_draw._quadVBO);

  glDeleteVertexArrays(1, &app->_draw._quadVAO);

  glDeleteBuffers(1, &app->_draw._globalUB);
}

typedef struct _Vertex2D_t {
  vec2 position;
  vec2 tex;
} Vertex2D_t;

Result_t _App_setupOpenGl(App_t *app) {
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


  FILE *vertFile, *fragFile;
  GLint vertFileSize = 0, fragFileSize = 0;

  errno_t err = 0;
  if ((err = fopen_s(&vertFile, VERT_FILE_NAME, "rb")) != 0 ||
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
  
  if ((err = fopen_s(&fragFile, FRAG_FILE_NAME, "rb")) != 0 || 
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

  app->_draw._shader = glCreateProgram();
  glAttachShader(app->_draw._shader, vertexShader);
  glAttachShader(app->_draw._shader, fragmentShader);
  glLinkProgram(app->_draw._shader);

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

#ifdef APP_DEBUG
  GLint linkStatus = GL_FALSE;
  glGetProgramiv(app->_draw._shader, GL_LINK_STATUS, &linkStatus);
  if (linkStatus != GL_TRUE) {
    GLint requiredLogLength = 0;
    glGetProgramiv(app->_draw._shader, GL_INFO_LOG_LENGTH,
      &requiredLogLength
    );

    GLint greaterSize = requiredLogLength > charCount ? 
      requiredLogLength : charCount;

    charCount = __intPow2ScaleToNext(charCount, greaterSize);
    
    charBuff = realloc(charBuff, charCount);

    glGetShaderInfoLog(app->_draw._shader, charCount, NULL,
      charBuff);

    log_error("(%u - %s : %s) link error: \"%s\"" ENDL,
      app->_draw._shader,
      VERT_FILE_NAME,
      FRAG_FILE_NAME,
      charBuff
    );
  } else {
    log_info("Successfully linked shader program (%u - %s : %s)" ENDL,
      app->_draw._shader,
      VERT_FILE_NAME,
      FRAG_FILE_NAME
    );
  }
#endif
  
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
      memccpy(
        &app->_draw._glyphs[ucharInd + 1],
        &app->_draw._glyphs[ucharInd],
        spaceDiff, sizeof(_Glyph_t)
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

Result_t App_create(App_t** p_app) {
  *p_app = malloc(sizeof(App_t));

  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  // glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

#ifdef APP_DEBUG
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

  GLFWwindow *window;
  if (!(window = glfwCreateWindow(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, "MacroApp", NULL, NULL))) {
    return RESULT_FAIL;
  }

  **p_app = (App_t) {
    ._wnd = window,
    ._running = true,

    .camera = {0, 0},
    ._cameraTarget = {0, 0},

    ._draw._shader = 0,

    ._draw = (struct __Draw_t) {
      ._quadEBO = 0,
      ._quadVAO = 0,
      ._quadVBO = 0,
      

      ._globalUB = 0, ._localUB = 0,
      ._globalUBData = {0},
    },

    ._spritePosition = {0, 0},
    ._mouseStart = {0, 0},
  };

  int fbfW = 0, fbfH = 0;
  glfwGetFramebufferSize((*p_app)->_wnd, &fbfW, &fbfH);

  float aspect = fbfW / (float)fbfH;

  mat4 projection = GLM_MAT4_IDENTITY_INIT;

  float left = -1.f;
  float right = 1.f;
  float bottom = -1.f / aspect;
  float top = 1.f / aspect;

  glm_ortho(
    left, right,
    bottom, top, 
    -1.f, 1.f,
    (*p_app)->_draw._projection
  );
  glm_mat4_identity((*p_app)->_draw._globalUBData.projectionView);

  glfwSetWindowUserPointer(window, *p_app);
  glfwSetWindowCloseCallback(window, _App_windowCloseCallback);
  glfwSetKeyCallback(window, _App_keyInputCallback);
  glfwSetMouseButtonCallback(window, _App_mouseButtonCallback);
  glfwSetFramebufferSizeCallback(window, _App_framebufferResizeCallback);

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

  if (_App_setupOpenGl(*p_app) != RESULT_SUCCESS) {
    log_error("Failed to setup OpenGL resources" ENDL);
    return RESULT_FAIL;
  }

  if (_Draw_loadAsciiGlyphs(*p_app) != RESULT_SUCCESS) {
    log_error("Failed to load glyphs" ENDL);
    return RESULT_FAIL;
  }

  return RESULT_SUCCESS;
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

void _Draw_text(App_t *app, const unsigned char *text,
  const Transform_t transform, const TextInfo_t info) {
  glUseProgram(app->_draw._shader);
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
        lPos[0] + glyph->bearing[0] * scaleX,
        lPos[1] - (glyph->size[1] / 2.f - glyph->bearing[1]) * scaleY,
        0.f
      };

      mat4 scaleMatrix = GLM_MAT4_IDENTITY_INIT;
      mat4 transformMatrix = GLM_MAT4_IDENTITY_INIT;
      glm_scale(scaleMatrix, 
        (vec3) { 
          glyph->size[0] * scaleX,
          glyph->size[1] * scaleY,
          1.f
        }
      );

      glm_translate(transformMatrix, glyphPosition);
      mat4 rotationMatrix = GLM_MAT4_IDENTITY_INIT;
      glm_rotate(rotationMatrix, 
        transform.rotation * GLM_PI / 180.0, 
        (vec3){0, 0, 1}
      );
      glm_mul(rotationMatrix, transformMatrix, ubData.model);
      glm_mat4_mul(ubData.model, scaleMatrix, ubData.model);

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

void _App_render(App_t *app) {
  glfwMakeContextCurrent(app->_wnd);
  glClear(GL_COLOR_BUFFER_BIT);

  glUseProgram(app->_draw._shader);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, app->_draw._globalUB); 
  glBindBufferBase(GL_UNIFORM_BUFFER, 1, app->_draw._localUB);
  glBindTextureUnit(0, app->_draw._glyphs['A'].glTextureHandle);
  glBindVertexArray(app->_draw._quadVAO);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
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
  glfwSwapBuffers(app->_wnd);
}

#define MOVEMENT_CUTOFF 0.5f

void _App_update(App_t *app)
{
  vec2 offset = {0, 0};

  if (glfwGetKey(app->_wnd, GLFW_KEY_W) == GLFW_PRESS)
    offset[1] += 1;

  if (glfwGetKey(app->_wnd, GLFW_KEY_S) == GLFW_PRESS)
    offset[1] -= 1;

  if (glfwGetKey(app->_wnd, GLFW_KEY_A) == GLFW_PRESS)
    offset[0] -= 1;
  
  if (glfwGetKey(app->_wnd, GLFW_KEY_D) == GLFW_PRESS)
    offset[0] += 1;

  float offsetMag = sqrtf(offset[0] * offset[0] + offset[1] * offset[1]);
  if (offsetMag > MOVEMENT_CUTOFF) {
    float offsetMagNorm = 1.f / offsetMag;
    float transformMultiplier = 
      (float)app->_deltaTime * offsetMagNorm;

    glm_vec2_scale(offset, transformMultiplier * BASE_SPEED, offset);
    
    glm_vec2_add(app->_spritePosition, offset, app->_spritePosition);
  }


  glm_vec2_lerp(app->camera, app->_cameraTarget, (float)app->_deltaTime, app->camera);

  mat4 view = GLM_MAT4_IDENTITY_INIT;
  glm_translate(view, (vec3) { app->camera[0], app->camera[1], 0.0 });

  glm_mat4_mul(app->_draw._projection, view, app->_draw._globalUBData.projectionView);

  // GL CODE -> marked to find if this gets moved into its own function
  // as the number of buffers that are written to grows
  // maybe a function called "Update GL Buffers"
  glNamedBufferSubData(app->_draw._globalUB, 0, 
    sizeof(_GlobalUBData_t), &app->_draw._globalUBData
  );

  _LocalUBData2D_t spriteUB = {
    .color = COLOR_WHITE
  };
  glm_mat4_identity(spriteUB.model);
  glm_translate(spriteUB.model, 
    (vec3) { app->_spritePosition[0], app->_spritePosition[1], 0 }
  );

  glNamedBufferSubData(app->_draw._localUB, 0, 
    sizeof(_LocalUBData2D_t), &spriteUB
  );
}

void App_destroy(App_t *app) {
  if (app == NULL) {
    log_warn("App is set to NULL");
    return;
  }

  _App_cleanupTextRenderer(app);

  _App_OpenGlCleanup(app);
  
  if (app->_wnd != NULL)
    glfwDestroyWindow(app->_wnd);
  
  free(app);
}

#define FPS_LIMIT 144
#define FRAME_TIME (1.0 / FPS_LIMIT)

void App_run(App_t *app) {
  app->_lastTime = glfwGetTime();

  while (app->_running) {
    double newTime = glfwGetTime();

    app->_deltaTime = newTime - app->_lastTime;
    if (app->_deltaTime < FRAME_TIME) {
      continue;
    }
    glfwPollEvents();

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
  if (App_create(&app) != RESULT_SUCCESS) {
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