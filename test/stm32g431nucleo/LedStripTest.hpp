#pragma once

#include <coco/platform/Loop_TIM2.hpp>
#include <coco/platform/LedStrip_UART_DMA.hpp>
#include <coco/board/config.hpp>


using namespace coco;

constexpr int LEDSTRIP_LENGTH = 300;

// drivers for SpiMasterTest
struct Drivers {
	Loop_TIM2 loop{APB1_TIMER_CLOCK, Loop_TIM2::Mode::POLL};

	using LedStrip = LedStrip_UART_DMA;
	LedStrip ledStrip{loop,
		gpio::PA(9, 7), // USART1 TX
		usart::USART1_INFO,
		dma::DMA1_CH4_INFO,
		USART1_CLOCK,
		1125ns, // bit time T
		50us}; // reset time
	LedStrip::Buffer<LEDSTRIP_LENGTH * 3> buffer{ledStrip};
	LedStrip::Buffer<LEDSTRIP_LENGTH * 3> buffer2{ledStrip};
};

Drivers drivers;

extern "C" {
void USART1_IRQHandler() {
	drivers.ledStrip.UARTx_IRQHandler();
}
void DMA1_Channel4_IRQHandler() {
	drivers.ledStrip.DMAx_IRQHandler();
}
}
