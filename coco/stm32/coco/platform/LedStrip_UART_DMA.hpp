#pragma once

#include <coco/BufferDevice.hpp>
#include <coco/BufferImpl.hpp>
#include <coco/Frequency.hpp>
#include <coco/platform/Loop_Queue.hpp>
#include <coco/platform/dma.hpp>
#include <coco/platform/gpio.hpp>
#include <coco/platform/usart.hpp>
#include <coco/platform/nvic.hpp>


namespace coco {

/**
	Implementation of LED strip interface on stm32 using USARTx, UARTx or LPUARTx.

	Reference manual:
		f0:
			https://www.st.com/resource/en/reference_manual/dm00031936-stm32f0x1stm32f0x2stm32f0x8-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
				USART: Section 27
				DMA: Section 10, Table 29
				Code Examples: Section A.19
		g4:
			https://www.st.com/resource/en/reference_manual/rm0440-stm32g4-series-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
				USART: Section 37
				DMA: Section 12
				DMAMUX: Section 13
	Data sheet:
		f0:
			https://www.st.com/resource/en/datasheet/stm32f042f6.pdf
				Alternate Functions: Section 4, Tables 14-16, Page 37
			https://www.st.com/resource/en/datasheet/dm00039193.pdf
				Alternate Functions: Section 4, Tables 14+15, Page 37
		g4:
			https://www.st.com/resource/en/datasheet/stm32g431rb.pdf
				Alternate Functions: Section 4.11, Table 13, Page 61
	Resources:
		USART or UART
		DMA
*/
class LedStrip_UART_DMA : public BufferDevice {
protected:
	LedStrip_UART_DMA(Loop_Queue &loop, gpio::Config txPin, const usart::Info &uartInfo, const dma::Info &dmaInfo,
		uint32_t brr, int resetCount);
public:
	/**
		Constructor
		@param loop event loop
		@param txPin transmit (TX) pin and alternative function (see data sheet)
		@param usartInfo info of USART/UART instance to use
		@param dmaInfo info of DMA channel to use
		@param clock peripheral clock frequency (USART1: USART1_CLOCK, USART2: USART2_CLOCK, USART3: USART3_CLOCK, UART4 - UART5: APB1_CLOCK)
	*/
	LedStrip_UART_DMA(Loop_Queue &loop, gpio::Config txPin, const usart::Info &uartInfo, const dma::Info &dmaInfo,
		Kilohertz<> clock, Nanoseconds<> bitTime, Microseconds<> resetTime) : LedStrip_UART_DMA(loop, txPin,
		uartInfo, dmaInfo, std::max(int(clock * bitTime / 3) + 1, 8), int(clock / ((int(clock * bitTime / 3) + 1) * 9) * resetTime)) {}

	~LedStrip_UART_DMA() override;


	// internal buffer base class, derives from IntrusiveListNode for the list of buffers and Loop_Queue::Handler to be notified from the event loop
	class BufferBase : public BufferImpl, public IntrusiveListNode, public Loop_Queue::Handler {
		friend class LedStrip_UART_DMA;
	public:
		/**
			Constructor
			@param data data of the buffer
			@param capacity capacity of the buffer
			@param channel channel to attach to
		*/
		BufferBase(uint8_t *data, int size, LedStrip_UART_DMA &device);
		~BufferBase() override;

		// Buffer methods
		bool start(Op op) override;
		bool cancel() override;

	protected:
		void start();
		void handle() override;

		LedStrip_UART_DMA &device;
	};

	/**
		Buffer for transferring data over UART.
		@tparam C capacity of buffer
	*/
	template <int C>
	class Buffer : public BufferBase {
	public:
		Buffer(LedStrip_UART_DMA &device) : BufferBase(data, C, device) {}

	protected:
		alignas(4) uint8_t data[C];
	};


	// Device methods
	State state() override;
	[[nodiscard]] Awaitable<Condition> until(Condition condition) override;

	// BufferDevice methods
	int getBufferCount() override;
	BufferBase &getBuffer(int index) override;

	/**
	 * UART interrupt handler, needs to be called from global USART/UART interrupt handler (e.g. USART1_IRQHandler() for usart::USART1_INFO on STM32G4)
	 */
	void UART_IRQHandler() {
		auto uart = this->uart;

		// check if transmission has completed
		if ((uart->ISR & USART_ISR_TC) != 0) {
			// disable transmission complete interrupt
			uart->CR1 = uart->CR1 & ~(USART_CR1_TCIE);

			handle();
		}
	}

	/**
		DMA interrupt handler, needs to be called from DMA channel interrupt handler (e.g. DMA1_Channel1_IRQHandler() for dma::DMA1_CH1_INFO on STM32G4)
	*/
	void DMA_IRQHandler() {
		// check if receive has completed
		if ((this->dmaStatus.get() & dma::Status::Flags::TRANSFER_COMPLETE) != 0)
			handle();
	}

protected:
	void handle();

	Loop_Queue &loop;

	gpio::Config txPin;

	// uart
	USART_TypeDef *uart;
	int uartIrq;

	// dma
	dma::Status dmaStatus;
	dma::Channel dmaChannel;

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
	int resetCount;

	// buffer for 2 x 16 LEDs
	static constexpr int LED_BUFFER_SIZE = 16 * 3;
	uint32_t buffer[(LED_BUFFER_SIZE * 4) / 3 / 2]; // need 4 x uint16_t for one LED which are 3 bytes

	enum class Phase {
		// nothing to do, I2S is stopped
		STOPPED,

		// copy data to the LED buffer
		COPY,

		// reset LEDs (by sending zeros for the specified Treset time)
		RESET,

		// notify the main application that a buffer has finished
		FINISHED
	};
	Phase phase = Phase::STOPPED;
};

} // namespace coco
