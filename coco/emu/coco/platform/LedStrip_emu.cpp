#include "LedStrip_emu.hpp"
#include "GuiLedStrip.hpp"


namespace coco {

LedStrip_emu::LedStrip_emu(Loop_emu &loop)
	: loop(loop)
{
	loop.guiHandlers.add(*this);
}

LedStrip_emu::~LedStrip_emu() {
}

Device::State LedStrip_emu::state() {
	return this->stat;
}

Awaitable<Device::Condition> LedStrip_emu::until(Condition condition) {
	// check if IN_* condition is met
	if ((int(condition) >> int(this->stat)) & 1)
		return {}; // don't wait
	return {this->stateTasks, condition};
}

int LedStrip_emu::getBufferCount() {
	return this->buffers.count();
}

LedStrip_emu::Buffer &LedStrip_emu::getBuffer(int index) {
	return this->buffers.get(index);
}

void LedStrip_emu::handle(Gui &gui) {
	auto buffer = this->transfers.pop();
	if (buffer != nullptr) {
		gui.draw<GuiLedStrip>(buffer->p.data, buffer->p.size / 3);
		buffer->setReady();
	} else {
		// draw emulated LED strip with previous content
		gui.draw<GuiLedStrip>();
	}
}


// Buffer

LedStrip_emu::Buffer::Buffer(int length, LedStrip_emu &device)
	: BufferImpl(new uint8_t[length * 3], length * 3, device.stat)
	, device(device)
{
	device.buffers.add(*this);
}

LedStrip_emu::Buffer::~Buffer() {
	delete [] this->p.data;
}

bool LedStrip_emu::Buffer::start(Op op) {
	if (this->p.state != State::READY) {
		assert(this->p.state != State::BUSY);
		return false;
	}

	// check if WRITE flag is set
	assert((op & Op::WRITE) != 0);

	// add buffer to list of transfers. No need to start first transfer as LedStrip_emu::handle() gets called periodically
	this->device.transfers.push(*this);

	// set state
	setBusy();

	return true;
}

bool LedStrip_emu::Buffer::cancel() {
	if (this->p.state != State::BUSY)
		return false;

	setReady(0);
	return true;
}

} // namespace coco
