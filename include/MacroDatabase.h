#ifndef __H_MACRO_DATABASE__
#define __H_MACRO_DATABASE__

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Common.h"

typedef struct {
  uint32_t cals, protein, fat, carbs;
} Macro_t;

typedef struct {
  uint32_t year;
  uint32_t month;
  uint32_t day;
} Date_t;

typedef struct {
  Macro_t macro;
  Date_t date;
} Meal_t;

typedef struct {
  Meal_t *meals;
  uint32_t mealCount;
  uint32_t _mealCapacity;
} MacroDatabase_t;

void Meal_logData(Meal_t *meal);
_Bool Meal_validate(Meal_t *self);
MacroDatabase_t *MacroDatabase_create();
void MacroDatabase_destroy(MacroDatabase_t *self);
void MacroDatabase_logToConsole(MacroDatabase_t *self);
void MacroDatabase_add(MacroDatabase_t *self, Meal_t meal);

#endif