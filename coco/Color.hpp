#pragma once

#include <cstdint>
#include <coco/assert.hpp>


namespace coco {

struct Color {
	uint8_t r;
	uint8_t g;
	uint8_t b;

	/**
		Scale by 24.8 fixed point value without overflow handling
		@param a color to scale
		@param b scale factor in 24.8 fixed point (256 represents 1.0)
		@return scaled color
	*/
	Color scale(int s) const {
		return {uint8_t(this->r * s >> 8), uint8_t(this->g * s >> 8), uint8_t(this->b * s >> 8)};
	}

	uint8_t &operator[](int index) {
		assert(unsigned(index) < 3);
		return (&r)[index];
	}
};



struct ColorWhite {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t w;

	/**
		Scale by 24.8 fixed point value without overflow handling
		@param a color to scale
		@param b scale factor in 24.8 fixed point (256 represents 1.0)
		@return scaled color
	*/
	ColorWhite scale(int s) const {
		return {uint8_t(this->r * s >> 8), uint8_t(this->g * s >> 8), uint8_t(this->b * s >> 8), uint8_t(this->w * s >> 8)};
	}

	uint8_t &operator[](int index) {
		assert(unsigned(index) < 4);
		return (&r)[index];
	}
};

} // namespace coco
