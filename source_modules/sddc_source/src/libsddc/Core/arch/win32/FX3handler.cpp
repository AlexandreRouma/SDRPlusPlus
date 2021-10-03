//
// FX3handler.cpp 
// 2020 10 12  Oscar Steila ik1xpv
// loading arm code.img from resource by Howard Su and Hayati Ayguen
// This module was previous named:
// openFX3.cpp MIT License Copyright (c) 2016 Booya Corp.
// booyasdr@gmail.com, http://booyasdr.sf.net
// modified 2017 11 30 ik1xpv@gmail.com, http://www.steila.com/blog
// 
#include <windows.h>
#include "FX3handler.h"
#include "./CyAPI/CyAPI.h"
#include "./CyAPI/cyioctl.h"
#define RES_BIN_FIRMWARE                2000


fx3class* CreateUsbHandler()
{
	return new fx3handler();
}

fx3handler::fx3handler():
	fx3dev (nullptr),
	Fx3IsOn (false)
{

}


fx3handler::~fx3handler() // reset USB device and exit
{
	DbgPrintf("\r\n~fx3handler\r\n");
	Close();
}

bool fx3handler::GetFx3Device() { 
	bool r = false;
	if (fx3dev == nullptr) return r; // no device
	int n = fx3dev->DeviceCount();
	if (n == 0)  return r;   // no one

    // Walk through all devices looking for VENDOR_ID/STREAMER_ID	
	for (int i = 0; i <= n; i++) {
		fx3dev->Open(i); // go down the list of devices to find our device
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
	}
	if (r == false)
		fx3dev->Close();
	return r;
}

bool fx3handler::GetFx3DeviceStreamer(void) {   // open class 
	bool r = false;
	if (fx3dev == NULL) return r;
	int n = fx3dev->DeviceCount();
	// Walk through all devices looking for VENDOR_ID/STREAMER_ID
	if (n == 0) return r; 
	// go down the list of devices to find STREAMER_ID device
	for (int i = 0; i <= n; i++) {
		fx3dev->Open(i);
		if ((fx3dev->VendorID == VENDOR_ID) && (fx3dev->ProductID == STREAMER_ID))
		{
			r = true;
			break;
		}
	}
	if (r == false)
		fx3dev->Close();
	return r;
}

bool  fx3handler::Open(uint8_t* fw_data, uint32_t fw_size) {
	bool r = false;
	fx3dev = new CCyFX3Device;              // instantiate the device
	if (fx3dev == nullptr) return r;        // return if failed
	int n = fx3dev->DeviceCount();          
	if (n == 0) return r;					// return if no devices connected
	if (!GetFx3Device()) return r;          // NO FX3 device connected

#ifdef _DEBUG
	if (!fx3dev->IsBootLoaderRunning()) { // if not bootloader device
		Control(RESETFX3);          // reset the fx3 firmware via CyU3PDeviceReset(false)
		DbgPrintf("DEBUG - Reset Firmware\n");
		Sleep(300);
		fx3dev->Close();            // close class
		delete fx3dev;              // destroy class
		Sleep(300);
		fx3dev = new CCyFX3Device;  // create class
		GetFx3Device();             // open class
	}
#endif

	FX3_FWDWNLOAD_ERROR_CODE dlf = SUCCESS;
	if (fx3dev->IsBootLoaderRunning())
	{
		dlf = fx3dev->DownloadFwToRam(fw_data, fw_size);
		Sleep(500); // wait for download to finish
	}

	if (dlf != 0)
	{
		DbgPrintf("MISSING/OLD FIRMWARE\n");
		return false;
	}
	int x = 0;
	int maxretry = 30;
	CCyFX3Device* expdev = nullptr;
	while (x++ < maxretry) // wait new firmware setup
	{
		bool r = false;
		expdev = new CCyFX3Device;              // instantiate the device
		if (expdev != NULL)
			int n = expdev->DeviceCount();      
		if (n > 0)
		{
			expdev->Open(0);
			// go down the list of devices to find our device
			for (int i = 1; i <= n; i++)
			{
				if ((expdev->VendorID == VENDOR_ID) && (expdev->ProductID == STREAMER_ID))
				{
					x = maxretry;	//got it exit
				}
			}
		}
		expdev->Close();            // close class
		delete expdev;              // destroy class
	}
	GetFx3DeviceStreamer();         // open class with new ram firmware
	if (!fx3dev->IsOpen()) {
		DbgPrintf("Failed to open device\n");
		return r;
	}
	EndPt = fx3dev->BulkInEndPt;
	if (!EndPt) {
		DbgPrintf("No Bulk In end point\n");
		return r;      // init failed
	}

	long pktSize = EndPt->MaxPktSize;
	EndPt->SetXferSize(transferSize);
	long ppx = transferSize / pktSize;
	DbgPrintf("buffer transferSize = %d. packet size = %ld. packets per transfer = %ld\n"
		, transferSize, pktSize, ppx);

	uint8_t data[4];
	GetHardwareInfo((uint32_t*)&data);

	if (data[1] != FIRMWARE_VER_MAJOR ||
		data[2] != FIRMWARE_VER_MINOR)
	{
		DbgPrintf("Firmware version mismatch %d.%d != %d.%d (actual)\n", FIRMWARE_VER_MAJOR, FIRMWARE_VER_MINOR, data[1], data[2]);
		Control(RESETFX3);
		return false;
	}

	Fx3IsOn = true;
	return Fx3IsOn;          // init success
}


