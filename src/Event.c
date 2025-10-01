#include "Event.h"

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