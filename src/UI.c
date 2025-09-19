#include "UI.h"

void UiSize_copy(UiSize_t *dst, UiSize_t *src) {
  memcpy(dst->dimentions, src->dimentions, sizeof(dst->dimentions));
  dst->flag = src->flag;
}

void __UI_calculateMatrix(UI_t *self) {
  glm_mat4_identity(self->_matrix);
  
  glm_translate(self->_matrix, (vec3){
      self->_globalPos[0],
      self->_globalPos[1],
      0.f
    }
  );

  float width = self->_size.width;
  float height = self->_size.height;

  if (self->parent != NULL &&
      self->_size.flag & UI_SIZE_FLAG_FILL_HEIGHT) {
    height = self->parent->_size.height;
  }
  if (self->parent != NULL &&
      self->_size.flag & UI_SIZE_FLAG_FILL_WIDTH) {
    width = self->parent->_size.width;
  }

  glm_scale(self->_matrix,
    (vec3) { width, height, 1.0f }
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

void UI_setSize(UI_t *self, UiSize_t newSize) {
  DEBUG_ASSERT(
    newSize.width < 1.01f && newSize.height < 1.01f &&
    newSize.width > 0.f && newSize.height > 0.f,
    "Size cannot exceed 1.f or be less than 0.f"
  );

  UiSize_copy(&self->_size, &newSize);
  __UI_updateMatrix(self);
}

Result_t UI_init(UI_t *self, UiInfo_t *info) {
  self->childCap = DEFAULT_CHILD_CAP;
  self->children = malloc(self->childCap);
  self->childCount = 0;

  self->type = info->type;
  self->parent = info->parent;
  self->flags = info->flags;

  memcpy(self->_pos, info->position, sizeof(vec2));
  UiSize_copy(&self->_size, &info->size);
  memcpy(self->color, info->color, sizeof(vec4));
  memcpy(self->_color, info->color, sizeof(vec4));

  self->id = info->id;

  __UI_updateMatrix(self);
  return EXIT_SUCCESS;
}

UI_t *UI_addChild(UI_t *self, UiInfo_t *info) {
  DEBUG_ASSERT(info->id != ROOT_ID, "Child cannot have root id");

  self->childCount++;

  while (self->childCount * sizeof(UI_t) > self->childCap) {
    self->childCap <<= 1;
  }

  UI_t *oldChildPtr = self->children;
  self->children = realloc(self->children, self->childCap);
  
  // IF THE BLOCK IS REALLOCATED THE SECOND LEVEL CHILDREN WILL LOSE REFERENCE TO THEIR PARENTS
  if (oldChildPtr != self->children) {
    for (UI_t *first = self->children; first < &self->children[self->childCount]; first++) {
      for (UI_t *sec = first->children; sec < &first->children[first->childCount]; sec++) {
        sec->parent = first;
      }
    }
  }

  UI_t *child = &self->children[self->childCount - 1];
  info->parent = self;
  UI_init(child, info);

  return child;
}

UI_t *UI_findById(UI_t *root, UiId_t id) {
  if (id == NO_ID) {
    return root;
  }

  if (id == ROOT_ID) {
    return root;
  }

  if (id == root->id) {
    return root;
  }

  for (UI_t *child = root->children; child < &root->children[root->childCount]; child++) {
    return UI_findById(child, id);
  }

  return root;
}

UiId_t UI_addChildById(UI_t *root, UiInfo_t *info) {
  UI_t *parent = UI_findById(root, info->parentId);
  UI_addChild(parent, info);
  return parent->id;
}

void UI_destroy(UI_t *self) {
  for (UI_t *child = self->children;
    child < &self->children[self->childCount]; child++) {
      UI_destroy(child);
  }

  switch (self->type) {
    case UI_EL_TYPE_INPUT:
    case UI_EL_TYPE_TEXT:
      UStr_destroy(&((UiText_t *)self->_unique)->str);
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

void __UI_initContainer(UI_t *self, UiContainerInfo_t *specInfo) {
  UiContainer_t *unique = (self->_unique = malloc(sizeof(UiContainer_t)));
  *unique = (UiContainer_t) {
    ._offsetAccumulation = 0
  };
}

UI_t *UI_addChildContainer(UI_t *self, UiInfo_t *info, UiContainerInfo_t *specInfo) {
  info->type = UI_EL_TYPE_CONTAINER;
  info->parent = self;

  UI_t *child = UI_addChild(self, info);
  __UI_initContainer(child, specInfo);

  return child;
}

UiId_t UI_addChildContainerById(UI_t *root, UiInfo_t *info, UiContainerInfo_t *specInfo) {
  UI_t *parent = UI_findById(root, info->parentId);
  UI_addChildContainer(parent, info, specInfo);
  return parent->id;
}

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

UI_t *UI_addChildButton(UI_t *self, UiInfo_t *info, UiButtonInfo_t *specInfo) {
  info->type = UI_EL_TYPE_BUTTON;
  info->parent = self;

  UI_t *child = UI_addChild(self, info);
  __UI_initButton(child, specInfo);

  return child;
}

UiId_t UI_addChildButtonById(UI_t *root, UiInfo_t *info, UiButtonInfo_t *specInfo) {
  UI_t *parent = UI_findById(root, info->parentId);
  UI_addChildButton(parent, info, specInfo);
  return parent->id;
}

bool UI_isHovered(UI_t* self, vec2 mouseWorldPos) {
  vec2 bottomLeft = {0};
  vec2 topRight = {0};
  glm_vec2_copy(self->_globalPos, bottomLeft);
  glm_vec2_copy(self->_globalPos, topRight);

  float halfWidth = self->_size.width / 2.f;
  float halfHeight = self->_size.height / 2.f;

  topRight[0] += halfWidth;
  bottomLeft[0] -= halfWidth;

  bottomLeft[1] -= halfHeight;
  topRight[1] += halfHeight;

  bool hovered = mouseWorldPos[0] < topRight[0] &&
    mouseWorldPos[0] > bottomLeft[0];
  hovered = hovered && 
    (mouseWorldPos[1] < topRight[1] && mouseWorldPos[1] > bottomLeft[1]);

  return hovered;
}

void __UI_initText(UI_t *self, UiTextInfo_t *specInfo) {
  UiText_t *unique = (self->_unique = malloc(sizeof(UiText_t)));
  UStr_init(&unique->str, specInfo->str);
}

UI_t *UI_addChildText(UI_t *self, UiInfo_t *info, UiTextInfo_t *specInfo) {
  info->type = UI_EL_TYPE_TEXT,
  info->parent = self;

  UI_t *child = UI_addChild(self, info);
  __UI_initText(child, specInfo);

  return child;
}


UiId_t UI_addChildTextById(UI_t *root, UiInfo_t *info, UiTextInfo_t *specInfo) {
  UI_t *parent = UI_findById(root, info->parentId);
  UI_addChildText(parent, info, specInfo);
  return parent->id;
}

void EventQueue_init(EventQueue_t *self) {
  self->cap = DEFAULT_BUF_CAP;
  self->count = 0;
  self->events = malloc(self->cap);
}

void EventQueue_cleanup(EventQueue_t *self) {
  free(self->events);
  self->count = 0;
  self->cap = 0;
}

void EventQueue_push(EventQueue_t *self, Event_t *ev) {
  self->count++;
  while (self->cap < self->count * sizeof(Event_t)) {
    self->cap <<= 1;
  }
  self->events = realloc(self->events, self->cap);

  memcpy(&self->events[self->count - 1], ev, sizeof(Event_t));
}

bool EventQueue_pop(EventQueue_t *self, Event_t *ev) {
  if (self->count == 0) {
    *ev = (Event_t){0};
    return false;
  }

  self->count--;
  memcpy(ev, &self->events[self->count], sizeof(Event_t));
  return true;
}

bool UI_buttonProcessEvent(UI_t *self, void *ctx, Event_t *ev) {
  UiButton_t *unique = self->_unique;
  bool hovered = UI_isHovered(self, ev->position);

  switch(ev->type) {
    case EVENT_TYPE_CLICK: {
      if (!hovered)
        return false;

      unique->onClick(ctx, self);
      return true;
    }
    case EVENT_TYPE_MOUSE_MOVE: {
      if (hovered)
        memcpy(self->_color, unique->onHoverColor, sizeof(self->_color));
      else
        memcpy(self->_color, self->color, sizeof(self->_color));

      return false; // NEVER ABSORB
    }
    default:
      break;
  }

  return false;
}

void __UI_initInput(UI_t *self, UiInputInfo_t *specInfo) {
  UiInput_t *unique = (self->_unique = malloc(sizeof(UiInput_t)));
  UStr_init(&unique->str, specInfo->str);
  self->flags &= ~UI_FLAG_FOCUS;
}

UI_t *UI_addChildInput(UI_t *self, UiInfo_t *info, UiInputInfo_t *specInfo) {
  info->type = UI_EL_TYPE_INPUT;
  info->parent = self;

  UI_t *child = UI_addChild(self, info);
  __UI_initInput(child, specInfo);

  return child;
}

// THIS SHOULD INLINE
UiId_t UI_addChildInputById(UI_t *root, UiInfo_t *info, UiInputInfo_t *specInfo) {
  UI_t *parent = UI_findById(root, info->parentId);
  UI_addChildInput(parent, info, specInfo);
  return parent->id;
}

bool UI_inputProcessEvent(UI_t *self, Event_t *ev) {
  UiInput_t *unique = self->_unique;
  bool hovered = UI_isHovered(self, ev->position);

  // TODO: create a way of deactivation -> event that gets sent out when something is focused
  switch(ev->type) {
    case EVENT_TYPE_CLICK: {
      if (!hovered) {
        if (self->flags & UI_FLAG_FOCUS) {
          self->flags &= ~UI_FLAG_FOCUS;
          self->_color[0] = 0.f;
        }
        return false;
      }

      self->flags |= UI_FLAG_FOCUS;
      self->_color[0] = 1.f;
      return true;
    }
    case EVENT_TYPE_CHAR_INPUT: {
      if (!(self->flags & UI_FLAG_FOCUS))
        return false;

      UStr_pushUC(&unique->str, ev->character);
      return true;
    }
    case EVENT_TYPE_KEY: {
      if (!(self->flags & UI_FLAG_FOCUS))
        return false;

      if (ev->glfwAction != GLFW_REPEAT && ev->glfwAction != GLFW_PRESS)
        return false;
      if (ev->glfwKey != GLFW_KEY_BACKSPACE)
        return false;

      UStr_trimEnd(&unique->str, 1);
      return true;
    }
    default:
      break;
  }

  return false;
}