using namespace std;

bool fx3handler::Control(FX3Command command, UINT8 data) { // firmware control BBRF
	long lgt = 1;

	fx3dev->ControlEndPt->ReqCode = command;
	fx3dev->ControlEndPt->Value = (USHORT)0;
	fx3dev->ControlEndPt->Index = (USHORT)0;
	bool r = fx3dev->ControlEndPt->Write(&data, lgt);
	DbgPrintf("FX3FWControl %x .%x %x\n", r, command, data);
	if (r == false)
	{
		Close();
	}
	return r;
}

bool fx3handler::Control(FX3Command command, UINT32 data) { // firmware control BBRF
	long lgt = 4;

	fx3dev->ControlEndPt->ReqCode = command;
	fx3dev->ControlEndPt->Value = (USHORT)0;
	fx3dev->ControlEndPt->Index = (USHORT)0;
	bool r = fx3dev->ControlEndPt->Write((PUCHAR)&data, lgt);
	DbgPrintf("FX3FWControl %x .%x %x\n", r, command, data);
	if (r == false)
	{
		Close();
	}
	return r;
}

bool fx3handler::Control(FX3Command command, UINT64 data) { // firmware control BBRF
	long lgt = 8;

	fx3dev->ControlEndPt->ReqCode = command;
	fx3dev->ControlEndPt->Value = (USHORT)0;
	fx3dev->ControlEndPt->Index = (USHORT)0;
	bool r = fx3dev->ControlEndPt->Write((PUCHAR)&data, lgt);
	DbgPrintf("FX3FWControl %x .%x %llx\n", r, command, data);
	if (r == false)
	{
		Close();
	}
	return r;
}


bool fx3handler::SetArgument(UINT16 index, UINT16 value) { // firmware control BBRF
	long lgt = 1;
	uint8_t data = 0;

	fx3dev->ControlEndPt->ReqCode = SETARGFX3;
	fx3dev->ControlEndPt->Value = (USHORT)value;
	fx3dev->ControlEndPt->Index = (USHORT)index;
	bool r = fx3dev->ControlEndPt->Write((PUCHAR)&data, lgt);
	DbgPrintf("SetArgument %x .%x (%x, %x)\n", r, SETARGFX3, index, value);
	if (r == false)
	{
		Close();
	}
	return r;
}

bool fx3handler::GetHardwareInfo(UINT32* data) { // firmware control BBRF
	long lgt = 4;

	fx3dev->ControlEndPt->ReqCode = TESTFX3;
#ifdef _DEBUG
	fx3dev->ControlEndPt->Value = (USHORT) 1;
#else
	fx3dev->ControlEndPt->Value = (USHORT) 0;
#endif
	fx3dev->ControlEndPt->Index = (USHORT)0;
	bool r = fx3dev->ControlEndPt->Read((PUCHAR)data, lgt);
	DbgPrintf("GetHardwareInfo %x .%x %x\n", r, TESTFX3, *data);
	if (r == false)
	{
		Close();
	}
	return r;

}

bool fx3handler::ReadDebugTrace(uint8_t* pdata, uint8_t len)
{
	long lgt = len;
	bool r;
	fx3dev->ControlEndPt->ReqCode = READINFODEBUG;
	fx3dev->ControlEndPt->Value = (USHORT) pdata[0]; // upstream char
	r = fx3dev->ControlEndPt->Read((PUCHAR)pdata, lgt);
	return r;
}

bool fx3handler::SendI2cbytes(UINT8 i2caddr, UINT8 regaddr, PUINT8 pdata, UINT8 len)
{
	bool r = false;
	LONG lgt = len;
	fx3dev->ControlEndPt->ReqCode = I2CWFX3;
	fx3dev->ControlEndPt->Value = (USHORT)i2caddr;
	fx3dev->ControlEndPt->Index = (USHORT)regaddr;
	Sleep(10);
	r = fx3dev->ControlEndPt->Write(pdata, lgt);
	if (r == false)
		DbgPrintf("\nfx3FWSendI2cbytes 0x%02x regaddr 0x%02x  1data 0x%02x len 0x%02x \n",
			i2caddr, regaddr, *pdata, len);
	return r;
}

bool fx3handler::ReadI2cbytes(UINT8 i2caddr, UINT8 regaddr, PUINT8 pdata, UINT8 len)
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
		printf("fx3FWReadI2cbytes %x : %02x %02x %02x %02x : %02x\n", r, I2CRFX3, i2caddr, regaddr, len, *pdata);
	fx3dev->ControlEndPt->Value = saveValue;
	fx3dev->ControlEndPt->Index = saveIndex;
	return r;
}

