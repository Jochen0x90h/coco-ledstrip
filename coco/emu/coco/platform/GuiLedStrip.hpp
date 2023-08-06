#pragma once

#include <coco/platform/Gui.hpp>


namespace coco {

/**
	LED strip on the emulator gui.
	Usage: gui.draw<GuiLedStrip>(buffer, length);
*/
class GuiLedStrip : public Gui::Renderer {
public:
	GuiLedStrip();

	Gui::Size draw(float x, float y);
	Gui::Size draw(float x, float y, const uint8_t *buffer, int length);

protected:
	GLuint texture;
};

} // namespace coco
