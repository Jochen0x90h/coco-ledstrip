#include "LedStrip_UART_DMA.hpp"
//#include <coco/debug.hpp>


namespace {
#include "bitTable_UART.hpp"
}

namespace coco {

// LedStrip_UART_DMA

LedStrip_UART_DMA::LedStrip_UART_DMA(Loop_TIM2 &loop, int txPinFunction, const usart::Info &usartInfo,
	const dma::Info &dmaInfo, uint32_t brr, int resetCount)
	: loop(loop), txPin(txPinFunction), resetCount(resetCount)
{
	//gpio::configureOutput(gpio::PA(15), false);

	// configure UART TX pin
	gpio::configureAlternateOutput(txPinFunction, gpio::Speed::HIGH, gpio::Drive::BOTH);
	gpio::setOutput(txPinFunction, false);


	// initialize UART
	usartInfo.enableClock();
	auto uart = this->uart = usartInfo.usart;
	this->uartIrq = usartInfo.irq; // gets enabled in first call to start()
	nvic::setPriority(this->uartIrq, nvic::Priority::MEDIUM);

	// set baud rate (minimum is 8)
	uart->BRR = ((brr & ~7) << 1) | (brr & 7);

	// set configuration registers
	uart->CR3 = USART_CR3_DMAT; // TX DMA mode
	uart->CR2 = USART_CR2_DATAINV | USART_CR2_TXINV; // invert TX
	uart->CR1 = USART_CR1_OVER8 // 8x oversampling
#ifdef USART_CR1_M1
		| USART_CR1_M1 // 7 bit
#endif
		| USART_CR1_UE; // enable UART

	// initialize DMA
	dmaInfo.enableClock();
	this->dma = dmaInfo.dma;

	// initialize TX DMA channel
	this->txChannel = dmaInfo.channel();
	this->txChannel->CPAR = uintptr_t(&uart->TDR);
	//this->dmaIrq = dmaInfo.irq;
	this->dmaFlag = dmaInfo.transferCompleteFlag();

	// connection between UART and DMA
#ifdef STM32F0
	// configure SYSCFG
	int sysConfig = dmaInfo.sysConfig;
	if (sysConfig != 0) {
		RCC->APB2ENR = RCC->APB2ENR | RCC_APB2ENR_SYSCFGCOMPEN;
		SYSCFG->CFGR1 = SYSCFG->CFGR1 | SYSCFG_CFGR1_USART1TX_DMA_RMP;
	}
#endif
#ifdef DMAMUX1
	// configure DMAMUX
	dmaInfo.setMux(usartInfo.mux + 1);
#endif

	// enable transmitter
	auto cr1 = uart->CR1;
	uart->CR1 = cr1 | USART_CR1_TE;
	while ((uart->ISR & USART_ISR_TEACK) == 0);

	//debug::setRed();

	/*uart->TDR = 0x05;
	while (true) {

	}*/

	nvic::enable(dmaInfo.irq);
}

LedStrip_UART_DMA::~LedStrip_UART_DMA() {
}

BufferDevice::State LedStrip_UART_DMA::state() {
	return State::READY;
}

Awaitable<> LedStrip_UART_DMA::stateChange(int waitFlags) {
	if ((waitFlags & (1 << int(State::READY))) == 0)
		return {};
	return {this->stateTasks};
}

int LedStrip_UART_DMA::getBufferCount() {
	return this->buffers.count();
}

LedStrip_UART_DMA::BufferBase &LedStrip_UART_DMA::getBuffer(int index) {
	return this->buffers.get(index);
}

void LedStrip_UART_DMA::handle() {
	auto uart = this->uart;
	auto txChannel = this->txChannel;

	// disable DMA
	txChannel->CCR = 0;

	// clear interrupt flag
	this->dma->IFCR = this->dmaFlag;

	switch (this->phase) {
	case Phase::COPY:
		{
			//gpio::setOutput(gpio::PA(15), true);

			// source data
			uint8_t *src = this->data;
			uint8_t *end = std::min(src + LED_BUFFER_SIZE, this->end);

			// destination
			uint32_t *dst = this->buffer;

			// set DMA pointer
			txChannel->CMAR = uintptr_t(dst);

			// copy/convert
			for (; src < end; src += 3, dst += 2) {
				int a = src[0];
				int b = src[1];
				int c = src[2];

				dst[0] = bitTable[a >> 2]
					| (bitTable[((a & 3) << 4) | b >> 4] << 16);
				dst[1] = bitTable[((b & 15) << 2) | c >> 6]
					| (bitTable[c & 63] << 16);
			}
			//gpio::setOutput(gpio::PA(15), false);

			// set DMA count
			txChannel->CNDTR = uintptr_t(dst) - uintptr_t(this->buffer);

			// check if more source data to transfer
			if (end < this->end) {
				// enable DMA
				txChannel->CCR = DMA_CCR_EN
					| DMA_CCR_DIR // read from memory
					| DMA_CCR_MINC // auto-increment memory
					| DMA_CCR_TCIE; // transfer complete interrupt enable

				// advance source data pointer
				this->data = end;

				// stay in copy phase
				break;
			}

			// enable DMA without transfer complete interrupt
			txChannel->CCR = DMA_CCR_EN
				| DMA_CCR_DIR // read from memory
				| DMA_CCR_MINC; // auto-increment memory

			// enable transmission complete interrupt (TC flag gets cleared automatically by new data)
			//uart->ICR = USART_ICR_TCCF;
			uart->CR1 = uart->CR1 | USART_CR1_TCIE;

			// go to reset phase
			this->phase = Phase::RESET;
		}
		break;
	case Phase::RESET:
		{
			// set output pin low
			gpio::setMode(this->txPin, gpio::Mode::OUTPUT);

			// clear first byte of buffer (not necessary as TX pin is permanently low)
			//this->buffer[0] = 0;

			// set DMA pointer
			txChannel->CMAR = uintptr_t(this->buffer);

			// set DMA count
			txChannel->CNDTR = this->resetCount;

			// enalbe DMA without auto-increment and transfer complete interrupt
			txChannel->CCR = DMA_CCR_EN
				| DMA_CCR_DIR // read from memory
				| 0; // no auto-increment memory

			// enable transmission complete interrupt (TC flag gets cleared automatically by new data)
			//uart->ICR = USART_ICR_TCCF;
			uart->CR1 = uart->CR1 | USART_CR1_TCIE;

			// go to finished phase
			this->phase = Phase::FINISHED;
		}
		break;
	case Phase::FINISHED:
		{
			this->phase = Phase::STOPPED;

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
		break;
	default:
		;
	}
}


// BufferBase

LedStrip_UART_DMA::BufferBase::BufferBase(uint8_t *data, int size, LedStrip_UART_DMA &device)
	: BufferImpl(data, size, BufferBase::State::READY), device(device)
{
	device.buffers.add(*this);
}

LedStrip_UART_DMA::BufferBase::~BufferBase() {
}

bool LedStrip_UART_DMA::BufferBase::start(Op op) {
	if (this->p.state != State::READY) {
		assert(this->p.state != State::BUSY);
		return false;
	}
	auto &device = this->device;

	// check if READ or WRITE flag is set
	assert((op & Op::READ_WRITE) != 0);

	// add to list of pending transfers and start immediately if list was empty
	if (this->device.transfers.push(device.uartIrq, *this))
		start();

	// set state
	setBusy();

	return true;
}

bool LedStrip_UART_DMA::BufferBase::cancel() {
	if (this->p.state != State::BUSY)
		return false;
	auto &device = this->device;

	// remove from pending transfers if not yet started, otherwise complete normally
	if (device.transfers.remove(device.uartIrq, *this, false))
		setReady(0);

	return true;
}

void LedStrip_UART_DMA::BufferBase::start() {
	auto &device = this->device;

	// set data
	device.data = this->p.data;
	device.end = this->p.data + this->p.size;

	// connect tx pin to UART
	gpio::setMode(device.txPin, gpio::Mode::ALTERNATE);

	// start
	device.phase = Phase::COPY;
	device.handle();
}

void LedStrip_UART_DMA::BufferBase::handle() {
	setReady();
}

} // namespace coco
