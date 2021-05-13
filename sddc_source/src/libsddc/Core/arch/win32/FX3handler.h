#ifndef FX3HANDLER_H
#define FX3HANDLER_H

//
// FX3handler.cpp 
// 2020 10 12  Oscar Steila ik1xpv
// loading arm code.img from resource by Howard Su and Hayati Ayguen
// This module was previous named:openFX3.cpp
// MIT License Copyright (c) 2016 Booya Corp.
// booyasdr@gmail.com, http://booyasdr.sf.net
// modified 2017 11 30 ik1xpv@gmail.com, http://www.steila.com/blog
// 

#include <sys/stat.h>
#include <iostream>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "sddc_config.h"

#include "dsp/ringbuffer.h"

#define	VENDOR_ID     (0x04B4)
#define	STREAMER_ID   (0x00F1)
#define	BOOTLOADER_ID (0x00F3)

#include "FX3Class.h"

class CCyFX3Device;
class CCyUSBEndPoint;

class fx3handler : public fx3class
{
public:
	fx3handler();
	virtual ~fx3handler(void);
	bool Open(uint8_t* fw_data, uint32_t fw_size);
	bool IsOn() { return Fx3IsOn; }
	bool Control(FX3Command command, uint8_t data);
	bool Control(FX3Command command, uint32_t data = 0);
	bool Control(FX3Command command, uint64_t data);
	bool SetArgument(uint16_t index, uint16_t value);
	bool GetHardwareInfo(uint32_t* data);
	bool ReadDebugTrace(uint8_t* pdata, uint8_t len);
	void StartStream(ringbuffer<int16_t>& input, int numofblock);
	void StopStream();

private:
	bool SendI2cbytes(uint8_t i2caddr, uint8_t regaddr, uint8_t* pdata, uint8_t len);
	bool ReadI2cbytes(uint8_t i2caddr, uint8_t regaddr, uint8_t* pdata, uint8_t len);

	bool BeginDataXfer(uint8_t *buffer, long transferSize, void** context);
	bool FinishDataXfer(void** context);
	void CleanupDataXfer(void** context);

	CCyFX3Device* fx3dev;
	CCyUSBEndPoint* EndPt;

    std::thread *adc_samples_thread;

	bool GetFx3Device();
	bool GetFx3DeviceStreamer();
	bool Fx3IsOn;
	bool Close(void);
	void AdcSamplesProcess();

	ringbuffer<int16_t> *inputbuffer;
	int numofblock;
	bool run;
};


#endif // FX3HANDLER_H
