#include "MacroDatabase.h"

void Meal_logData(Meal_t *meal) {
  printf("Meal (%u/%u/%u)\n", meal->date.day, meal->date.month, meal->date.year);
  printf("cals: %u, protein: %u, fat: %u, carbs: %u\n", meal->macro.cals, meal->macro.protein, meal->macro.fat, meal->macro.carbs);
}

_Bool Meal_validate(Meal_t *self) {
  return self->macro.cals > ((self->macro.protein + self->macro.carbs) * 4 + self->macro.fat * 9);
}

MacroDatabase_t *MacroDatabase_create() {
  MacroDatabase_t *self = malloc(sizeof(MacroDatabase_t));
  *self = (MacroDatabase_t) {
    ._mealCapacity = DEFAULT_BUF_CAP,
    .meals = malloc(sizeof(Meal_t) * DEFAULT_BUF_CAP),
    .mealCount = 0,
  };

  memset(self->meals, 0, self->_mealCapacity * sizeof(Meal_t));

  return self;
}

void MacroDatabase_destroy(MacroDatabase_t *self) {
  free(self->meals);
  
  self->_mealCapacity = 0;
  self->mealCount = 0;
  self->meals = NULL;
  free(self);
}

void MacroDatabase_logToConsole(MacroDatabase_t *self) {
  for (Meal_t *meal = &self->meals[0]; meal < &self->meals[self->mealCount]; meal++) {
    Meal_logData(meal);
  }
}

void _MacroDatabase_resize(MacroDatabase_t *self, uint32_t newCount) {
  self->mealCount = newCount;

  while (self->_mealCapacity < sizeof(Meal_t) * self->mealCount) {
    self->_mealCapacity <<= 1;
  }

  self->meals = realloc(self->meals, self->_mealCapacity);
}

void MacroDatabase_add(MacroDatabase_t *self, Meal_t meal) {
  if (!Meal_validate(&meal)) {
    printf("Meal's macros are greater than calories\n");
    return;
  }

  _MacroDatabase_resize(self, self->mealCount + 1);
  self->meals[self->mealCount - 1] = meal;
}