bool fx3handler::Close() {
	fx3dev->Close();            // close class
	delete fx3dev;              // destroy class
	Fx3IsOn = false;
	return true;
}

#define BLOCK_TIMEOUT (80) // block 65.536 ms timeout is 80

struct ReadContext
{
	PUCHAR context;
	OVERLAPPED overlap;
	SINGLE_TRANSFER transfer;
	uint8_t* buffer;
	long size;
};

bool fx3handler::BeginDataXfer(UINT8 *buffer, long transferSize, void** context)
{
	ReadContext *readContext = (ReadContext *)(*context);

	if (!EndPt)
		return false;

	if (*context == nullptr)
	{
		// first time call, allocate the context structure
		readContext = new ReadContext;
		*context = readContext;
		memset(&readContext->overlap, 0, sizeof(readContext->overlap));
		readContext->overlap.hEvent = CreateEvent(NULL, false, false, NULL);
	}

	readContext->buffer = buffer;
	readContext->size = transferSize;

	readContext->context = EndPt->BeginDataXfer(readContext->buffer, transferSize, &readContext->overlap);
	if (EndPt->NtStatus || EndPt->UsbdStatus) {// BeginDataXfer failed
		DbgPrintf((char*)"Xfer request rejected. 1 STATUS = %ld %ld\n", EndPt->NtStatus, EndPt->UsbdStatus);
		return false;
	}

	return true;
}

bool fx3handler::FinishDataXfer(void** context)
{
	ReadContext *readContext = (ReadContext *)(*context);

	if (!readContext)
	{
		return nullptr;
	}

	if (!EndPt->WaitForXfer(&readContext->overlap, BLOCK_TIMEOUT)) { // block on transfer
		DbgPrintf("WaitForXfer timeout. NTSTATUS = 0x%08X\n", EndPt->NtStatus);
		EndPt->Abort(); // abort if timeout
		return false;
	}

	auto requested_size = readContext->size;
	if (!EndPt->FinishDataXfer(readContext->buffer, readContext->size, &readContext->overlap, readContext->context)) {
		DbgPrintf("FinishDataXfer Failed. NTSTATUS = 0x%08X\n", EndPt->NtStatus);
		return false;
	}

	if (readContext->size < requested_size)
		DbgPrintf("only read %ld but requested %ld\n",  readContext->size, requested_size);

	return true;
}

void fx3handler::CleanupDataXfer(void** context)
{
	ReadContext *readContext = (ReadContext *)(*context);

	CloseHandle(readContext->overlap.hEvent);
	delete (readContext);
}

#define USB_READ_CONCURRENT 4

void fx3handler::AdcSamplesProcess()
{
	DbgPrintf("AdcSamplesProc thread runs\n");
	int buf_idx;            // queue index
	int read_idx;
	void*		contexts[USB_READ_CONCURRENT];

	memset(contexts, 0, sizeof(contexts));

	// Queue-up the first batch of transfer requests
	for (int n = 0; n < USB_READ_CONCURRENT; n++) {
		auto ptr = inputbuffer->peekWritePtr(n);
		if (!BeginDataXfer((uint8_t*)ptr, transferSize, &contexts[n])) {
			DbgPrintf("Xfer request rejected.\n");
			return;
		}
	}

	read_idx = 0;	// context cycle index
	buf_idx = 0;	// buffer cycle index

	// The infinite xfer loop.
	while (run) {
		if (!FinishDataXfer(&contexts[read_idx])) {
			break;
		}

		inputbuffer->WriteDone();

		// Re-submit this queue element to keep the queue full
		auto ptr = inputbuffer->peekWritePtr(USB_READ_CONCURRENT - 1);
		if (!BeginDataXfer((uint8_t*)ptr, transferSize, &contexts[read_idx])) { // BeginDataXfer failed
			DbgPrintf("Xfer request rejected.\n");
			break;
		}

		buf_idx = (buf_idx + 1) % QUEUE_SIZE;
		read_idx = (read_idx + 1) % USB_READ_CONCURRENT;
	}  // End of the infinite loop

	for (int n = 0; n < USB_READ_CONCURRENT; n++) {
		CleanupDataXfer(&contexts[n]);
	}

	DbgPrintf("AdcSamplesProc thread_exit\n");
	return;  // void *
}

void fx3handler::StartStream(ringbuffer<int16_t>& input, int numofblock)
{
	// Allocate the context and buffers
	inputbuffer = &input;

	// create the thread
	this->numofblock = numofblock;
	run = true;
	adc_samples_thread = new std::thread(
		[this]() {
			this->AdcSamplesProcess();
		}
	);
}

void fx3handler::StopStream()
{
	// set the flag
	run = false;
	adc_samples_thread->join();

	// force exit the thread
	inputbuffer = nullptr;

	delete adc_samples_thread;
}
