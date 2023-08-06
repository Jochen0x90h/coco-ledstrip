#pragma once

#include <coco/platform/Loop_RTC0.hpp>
#include <coco/platform/LedStrip_I2S.hpp>
#include <coco/board/config.hpp>


using namespace coco;


// drivers for FileTest
struct Drivers {
	Loop_RTC0 loop;
	LedStrip_I2S ledStrip{loop,
		gpio::P1(14), // SCK
		gpio::P0(2),//gpio::P1(15), // LRCK
		gpio::P0(3), // data
		1125ns, // bit time T
		50us}; // reset time
	LedStrip_I2S::Buffer<300 * 3> buffer{ledStrip};
	LedStrip_I2S::Buffer<300 * 3> buffer2{ledStrip};
};

Drivers drivers;

extern "C" {
void I2S_IRQHandler() {
	drivers.ledStrip.I2S_IRQHandler();
}
}
