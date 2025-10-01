#ifndef _H_EVENT_
#define _H_EVENT_

#include "Common.h"
#include "UStr.h"

// REMOVE THIS IN THE FUTURE
// BY COPYING THE GLFW KEY MAPPING AND RENAMING IT
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>

// TODO: EVENT SYSTEM
typedef enum __EVENT_TYPE_t {
  EVENT_TYPE_NONE = 0, // EVIL
  // INPUT EVENTS
  EVENT_TYPE_CLICK = 1,
  EVENT_TYPE_CHAR_INPUT = 2,
  EVENT_TYPE_MOUSE_MOVE = 3,
  EVENT_TYPE_FOCUS_SET = 4,
  EVENT_TYPE_KEY = 5,

  // RENDERER EVENTS
  ET_RENDER_FB_RESIZE = 1
} EVENT_TYPE_t;

typedef enum __EVENT_CATEGORY_t {
  EVENT_CAT_NONE = 0,
  EVENT_CAT_INPUT = 1,
  EVENT_CAT_RENDER = 2
} EVENT_CAT_t;

typedef struct __Event_t {
  EVENT_CAT_t category;
  EVENT_TYPE_t type;

  union {
    vec2 position;
    UC_t character;
    struct {
      int glfwKey;
      int glfwAction;
    };
  };
} Event_t;

typedef struct __EventQueue_t {
  size_t count, cap;
  Event_t *events;
} EventQueue_t;

void EventQueue_init(EventQueue_t *self);
void EventQueue_cleanup(EventQueue_t *self);

void EventQueue_push(EventQueue_t *self, Event_t *ev);
bool EventQueue_pop(EventQueue_t *self, Event_t *ev);

#endif