#include "device_manager.h"
#include <cstring>
#include <algorithm>
#include "logger.h"
#include "memory_manager.h"
#include "machine.h"
#include "disk_image.h"
#include "device_interface.h"
#include "storage_device.h"

#define CDROM_IMAGE         "/mnt/iso/win2016.iso"
#define HARDDISK_IMAGE      "/data/win10.img"

DeviceManager::DeviceManager(Machine* machine) : machine_(machine) {
}

DeviceManager::~DeviceManager() {
  for (auto device: registered_devices_) {
    delete device;
  }
}

/* Create necessary devices for a Q35 chipset machine  */
void DeviceManager::IntializeQ35() {
  auto ahci_host = PciDevice::Create("AhciHost");
  MV_LOG("ahci host = %p %s", ahci_host, ahci_host->name());
  auto cd = StorageDevice::Create("Cdrom", new RawDiskImage(CDROM_IMAGE, true));
  ahci_host->AddChild(cd);
  auto hd = StorageDevice::Create("Harddisk", new RawDiskImage(HARDDISK_IMAGE, false));
  ahci_host->AddChild(hd);

  auto lpc = PciDevice::Create("Ich9Lpc");
  lpc->AddChild(Device::Create("DebugConsole"));
  lpc->AddChild(Device::Create("Cmos"));
  lpc->AddChild(Device::Create("Keyboard"));
  lpc->AddChild(Device::Create("DummyDevice"));
  lpc->AddChild(Device::Create("SmBus"));
  lpc->AddChild(ahci_host);

  auto pci_host = PciDevice::Create("PciHost");
  pci_host->AddChild(lpc);
  pci_host->AddChild(PciDevice::Create("Qxl"));

  root_ = new SystemRoot(this);
  root_->AddChild(Device::Create("FirmwareConfig"));
  root_->AddChild(pci_host);

  /* Call Connect() on all devices */
  root_->Connect();
}

/* Used for debugging */
void DeviceManager::PrintDevices() {
  for (auto device : registered_devices_) {
    MV_LOG("Device: %s", device->name());
    for (auto &ir : device->io_resources()) {
      switch (ir.type)
      {
      case kIoResourceTypePio:
        MV_LOG("\tIO port 0x%lx-0x%lx", ir.base, ir.base + ir.length - 1);
        break;
      case kIoResourceTypeMmio:
        MV_LOG("\tMMIO address 0x%016lx-0x016%lx", ir.base, ir.base + ir.length - 1);
      case kIoResourceTypeRam:
        MV_LOG("\tRAM address 0x%016lx-0x016%lx", ir.base, ir.base + ir.length - 1);
        break;
      }
    }
  }
}

Device* DeviceManager::LookupDeviceByName(const std::string name) {
  for (auto device : registered_devices_) {
    if (device->name() == name) {
      return device;
    }
  }
  return nullptr;
}

PciDevice* DeviceManager::LookupPciDevice(uint16_t bus, uint8_t devfn) {
  for (auto device : registered_devices_) {
    PciDevice* pci_device = dynamic_cast<PciDevice*>(device);
    if (pci_device && pci_device->devfn_ == devfn) {
      return pci_device;
    }
  }
  return nullptr;
}


void DeviceManager::RegisterDevice(Device* device) {
  // Check devfn conflicts or reassign it
  PciDevice* pci_device = dynamic_cast<PciDevice*>(device);
  if (pci_device) {
    if (LookupPciDevice(pci_device->bus(), pci_device->devfn())) {
      MV_PANIC("PCI device function %x conflicts", pci_device->devfn());
      return;
    }
  }

  registered_devices_.insert(device);
}

void DeviceManager::UnregisterDevice(Device* device) {
  registered_devices_.erase(device);
}


void DeviceManager::RegisterIoHandler(Device* device, const IoResource& io_resource) {
  if (io_resource.type == kIoResourceTypePio) {
    pio_handlers_.push_back(new IoHandler {
      .io_resource = io_resource,
      .device = device
    });
  } else if (io_resource.type == kIoResourceTypeMmio) {
    // Map the memory to type Device, access these regions will cause MMIO access fault
    const MemoryRegion* region = machine_->memory_manager()->Map(io_resource.base, io_resource.length,
      nullptr, kMemoryTypeDevice, io_resource.name);

    mmio_handlers_.push_back(new IoHandler {
      .io_resource = io_resource,
      .device = device,
      .memory_region = region
    });
  }

  MV_LOG("%s register %s 0x%08lx-0x%08lx", device->name(),
    io_resource.type == kIoResourceTypeMmio ? "mmio" : "pio",
    io_resource.base, io_resource.base + io_resource.length - 1);
}

