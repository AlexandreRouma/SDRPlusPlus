#ifndef FX3CLASS_H
#define FX3CLASS_H

//
// FX3handler.cpp 
// 2020 10 12  Oscar Steila ik1xpv
// loading arm code.img from resource by Howard Su and Hayati Ayguen
// This module was previous named:openFX3.cpp
// MIT License Copyright (c) 2016 Booya Corp.
// booyasdr@gmail.com, http://booyasdr.sf.net
// modified 2017 11 30 ik1xpv@gmail.com, http://www.steila.com/blog
// 

#include <stdint.h>
#include <functional>
#include "../Interface.h"
#include "dsp/ringbuffer.h"

class fx3class
{
public:
	virtual ~fx3class(void) {}
	virtual bool Open(uint8_t* fw_data, uint32_t fw_size) = 0;
	virtual bool Control(FX3Command command, uint8_t data = 0) = 0;
	virtual bool Control(FX3Command command, uint32_t data) = 0;
	virtual bool Control(FX3Command command, uint64_t data) = 0;
	virtual bool SetArgument(uint16_t index, uint16_t value) = 0;
	virtual bool GetHardwareInfo(uint32_t* data) = 0;
	virtual bool ReadDebugTrace(uint8_t* pdata, uint8_t len) = 0;
	virtual void StartStream(ringbuffer<int16_t>& input, int numofblock) = 0;
	virtual void StopStream() = 0;

};

extern "C" fx3class* CreateUsbHandler();

#endif // FX3CLASS_H
