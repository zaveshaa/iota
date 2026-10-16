#ifndef LEVEL_H
#define LEVEL_H

#include "world.h"

int level_save(const Scene *s, const char *path);
int level_load(Scene *s, const char *path);

#endif
