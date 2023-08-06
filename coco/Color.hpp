#pragma once

#include <cstdint>
#include <coco/assert.hpp>


namespace coco {

/**
 * Fixed point number
 * @tparam F fractional bits (e.g. for F = 8, a value of 256 represents 1.0)
 */
template <int F>
struct Fixed {
	int value;
};


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
	//Color scale(int s) const {
	//	return {uint8_t(this->r * s >> 8), uint8_t(this->g * s >> 8), uint8_t(this->b * s >> 8)};
	//}

	uint8_t &operator[](int index) {
		assert(unsigned(index) < 3);
		return (&r)[index];
	}
};

/**
	Multiply color with fixed point value without overflow handling
	@tparam F fractional bits
	@param color color to multiply
	@param factor multiplication factor
	@return multiplied color
*/
template <int F>
Color operator *(const Color &color, Fixed<F> factor) {
	return {uint8_t(color.r * factor.value >> F), uint8_t(color.g * factor.value >> F), uint8_t(color.b * factor.value >> F)};
}



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
	//ColorWhite scale(int s) const {
	//	return {uint8_t(this->r * s >> 8), uint8_t(this->g * s >> 8), uint8_t(this->b * s >> 8), uint8_t(this->w * s >> 8)};
	//}

	uint8_t &operator[](int index) {
		assert(unsigned(index) < 4);
		return (&r)[index];
	}
};

/**
	Multiply color with fixed point value without overflow handling
	@tparam F fractional bits
	@param color color to multiply
	@param factor multiplication factor
	@return multiplied color
*/
template <int F>
ColorWhite operator *(const ColorWhite &color, Fixed<F> factor) {
	return {uint8_t(color.r * factor.value >> F), uint8_t(color.g * factor.value >> F), uint8_t(color.b * factor.value >> F), uint8_t(color.w * factor.value >> F)};
}

} // namespace coco
