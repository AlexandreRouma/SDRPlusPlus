#include <string.h>

#include "FX3handler.h"
#include "usb_device.h"

fx3class* CreateUsbHandler()
{
	return new fx3handler();
}

fx3handler::fx3handler()
{
}

fx3handler::~fx3handler()
{
}

bool fx3handler::Open(uint8_t* fw_data, uint32_t fw_size)
{
    dev = usb_device_open(0, (const char*)fw_data, fw_size);

    return dev != nullptr;
}

bool fx3handler::Control(FX3Command command, uint8_t data)
{
    return usb_device_control(this->dev, command, 0, 0, (uint8_t *) &data, sizeof(data), 0) == 0;
}

bool fx3handler::Control(FX3Command command, uint32_t data)
{
    return usb_device_control(this->dev, command, 0, 0, (uint8_t *) &data, sizeof(data), 0) == 0;
}

bool fx3handler::Control(FX3Command command, uint64_t data)
{
    return usb_device_control(this->dev, command, 0, 0, (uint8_t *) &data, sizeof(data), 0) == 0;
}

bool fx3handler::SetArgument(uint16_t index, uint16_t value)
{
    uint8_t data = 0;
    return usb_device_control(this->dev, SETARGFX3, value, index, (uint8_t *) &data, sizeof(data), 0) == 0;
}

bool fx3handler::GetHardwareInfo(uint32_t* data)
{
    return usb_device_control(this->dev, TESTFX3, 0, 0, (uint8_t *) data, sizeof(*data), 1) == 0;
}

void fx3handler::StartStream(ringbuffer<int16_t>& input, int numofblock)
{
    inputbuffer = &input;
    auto readsize = input.getWriteCount() * sizeof(uint16_t);
    stream = streaming_open_async(this->dev, readsize, numofblock, PacketRead, this);

    // Start background thread to poll the events
    run = true;
    poll_thread = std::thread(
        [this]() {
            while(run)
            {
                usb_device_handle_events(this->dev);
            }
        });

    if (stream)
    {
        streaming_start(stream);
    }
}

void fx3handler::StopStream()
{
    run = false;
    poll_thread.join();

    streaming_stop(stream);
    streaming_close(stream);
}

void fx3handler::PacketRead(uint32_t data_size, uint8_t *data, void *context)
{
    fx3handler *handler = (fx3handler*)context;

    auto *ptr = handler->inputbuffer->getWritePtr();
    memcpy(ptr, data, data_size);
    handler->inputbuffer->WriteDone();
}

bool fx3handler::ReadDebugTrace(uint8_t* pdata, uint8_t len)
{
	return true;
}