void DeviceManager::UnregisterIoHandler(Device* device, const IoResource& io_resource) {
  if (io_resource.type == kIoResourceTypePio) {
    for (auto it = pio_handlers_.begin(); it != pio_handlers_.end(); it++) {
      if ((*it)->device == device && (*it)->io_resource.base == io_resource.base) {
        delete *it;
        pio_handlers_.erase(it);
        break;
      }
    }
  } else if (io_resource.type == kIoResourceTypeMmio) {
    for (auto it = mmio_handlers_.begin(); it != mmio_handlers_.end(); it++) {
      if ((*it)->device == device && (*it)->io_resource.base == io_resource.base) {
        delete *it;
        mmio_handlers_.erase(it);
        break;
      }
    }
  }
}


/* IO ports may overlap like MMIO addresses.
 * Use para-virtual drivers instead of IO operations to improve performance.
 */
void DeviceManager::HandleIo(uint16_t port, uint8_t* data, uint16_t size, int is_write, uint32_t count) {
  int found = 0, it_count = 0;
  std::deque<IoHandler*>::iterator it;
  for (it = pio_handlers_.begin(); it != pio_handlers_.end(); it++, it_count++) {
    auto &resource = (*it)->io_resource;
    if (port >= resource.base && port < resource.base + resource.length) {
      Device* device = (*it)->device;
      uint8_t* ptr = data;
      for (uint32_t i = 0; i < count; i++) {
        if (is_write) {
          device->Write(resource, port - resource.base, ptr, size);
        } else {
          device->Read(resource, port - resource.base, ptr, size);
        }
        ptr += size;
      }
      ++found;
      if (it_count > 2) {
        pio_handlers_.push_front(*it);
        pio_handlers_.erase(it);
        --it;
      }

      // if (port != 0x402) {
      //   MV_LOG("%s handle io %s port: 0x%x size: %x data: %x count: %d", device->name(),
      //     is_write ? "out" : "in", port, size, *(uint64_t*)data, count);
      // }
    }
  }

  if (!found) {
    /* Accessing invalid port always returns error */
    memset(data, 0xFF, size);
    /* Not allowed unhandled IO for debugging */
    MV_PANIC("unhandled io %s port: 0x%x size: %x data: %016lx count: %d",
      is_write ? "out" : "in", port, size, *(uint64_t*)data, count);
  }
}


/* Use for loop to find MMIO handlers is stupid, unless we are sure addresses not overlapped.
 * But moving the handler to the front works great for now
 */
void DeviceManager::HandleMmio(uint64_t base, uint8_t* data, uint16_t size, int is_write) {
  std::deque<IoHandler*>::iterator it;
  int it_count = 0;
  uint8_t *ptr = data;
  for (it = mmio_handlers_.begin(); it != mmio_handlers_.end(); it++, it_count++) {
    auto &resource = (*it)->io_resource;
    if (base >= resource.base && base < resource.base + resource.length) {
      Device* device = (*it)->device;

      if (is_write) {
        device->Write(resource, base - resource.base, ptr, size);
      } else {
        device->Read(resource, base - resource.base, ptr, size);
      }
      ptr += size;
      if (it_count > 2) {
        mmio_handlers_.push_front(*it);
        mmio_handlers_.erase(it);
        --it;
      }

      // if (base < 0xa0000 || base >= 0xc0000) {
      //   MV_LOG("%s handle mmio %s addr: 0x%x size: %x data: %x", device->name(),
      //     is_write ? "out" : "in", base, size, *(uint64_t*)data);
      // }
      return;
    }
  }
  MV_PANIC("unhandled mmio %s base: 0x%016lx size: %x data: %016lx",
    is_write ? "write" : "read", base, size, *(uint64_t*)data);
}

/* Get the host memory address of a guest physical address */
void* DeviceManager::TranslateGuestMemory(uint64_t gpa) {
  auto memory_manger = machine_->memory_manager();
  void* host = memory_manger->GuestToHostAddress(gpa);
  return host;
}

/* Maybe we should have an IRQ manager or just let KVM do this? */
void DeviceManager::SetIrq(uint32_t irq, uint32_t level) {
  machine_->Interrupt(irq, level);
}
