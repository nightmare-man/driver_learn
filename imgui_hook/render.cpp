#include "render.h"
#include "imgui.h"
#include <stdio.h>
void render_box(int xmin, int ymin, int xmax, int ymax, unsigned int color) {
	ImVec2 pmin{ (float)xmin,(float)ymin };
	ImVec2 pmax{ (float)xmax,(float)ymax };
	ImGui::GetBackgroundDrawList()->AddRect(pmin, pmax, color,0,0,3.0f);
}
void render_text_intern(int x, int y, unsigned int color, float size, const char* text) {
	ImVec2 pos{ (float)x,(float)y };
	ImGui::GetBackgroundDrawList()->AddText(NULL, size, pos, (ImU32)color, text);
}
void render_text(VEC2 pos, unsigned int color, float size, const char* format, ...) {

	ImVec2 pos1{ (float)pos.x,(float)pos.y };
	
	va_list args;
	va_start(args, format);
	char buffer[256] = { 0 };
	vsprintf_s(buffer, 256, format, args);
	va_end(args);
	ImGui::GetBackgroundDrawList()->AddText(NULL, size, pos1, (ImU32)color, buffer);
}

void render_point(int x, int y, unsigned int color, float radius) {
	ImVec2 pos{ (float)x,(float)y };
	ImGui::GetBackgroundDrawList()->AddCircleFilled(pos, radius, (ImU32)color);
}

void render_line(VEC2 start, VEC2 end, unsigned int color, float width) {
	ImVec2 pos1{ (float)start.x,(float)start.y };
	ImVec2 pos2{ (float)end.x,(float)end.y };
	ImGui::GetBackgroundDrawList()->AddLine(pos1, pos2, (ImU32)color, width);
}
void render_circle(VEC2 pos, float radius, unsigned int color, float width) {
	ImVec2 pos1{ (float)pos.x,(float)pos.y };
	ImGui::GetBackgroundDrawList()->AddCircle(pos1, radius, (ImU32)color, 0, width);
}