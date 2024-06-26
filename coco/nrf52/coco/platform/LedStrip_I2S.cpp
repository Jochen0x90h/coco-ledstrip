#include "LedStrip_I2S.hpp"
#include <coco/debug.hpp>
#include <coco/platform/platform.hpp>
#include <coco/platform/nvic.hpp>
#include <coco/platform/gpio.hpp>


namespace {
#include "bitTable_I2S.hpp"
}

namespace coco {

LedStrip_I2S::LedStrip_I2S(Loop_Queue &loop, gpio::Config sckPin, gpio::Config lrckPin, gpio::Config dataPin,
	int bitTime, int resetTime)
	: loop(loop)
{
	// debug start indicator pin
	//gpio::configureOutput(P0(19), false);

	// configure I2S pins
	gpio::configureAlternate(dataPin);
	auto i2s = NRF_I2S;
	//i2s->PSEL.MCK = DISCONNECTED;
	i2s->PSEL.SCK = gpio::getPinIndex(sckPin);
	i2s->PSEL.LRCK = gpio::getPinIndex(lrckPin);
	//i2s->PSEL.SDIN = DISCONNECTED;
	i2s->PSEL.SDOUT = gpio::getPinIndex(dataPin);

	// initialize I2S
	i2s->CONFIG.MODE = N(I2S_CONFIG_MODE_MODE, Master);
	i2s->CONFIG.RXEN = 0;
	i2s->CONFIG.TXEN = N(I2S_CONFIG_TXEN_TXEN, Enabled);
	i2s->CONFIG.MCKEN = N(I2S_CONFIG_MCKEN_MCKEN, Enabled);
	i2s->CONFIG.RATIO = N(I2S_CONFIG_RATIO_RATIO, 48X);
	i2s->CONFIG.SWIDTH = N(I2S_CONFIG_SWIDTH_SWIDTH, 24Bit);
	//i2s->CONFIG.ALIGN = N(I2S_CONFIG_ALIGN_ALIGN, Left);
	i2s->CONFIG.FORMAT = N(I2S_CONFIG_FORMAT_FORMAT, Aligned);
	i2s->CONFIG.CHANNELS = N(I2S_CONFIG_CHANNELS_CHANNELS, Stereo);

	// https://devzone.nordicsemi.com/f/nordic-q-a/391/uart-baudrate-register-values
	//int value = (int64_t(bitRate.value * 3) << 32) / 32000000;
	int value = (int64_t(3000) << 32) / int(bitTime * 32);
	i2s->CONFIG.MCKFREQ = (value + 0x800) & 0xFFFFF000;
	//i2s->CONFIG.MCKFREQ = N(I2S_CONFIG_MCKFREQ_MCKFREQ, 32MDIV16);

	i2s->RXTXD.MAXCNT = LED_BUFFER_SIZE;
	i2s->INTENSET = N(I2S_INTENSET_TXPTRUPD, Set);// | N(I2S_INTENSET_STOPPED, Set);
	i2s->ENABLE = N(I2S_ENABLE_ENABLE, Enabled);

	// calc reset time in number of words
	//int actualFreq = int64_t(i2s->CONFIG.MCKFREQ) * 32000000 >> 32;
	int wordFreq = int64_t(i2s->CONFIG.MCKFREQ) * 1333333 >> 32; // frequency of 24 bit words, e.g. 41kHz
	this->resetWords = (wordFreq * resetTime) / 1000000 + 1;
}

LedStrip_I2S::~LedStrip_I2S() {
}

BufferDevice::State LedStrip_I2S::state() {
	return State::READY;
}

Awaitable<Device::Condition> LedStrip_I2S::until(Condition condition) {
	// check if IN_* condition is met
	if ((int(condition) >> int(State::READY)) & 1)
		return {}; // don't wait
	return {this->stateTasks, condition};
}

int LedStrip_I2S::getBufferCount() {
	return this->buffers.count();
}

LedStrip_I2S::BufferBase &LedStrip_I2S::getBuffer(int index) {
	return this->buffers.get(index);
}

void LedStrip_I2S::handle() {
	auto i2s = NRF_I2S;
	switch (this->phase) {
	case Phase::COPY:
		{
			// get LED buffer fill size (already occupied part of the buffer)
			int size = this->size;

			// source data
			uint8_t *begin = this->data;
			uint8_t *src = begin;
			uint8_t *end2 = src + (LED_BUFFER_SIZE - size);
			uint8_t *end = std::min(end2, this->end);

			// destination
			int offset = this->offset;
			uint32_t *dst = this->buffer + offset;
			uintptr_t ptr = uintptr_t(dst);
			dst += size;

			// copy/convert
			for (; src != end; ++src, ++dst) {
				*dst = bitTable[*src];
			}

			// check if LED buffer is full
			if (end == end2) {
				// advance source data pointer
				this->data = end;

				// set DMA pointer to LED buffer
				i2s->TXD.PTR = ptr;

				// toggle and reset LED buffer
				this->offset = offset ^ LED_BUFFER_SIZE;
				this->size = 0;

				// stay in copy phase
				break;
			}

			// end of data: update buffer fill size
			this->size = size + (end - begin);

			// go to reset phase
			this->phase = Phase::RESET;
		}
		// fall through
	case Phase::RESET:
		{
			// get buffer size (already occupied part of the buffer) and number of free words
			int size = this->size;
			int free = LED_BUFFER_SIZE - size;

			// number of words to clear
			int count = this->resetCount;
			int toClear = std::min(count, free);

			// destination
			int offset = this->offset;
			uint32_t *dst = this->buffer + offset;
			uintptr_t ptr = uintptr_t(dst);
			dst += size;
			uint32_t *end = dst + toClear;

			// clear
			for (; dst != end; ++dst) {
				*dst = 0;
			}

			// check if buffer is full
			if (toClear == free) {
				// decrease resetCount
				this->resetCount = count - toClear;

				// set DMA pointer
				i2s->TXD.PTR = ptr;

				// toggle and reset buffer
				this->offset = offset ^ LED_BUFFER_SIZE;
				this->size = 0;

				// stay in reset phase
				break;
			}

			// clear finished: update buffer fill size
			this->size = size + toClear;

			// go to finished phase
			this->phase = Phase::FINISHED;
		}
		// fall through
	case Phase::FINISHED:
		{
			this->phase = Phase::IDLE;

			// set debug start indicator pin
			//gpio::setOutput(P0(19), true);

			this->transfers.pop(
				[this](BufferBase &buffer) {
					// push finished transfer buffer to event loop so that BufferBase::handle() gets called from the event loop
					this->loop.push(buffer);
					return true;
				},
				[](BufferBase &next) {
					// start next transfer if there is one
					next.start();
				}
			);

			// clear debug start indicator pin
			//gpio::setOutput(P0(19), false);
		}
		if (this->phase != Phase::IDLE)
			break;
		// fall through
	case Phase::IDLE:
		// let I2S run for some more cycles
		if (--this->idleCount >= 0) {
			// get buffer size (already occupied part of the buffer)
			int size = this->size;

			// destination
			int offset = this->offset;
			uint32_t *dst = this->buffer + offset;
			uintptr_t ptr = uintptr_t(dst);
			uint32_t *end = dst + LED_BUFFER_SIZE;
			dst += size;

			// clear
			for (; dst != end; ++dst) {
				*dst = 0;//0xffffff;
			}

			// set DMA pointer
			i2s->TXD.PTR = ptr;

			// toggle and reset buffer
			this->offset = offset ^ LED_BUFFER_SIZE;
			this->size = 0;
		} else {
			// stop I2S
			i2s->TASKS_STOP = TRIGGER;
			this->phase = Phase::STOPPED;
		}
		break;
	default:
		;
	}
}


// BufferBase

LedStrip_I2S::BufferBase::BufferBase(uint8_t *data, int capacity, LedStrip_I2S &device)
	: BufferImpl(data, capacity, BufferBase::State::READY), device(device)
{
	device.buffers.add(*this);
}

LedStrip_I2S::BufferBase::~BufferBase() {
}

bool LedStrip_I2S::BufferBase::start(Op op) {
	if (this->p.state != State::READY) {
		assert(this->p.state != State::BUSY);
		return false;
	}

	// check if WRITE flag is set
	assert((op & Op::WRITE) != 0);

	// add to list of pending transfers and start immediately if list was empty
	if (this->device.transfers.push(I2S_IRQn, *this))
		start();

	// set state
	setBusy();

	return true;
}

bool LedStrip_I2S::BufferBase::cancel() {
	if (this->p.state != State::BUSY)
		return false;
	auto &device = this->device;

	// remove from pending transfers if not yet started, otherwise complete normally
	if (device.transfers.remove(I2S_IRQn, *this, false))
		setReady(0);

	return true;
}

void LedStrip_I2S::BufferBase::start() {
	// set debug start indicator pin
	//gpio::setOutput(P0(19), true);

	auto &device = this->device;
	auto i2s = NRF_I2S;

	// set data
	device.data = this->p.data;
	device.end = this->p.data + this->p.size;

	// set reset count (enlarge so that at least one buffer gets filled)
	device.resetCount = std::max(device.resetWords, LED_BUFFER_SIZE - int(this->p.size));

	// set idle count
	device.idleCount = 3;

	// get curren phase
	auto ph = device.phase;

	device.phase = Phase::COPY;
	device.handle();

	if (ph == Phase::STOPPED)
		i2s->TASKS_START = TRIGGER;

	// clear debug start indicator pin
	//gpio::setOutput(P0(19), false);
}

void LedStrip_I2S::BufferBase::handle() {
	// set debug start indicator pin
	//gpio::setOutput(P0(19), true);

	setReady();

	// clear debug start indicator pin
	//gpio::setOutput(P0(19), false);
}

} // namespace coco
