// MIT License Copyright (c) 2016 Booya Corp.
// booyasdr@gmail.com, http://booyasdr.sf.net
// modified 2017 11 30 ik1xpv@gmail.com, http://www.steila.com/blog
#include <windows.h>

#include <sys/stat.h>
#include <iostream>
#include <stdio.h>
#include "openfx3.h"

#include <spdlog/spdlog.h>

#define DbgPrintf spdlog::info

using namespace std;

void fx3Control(FX3Command command) { // firmware control
	long lgt = 1;
	UINT8 z = 0; // dummy data = 0
	fx3dev->ControlEndPt->ReqCode = command;
	fx3dev->ControlEndPt->Write(&z, lgt);
}

bool fx3Control(FX3Command command, PUINT8 data) { // firmware control BBRF
	long lgt = 1; // default
	bool r = false;
	switch (command)
	{
	case GPIOFX3:
		fx3dev->ControlEndPt->ReqCode = command;
		fx3dev->ControlEndPt->Value = (USHORT)0;
		fx3dev->ControlEndPt->Index = (USHORT)0;
		lgt = 2;
		DbgPrintf("GPIO %x %x\n", data[0], data[1]);
		r = fx3dev->ControlEndPt->Write(data, lgt);
		break;
	case TESTFX3:
		fx3dev->ControlEndPt->ReqCode = command;
		fx3dev->ControlEndPt->Value = (USHORT)0;
		fx3dev->ControlEndPt->Index = (USHORT)0;
		lgt = 4; // TESTFX3 len
		r = fx3dev->ControlEndPt->Read(data, lgt);
		break;
	default:
		break;
	}
	if (r == false)
	{
		DbgPrintf("WARNING FX3FWControl failed %x .%x %x\n", r, command, *data);
		closeFX3();
	}
	return r;
}


bool fx3SendI2cbytes(UINT8 i2caddr, UINT8 regaddr, PUINT8 pdata, UINT8 len)
{
	bool r = false;
	LONG lgt = len;
	fx3dev->ControlEndPt->ReqCode = I2CWFX3;
	fx3dev->ControlEndPt->Value = (USHORT)i2caddr;
	fx3dev->ControlEndPt->Index = (USHORT)regaddr;
	Sleep(10);
	r = fx3dev->ControlEndPt->Write(pdata, lgt);
	if (r == false)
		DbgPrintf("\nWARNING fx3FWSendI2cbytes i2caddr 0x%02x regaddr 0x%02x  1data 0x%02x len 0x%02x \n",
			i2caddr, regaddr, *pdata, len);
	return r;
}

bool fx3SendI2cbyte(UINT8 i2caddr, UINT8 regaddr, UINT8 data)
{
	return fx3SendI2cbytes(i2caddr, regaddr, &data, 1);
}

bool fx3ReadI2cbytes(UINT8 i2caddr, UINT8 regaddr, PUINT8 pdata, UINT8 len)
{
	bool r = false;
	LONG lgt = len;
	WORD saveValue, saveIndex;
	saveValue = fx3dev->ControlEndPt->Value;
	saveIndex = fx3dev->ControlEndPt->Index;

	fx3dev->ControlEndPt->ReqCode = I2CRFX3;
	fx3dev->ControlEndPt->Value = (USHORT)i2caddr;
	fx3dev->ControlEndPt->Index = (USHORT)regaddr;
	r = fx3dev->ControlEndPt->Read(pdata, lgt);
	if (r == false)
		printf("WARNING fx3FWReadI2cbytes failed %x : %02x %02x %02x %02x : %02x\n", r, I2CRFX3, i2caddr, regaddr, len, *pdata);
	fx3dev->ControlEndPt->Value = saveValue;
	fx3dev->ControlEndPt->Index = saveIndex;
	return r;
}

bool fx3ReadI2cbyte(UINT8 i2caddr, UINT8 regaddr, UINT8 data)
{
	return fx3ReadI2cbytes(i2caddr, regaddr, &data, 1);
}

bool GetFx3Device(void);    // open the device, called in initFX3()

CCyFX3Device        *fx3dev = NULL;
const int			VENDOR_ID = 0x04B4;
const int			STREAMER_ID = 0x00F1;
const int			BOOTLOADER_ID = 0x00F3;
CCyUSBEndPoint		*EndPt;
bool				bHighSpeedDevice;
bool				bSuperSpeedDevice;

bool openFX3() {
	bool r = false;
	fx3dev = new CCyFX3Device;              // instantiate the device
	if (fx3dev == NULL) return 0;           // return if failed
	int n = fx3dev->DeviceCount();          // return if no devices connected
	if (n == 0) {
		DbgPrintf("Device Count = 0, Exit\n");
		return r;
	}
	if (!GetFx3Device()) { DbgPrintf("No streamer or boot device found, Exit\n");  return r; }

	char fwname[] = "rx888.img";  // firmware file
 
	if (!fx3dev->IsBootLoaderRunning()) { // if not bootloader device
		fx3Control(RESETFX3);       // reset the fx3 firmware via CyU3PDeviceReset(false)
		DbgPrintf("Reset Device\n");
		Sleep(300);
		fx3dev->Close();            // close class
		delete fx3dev;              // destroy class
		Sleep(300);
		fx3dev = new CCyFX3Device;  // create class
		GetFx3Device();             // open class
	}
	FX3_FWDWNLOAD_ERROR_CODE dlf = FAILED;
	if (fx3dev->IsBootLoaderRunning()) {
		dlf = fx3dev->DownloadFw(fwname, RAM);
		Sleep(500); // wait for download to finish
	}
	if (dlf == 0) {
		struct stat stbuf;
		stat(fwname, &stbuf);
		char *timestr;
		timestr = ctime(&stbuf.st_mtime);
		DbgPrintf("Loaded NEW FIRMWARE {0} {1}", fwname, timestr);
	}
	else if (dlf != 0)
	{
		DbgPrintf("OLD FIRMWARE\n");
	}

	GetFx3Device();         // open class with new firmware
	if (!fx3dev->IsOpen()) {
		DbgPrintf("Failed to open device\n");
		return r;
	}
	EndPt = fx3dev->BulkInEndPt;
	if (!EndPt) {
		DbgPrintf("No Bulk In end point\n");
		return r;      // init failed
	}
	r = true;
	return r; // init success

}

bool closeFX3() {
	bool r = false;
	fx3dev->Close();            // close class
	delete fx3dev;              // destroy class
	Sleep(300);
	return r;
}

bool GetFx3Device() {   // open class

	bool r = false;
	if (fx3dev == NULL) return r;
	int n = fx3dev->DeviceCount();
	// Walk through all devices looking for VENDOR_ID/STREAMER_ID
	if (n == 0) { DbgPrintf("Device Count = 0, Exit\n"); return r; }

	fx3dev->Open(0);
	// go down the list of devices to find our device
	for (int i = 1; i <= n; i++) {
		cout << hex << fx3dev->VendorID << " "
			<< hex << fx3dev->ProductID << " " << fx3dev->FriendlyName << '\n';
		if ((fx3dev->VendorID == VENDOR_ID) && (fx3dev->ProductID == STREAMER_ID))
		{
			r = true;
			break;
		}

		if ((fx3dev->VendorID == VENDOR_ID) && (fx3dev->ProductID == BOOTLOADER_ID))
		{
			r = true;
			break;
		}

		fx3dev->Open(i);
	}
	if (r == false)
		fx3dev->Close();
	return r;
}

bool fx3Check()
{
	return (fx3dev != NULL);
}
