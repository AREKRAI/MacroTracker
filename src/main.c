#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <GLFW/glfw3.h>

#include "MacroDatabase.h"

#if defined(WINDOWS) || defined(_WIN32) || defined(_WIN64)
    #define CLEAR_CONSOLE_COMMAND "cls"
#else
    // Assume POSIX
    #define CLEAR_CONSOLE_COMMAND "clear"
#endif

#define CLEAR_CONSOLE(...) system(CLEAR_CONSOLE_COMMAND)

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

int main(void) {
  // ConsoleApp_t *app = ConsoleApp_create();
  // ConsoleApp_run(app);
  // ConsoleApp_destroy(app);
  if (glfwInit() != GLFW_TRUE)
  {
    printf("Failed to init GLFW\n");
  }

  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  GLFWwindow *window = glfwCreateWindow(1024, 512, "MacroApp", NULL, NULL);

  while (!glfwWindowShouldClose(window))
  {
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}