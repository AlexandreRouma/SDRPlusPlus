// MIT License Copyright (c) 2016 Booya Corp.
// booyasdr@gmail.com, http://booyasdr.sf.net
// modified 2017 11 30 ik1xpv@gmail.com, http://www.steila.com/blog

#ifndef FX3DEV_H
#define FX3DEV_H

#include <windows.h>
#include "CyAPI.h"

#define PUINT8 UINT8*

bool openFX3(void);
extern CCyFX3Device     *fx3dev;
extern CCyUSBEndPoint	*EndPt;
bool closeFX3(void);

enum FX3Command {
	STARTFX3 = 0xaa,
	STOPFX3 = 0xab,
	TESTFX3 = 0xac,
	RESETFX3 = 0xcc,
	PAUSEFX3 = 0xdd,
	GPIOFX3 = 0xbc,
	I2CWFX3 = 0xba,
	I2CRFX3 = 0xbe
};
void fx3Control(FX3Command command);
bool fx3Control(FX3Command command, PUINT8 data);
bool fx3SendI2cbytes(UINT8 i2caddr, UINT8 regaddr, PUINT8 pdata, UINT8 len);
bool fx3SendI2cbyte(UINT8 i2caddr, UINT8 regaddr, UINT8 pdata);
bool fx3ReadI2cbytes(UINT8 i2caddr, UINT8 regaddr, PUINT8 pdata, UINT8 len);
bool fx3ReadI2cbyte(UINT8 i2caddr, UINT8 regaddr, UINT8 pdata);
bool fx3Check();

#endif
