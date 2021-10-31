#include "devices/dummy.h"
#include "logger.h"

DummyDevice::DummyDevice(DeviceManager* manager)
  : Device(manager) {
  name_ = "dummy";
  
  
	/* Legacy ioport setup */
	/* 0000 - 001F - DMA1 controller */
  AddIoResource(kIoResourceTypePio, 0x0000, 32, "DMA1");

	/* 0x0020 - 0x003F - 8259A PIC 1 */
  AddIoResource(kIoResourceTypePio, 0x0020, 2, "PIC1");

	/* PORT 0040-005F - PIT - PROGRAMMABLE INTERVAL TIMER (8253, 8254) */
  AddIoResource(kIoResourceTypePio, 0x0040, 4, "PIT");

	/* 0x00A0 - 0x00AF - 8259A PIC 2 */
  AddIoResource(kIoResourceTypePio, 0x00A0, 2, "PIC2");

	/* 00C0 - 001F - DMA2 controller */
  AddIoResource(kIoResourceTypePio, 0x00C0, 32, "DMA2");

	/* PORT 00ED - DUMMY PORT FOR DELAY??? */
  AddIoResource(kIoResourceTypePio, 0x00ed, 1, "??");

	/* 0x00F0 - 0x00FF - Math co-processor */
  AddIoResource(kIoResourceTypePio, 0x00f0, 2, "Math Processor");

	/* PORT 0278-027A - PARALLEL PRINTER PORT (usually LPT1, sometimes LPT2) */
  AddIoResource(kIoResourceTypePio, 0x0278, 3, "Parallel LPT1");

	/* PORT 0378-037A - PARALLEL PRINTER PORT (usually LPT2, sometimes LPT3) */
  AddIoResource(kIoResourceTypePio, 0x0378, 3, "Parallel LPT2");

	/* PORT 03D4-03D5 - COLOR VIDEO - CRT CONTROL REGISTERS */
  AddIoResource(kIoResourceTypePio, 0x03d4, 2, "Crt");
}

void DummyDevice::Write(const IoResource& ir, uint64_t offset, uint8_t* data, uint32_t size) {
  // Do nothing
  MV_LOG("%s ignore write base=0x%lx offset=0x%lx size=%d",
    ir.name, ir.base, offset, size);
}

void DummyDevice::Read(const IoResource& ir, uint64_t offset, uint8_t* data, uint32_t size) {
  // Do nothing
  MV_LOG("%s ignore read base=0x%lx offset=0x%lx size=%d",
    ir.name, ir.base, offset, size);
}
