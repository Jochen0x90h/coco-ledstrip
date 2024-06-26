#pragma once

#include <coco/BufferDevice.hpp>
#include <coco/BufferImpl.hpp>
#include <coco/Frequency.hpp>
#include <coco/platform/Loop_Queue.hpp>
#include <coco/platform/gpio.hpp>
#include <coco/platform/nvic.hpp>


namespace coco {

/**
	Implementation of LED strip interface on nrf52 using I2S.

	Reference manual:
		https://infocenter.nordicsemi.com/topic/ps_nrf52840/i2s.html?cp=5_0_0_5_10
	Resources:
		I2S
*/
class LedStrip_I2S : public BufferDevice {
protected:
	LedStrip_I2S(Loop_Queue &loop, gpio::Config sckPin, gpio::Config lrckPin, gpio::Config dataPin, int bitTime, int resetTime);
public:
	/**
		Constructor
		@param loop event loop
		@param sckPin i2s sck pin needs to be an unused pin
		@param lrckPin i2s lrck pin needs to be an unused pin
		@param dataPin pin that transmits data to the LED strip
		@param bitTime bit time, e.g. T = 1125ns (T0H = 375ns, T1H = 750ns)
		@param resetTime reset time in us, e.g. 20μs
	*/
	LedStrip_I2S(Loop_Queue &loop, gpio::Config sckPin, gpio::Config lrckPin, gpio::Config dataPin,
		Nanoseconds<> bitTime, Microseconds<> resetTime)
		: LedStrip_I2S(loop, sckPin, lrckPin, dataPin, bitTime.value, resetTime.value) {}

	~LedStrip_I2S() override;


	// internal buffer base class, derives from IntrusiveListNode for the list of active transfers and Loop_Queue::Handler to be notified from the event loop
	class BufferBase : public BufferImpl, public IntrusiveListNode, public Loop_Queue::Handler {
		friend class LedStrip_I2S;
	public:
		/**
			Constructor
			@param data data of the buffer
			@param capacity capacity of the buffer
			@param device led strip device to attach to
		*/
		BufferBase(uint8_t *data, int capacity, LedStrip_I2S &device);
		~BufferBase() override;

		// Buffer methods
		bool start(Op op) override;
		bool cancel() override;

	protected:
		void start();
		void handle() override;

		LedStrip_I2S &device;
	};

	/**
		Buffer for transferring data to LED strip.
		@tparam C capacity of buffer
	*/
	template <int C>
	class Buffer : public BufferBase {
	public:
		Buffer(LedStrip_I2S &device) : BufferBase(data, C, device) {}

	protected:
		alignas(4) uint8_t data[C];
	};


	// Device methods
	State state() override;
	[[nodiscard]] Awaitable<Condition> until(Condition condition) override;

	// BufferDevice methods
	int getBufferCount();
	BufferBase &getBuffer(int index);

	/**
	 * I2S interrupt handler, needs to be called from global I2S interrupt handler
	 */
	void I2S_IRQHandler() {
		// check if tx pointer has been read
		if (NRF_I2S->EVENTS_TXPTRUPD) {
			NRF_I2S->EVENTS_TXPTRUPD = 0;
			handle();
		}
	}

protected:
	void handle();

	Loop_Queue &loop;

	// dummy (state is always READY)
	CoroutineTaskList<Condition> stateTasks;

	// list of buffers
	IntrusiveList<BufferBase> buffers;

	// list of active transfers
	nvic::Queue<BufferBase> transfers;

	// data to transfer
	uint8_t *data;
	uint8_t *end;

	// reset after data
	int resetWords;
	int resetCount;

	// number of idle buffers to send when no new data arrives
	int idleCount;

	// buffer for 2 x 16 LEDs
	static constexpr int LED_BUFFER_SIZE = 16 * 3;
	uint32_t buffer[2 * LED_BUFFER_SIZE];
	int offset = 0;
	int size = 0;

	enum class Phase {
		// nothing to do, I2S is stopped
		STOPPED,

		// copy data to the LED buffer
		COPY,

		// reset LEDs (by sending zeros for the specified Treset time)
		RESET,

		// notify the main application that a buffer has finished
		FINISHED,

		// continue sending zeros for a short time before stopping I2S
		IDLE
	};
	Phase phase = Phase::STOPPED;
};

} // namespace coco
