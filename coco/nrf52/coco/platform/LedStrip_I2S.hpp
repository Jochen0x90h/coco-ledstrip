#pragma once

#include <coco/BufferDevice.hpp>
#include <coco/BufferImpl.hpp>
#include <coco/Frequency.hpp>
#include <coco/platform/Loop_RTC0.hpp>
#include <coco/platform/gpio.hpp>
#include <coco/platform/nvic.hpp>


namespace coco {

/**
	Implementation of LED strip interface on nrf52 using I2S.

	Reference manual:
		https://infocenter.nordicsemi.com/topic/ps_nrf52840/uarte.html?cp=5_0_0_5_33
	Resources:
		UARTE
*/
class LedStrip_I2S : public BufferDevice /*, public Loop_RTC0::Handler*/ {
protected:
	LedStrip_I2S(Loop_RTC0 &loop, int sckPin, int lrckPin, int dataPin, int bitTime, int resetTime);
public:
	/**
		Constructor
		@param loop event loop
		@param bitTime bit time, e.g. T = 1125ns (T0H = 375ns, T1H = 750ns)
		@param resetTime reset time in us, e.g. 20μs
		@param sckPin i2s sck pin needs to be an unused pin
		@param lrckPin i2s lrck pin needs to be an unused pin
		@param dataPin pin that transmits data to the LED strip
	*/
	LedStrip_I2S(Loop_RTC0 &loop, int sckPin, int lrckPin, int dataPin, NanoSeconds<> bitTime, MicroSeconds<> resetTime)
		: LedStrip_I2S(loop, sckPin, lrckPin, dataPin, bitTime.value, resetTime.value) {}

	~LedStrip_I2S() override;

	class BufferBase;

	State state() override;
	[[nodiscard]] Awaitable<> stateChange(int waitFlags = -1) override;
	int getBufferCount();
	BufferBase &getBuffer(int index);


	// internal buffer base class, derives from IntrusiveListNode for the list of active transfers and Loop_RTC0::Handler2 to be notified from the event loop
	class BufferBase : public BufferImpl, public IntrusiveListNode, public Loop_RTC0::Handler2 {
		friend class LedStrip_I2S;
	public:
		/**
			Constructor
			@param data data of the buffer
			@param capacity capacity of the buffer
			@param channel channel to attach to
		*/
		BufferBase(uint8_t *data, int size, LedStrip_I2S &device);
		~BufferBase() override;

		bool start(Op op) override;
		bool cancel() override;

	protected:
		void startTx();
		void handle() override;

		LedStrip_I2S &device;
	};

	/**
		Buffer for transferring data to LED strip.
		@tparam B capacity of buffer
	*/
	template <int B>
	class Buffer : public BufferBase {
	public:
		Buffer(LedStrip_I2S &device) : BufferBase(data, B, device) {}

	protected:
		alignas(4) uint8_t data[B];
	};

	// needs to be called from I2S interrupt handler
	void I2S_IRQHandler() {
		// check if tx pointer has been read
		if (NRF_I2S->EVENTS_TXPTRUPD) {
			NRF_I2S->EVENTS_TXPTRUPD = 0;
			update();
		}
	}

protected:
	void update();

	Loop_RTC0 &loop;
	int resetWords;

	// dummy (state is always READY)
	CoroutineTaskList<> stateTasks;

	// list of buffers
	IntrusiveList<BufferBase> buffers;

	// list of active transfers
	nvic::Queue<BufferBase> transfers;

	// data to transfer
	uint8_t *data;
	uint8_t *end;

	// reset after data
	int resetCount;

	// number of idle buffers to send when no new data arrives
	int idleCount;

	// buffer for 2 x 16 LEDs
	static constexpr int BUFFER_SIZE = 16 * 3;
	int32_t buffer[2 * BUFFER_SIZE];
	int offset = 0;
	int size = 0;

	enum class Phase {
		STOPPED,
		COPY,
		CLEAR,
		FINISHED,
		IDLE
	};
	Phase phase = Phase::STOPPED;
};

} // namespace coco
