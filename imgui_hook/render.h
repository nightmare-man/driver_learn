#pragma once
#include "tool.h"
void render_box(int xmin, int ymin, int xmax, int ymax, unsigned int color);
void render_text(VEC2 pos, unsigned int color, float size, const char* format, ...);
void render_point(int x, int y, unsigned int color, float radius =3.0f);
void render_line(VEC2 start, VEC2 end, unsigned int color, float width = 3.0f);
void render_circle(VEC2 pos, float radius, unsigned int color, float width);