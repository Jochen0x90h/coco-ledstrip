#include <coco/debug.hpp>
#include <coco/Color.hpp>
#include <LedStripTest.hpp>


using namespace coco;


struct SingleBufferStrip {
	// number of LEDs
	int count;

	// single buffer
	Buffer &buffer;

	SingleBufferStrip(Buffer &buffer) : buffer(buffer) {
		this->count = buffer.capacity() / sizeof(Color);
	}

	int size() {return this->count;}

	// get array of colors, only valid until show() is called
	Array<Color> array() {
		return {this->buffer.pointer<Color>(), this->count};
	}

	[[nodiscard]] Awaitable<> show() {
		// write buffer
		return this->buffer.write(this->count * sizeof(Color));
	}
};

struct DoubleBufferStrip {
	// number of LEDs
	int count;

	// double buffer
	Buffer *buffers[2];
	Buffer *buffer;

	DoubleBufferStrip(Buffer &buffer1, Buffer &buffer2) : buffers{&buffer1, &buffer2} {
		this->count = std::min(buffer1.capacity(), buffer2.capacity()) / sizeof(Color);
		this->buffer = &buffer1;
	}

	int size() {return this->count;}

	// get array of colors, only valid until show() is called
	Array<Color> array() {
		return {this->buffer->pointer<Color>(), this->count};
	}

	[[nodiscard]] Awaitable<> show() {
		// start writeing buffer
		this->buffer->startWrite(this->count * sizeof(Color));

		// toggle buffer
		this->buffer = this->buffer == this->buffers[0] ? this->buffers[1] : this->buffers[0];

		// wait until other buffer is finished
		return this->buffer->untilReadyOrDisabled();
	}
};

template <typename S>
Coroutine effect(Loop &loop, S &strip) {
	while (true) {
		// get time in milliseconds
		auto time = int(loop.now());
		int rOffset = time / 32;
		int gOffset = time * 2 / 32;
		int bOffset = time * 3 / 32;


		auto leds = strip.array();
		for (int i = 0; i < leds.size(); ++i) {
			Color &color = leds[i];

			int rx = i + rOffset;
			color.r = (rx & 256) ? 255 - rx : rx;

			int gx = i + gOffset;
			color.g = (gx & 256) ? 255 - gx : gx;

			int bx = i + bOffset;
			color.b = (bx & 256) ? 255 - bx : bx;
		}
		co_await strip.show();
	}
}


int main() {
	//singleBuffer(drivers.loop, drivers.buffer);
	//doubleBuffer(drivers.loop, drivers.buffer, drivers.buffer2);

	//SingleBufferStrip strip(drivers.buffer);
	DoubleBufferStrip strip(drivers.buffer, drivers.buffer2);
	effect(drivers.loop, strip);

	drivers.loop.run();
	return 0;
}
