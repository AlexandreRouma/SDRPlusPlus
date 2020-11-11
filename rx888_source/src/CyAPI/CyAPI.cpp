/*
 ## Cypress CyAPI C++ library source file (CyAPI.cpp)
 ## =======================================================
 ##
 ##  Copyright Cypress Semiconductor Corporation, 2009-2012,
 ##  All Rights Reserved
 ##  UNPUBLISHED, LICENSED SOFTWARE.
 ##
 ##  CONFIDENTIAL AND PROPRIETARY INFORMATION
 ##  WHICH IS THE PROPERTY OF CYPRESS.
 ##
 ##  Use of this file is governed
 ##  by the license agreement included in the file
 ##
 ##  <install>/license/license.rtf
 ##
 ##  where <install> is the Cypress software
 ##  install root directory path.
 ##
 ## =======================================================
*/
#ifndef WINVER
#define WINVER 0x0500
#endif

#include <objbase.h>
#include <setupapi.h>
#include <stdio.h>
#include "cyioctl.h"
#include "CyAPI.h"
#include "UsbdStatus.h"
#include "dbt.h"

const char API_VERSION[8] = "1.3.2.0";

UINT SPI_FLASH_PAGE_SIZE_IN_BYTE = 256;
UINT SPI_FLASH_SECTOR_SIZE_IN_BYTE = (64 * 1024);
UINT CYWB_BL_MAX_BUFFER_SIZE_WHEN_USING_EP0_TRANSPORT = BUFSIZE_UPORT; //(8 * 512); // 4KB

////////////////////////////////////////////////////////////////////////////////
//
// The USB Device Class
//
////////////////////////////////////////////////////////////////////////////////

// Constructor
CCyUSBDevice::CCyUSBDevice(HANDLE hnd, GUID guid, BOOL bOpen)
{
    hDevice = INVALID_HANDLE_VALUE;
    hDevNotification = 0;
    hHndNotification = 0;

    DevNum = 0;
    CfgNum = 0;
    IntfcNum  = 0;     // The current selected interface's bInterfaceNumber
    IntfcIndex = 0;    // The entry in the Config's interfaces table matching to IntfcNum and AltSetting

    Devices = 0;
    Interfaces = 0;
    AltInterfaces = 0;
    Configs = 0;

    DrvGuid = guid;
    hWnd = hnd;
    LastError  = 0;

    EndPoints         = NULL;
    ControlEndPt      = NULL;
    IsocInEndPt       = NULL;
    IsocOutEndPt      = NULL;
    BulkInEndPt       = NULL;
    BulkOutEndPt      = NULL;
    InterruptInEndPt  = NULL;
    InterruptOutEndPt = NULL;

    USBCfgs[0] = NULL;
    USBCfgs[1] = NULL;
    USBConfigDescriptors[0] = NULL;
    USBConfigDescriptors[1] = NULL;
    pUsbBosDescriptor = NULL;
    UsbBos = NULL;

    ZeroMemory(Manufacturer, USB_STRING_MAXLEN);
    ZeroMemory(Product, USB_STRING_MAXLEN);
    ZeroMemory(SerialNumber, USB_STRING_MAXLEN);
    ZeroMemory(DeviceName, USB_STRING_MAXLEN);
    ZeroMemory(FriendlyName, USB_STRING_MAXLEN);


    if (hnd) RegisterForPnpEvents(hnd);

    if (bOpen) Open(DevNum);
}

//______________________________________________________________________________

CCyUSBDevice::~CCyUSBDevice(void)
{

    Close();

    if (hDevNotification) {
        if (! UnregisterDeviceNotification(hDevNotification))
            throw "Failed to close the device notification handle.";

        hDevNotification = 0;
    }

}


//______________________________________________________________________________
//
// It is expected that the device will only expose a single Config via the
// Windows USB HCD stack.  The driver doesn't even expose an IOCTL for setting
// the configuration.
//
// This method just initializes a bunch of important variables, most important
// of which is the call to SetAltIntfcParams( ).

void  CCyUSBDevice::SetConfig(UCHAR cfg){

    if (!USBCfgs[0]) return;

    CfgNum = 0;
    if ((USBCfgs[0]) && (USBCfgs[0]->iConfiguration == cfg)) CfgNum = cfg;
    if ((USBCfgs[1]) && (USBCfgs[1]->iConfiguration == cfg)) CfgNum = cfg;

    ConfigValue   = USBCfgs[CfgNum]->bConfigurationValue;
    ConfigAttrib  = USBCfgs[CfgNum]->bmAttributes;
    MaxPower      = USBCfgs[CfgNum]->MaxPower;
    Interfaces    = USBCfgs[CfgNum]->bNumInterfaces;
    AltInterfaces = USBCfgs[CfgNum]->AltInterfaces;
    IntfcNum      = USBCfgs[CfgNum]->Interfaces[0]->bInterfaceNumber;

    UCHAR a  = AltIntfc();  // Get the current alt setting from the device
    SetAltIntfcParams(a);   // Initializes endpts, IntfcIndex, etc. without actually setting the AltInterface

    if (USBCfgs[CfgNum]->Interfaces[IntfcIndex]) {
        IntfcClass = USBCfgs[CfgNum]->Interfaces[IntfcIndex]->bInterfaceClass;
        IntfcSubClass = USBCfgs[CfgNum]->Interfaces[IntfcIndex]->bInterfaceSubClass;
        IntfcProtocol = USBCfgs[CfgNum]->Interfaces[IntfcIndex]->bInterfaceProtocol;
    }


}

//______________________________________________________________________________


UCHAR   CCyUSBDevice::ConfigCount(void){

    if (hDevice == INVALID_HANDLE_VALUE) return (UCHAR) NULL;

    return Configs;

}

//______________________________________________________________________________

UCHAR   CCyUSBDevice::IntfcCount(void){

    if (hDevice == INVALID_HANDLE_VALUE) return (UCHAR) NULL;

    return Interfaces;

}

//______________________________________________________________________________
// This method is not currently exposed because the driver does not support
// selection of the interface.  This is because the Windows USB hub driver
// creates a new device node for each interface.
/*
void   CCyUSBDevice::SetInterface(UCHAR i){
if (i < NumInterfaces) IntfcNum = i;

if (Configs[CfgNum]) {
for (int j=0; j < Configs[CfgNum]->bAltInterfaces; j++)
if ( (Configs[CfgNum]->Interfaces[j]->bInterfaceNumber == IntfcNum) &&
(Configs[CfgNum]->Interfaces[j]->bAlternateSetting == AltSetting) ) {


// TODO: Select the specified interface via the driver



IntfcIndex = j;
return;
}
}

}

*/
//______________________________________________________________________________

UCHAR   CCyUSBDevice::AltIntfc(void){

    UCHAR alt;
    if (IoControl(IOCTL_ADAPT_GET_ALT_INTERFACE_SETTING, &alt, 1))
        return alt;
    else
        return 0xFF;
}


//______________________________________________________________________________

void   CCyUSBDevice::SetAltIntfcParams(UCHAR alt){

    // Find match of IntfcNum and alt in table of interfaces
    if (USBCfgs[CfgNum]) {
        for (int j=0; j < USBCfgs[CfgNum]->AltInterfaces; j++)
            if ( //(USBCfgs[CfgNum]->Interfaces[j]->bInterfaceNumber == IntfcNum) &&
                (USBCfgs[CfgNum]->Interfaces[j]->bAlternateSetting == alt) ) {

                    IntfcIndex = j;
                    IntfcClass = USBCfgs[CfgNum]->Interfaces[j]->bInterfaceClass;
                    IntfcSubClass = USBCfgs[CfgNum]->Interfaces[j]->bInterfaceSubClass;;
                    IntfcProtocol = USBCfgs[CfgNum]->Interfaces[j]->bInterfaceProtocol;;

                    SetEndPointPtrs();
                    return;
            }
    }

}

//______________________________________________________________________________

bool CCyUSBDevice::SetAltIntfc(UCHAR alt){

    bool bSuccess = false;

    if (alt == AltIntfc()) return true;

    // Find match of IntfcNum and alt in table of interfaces
    if (USBCfgs[CfgNum]) {
        for (int j=0; j < USBCfgs[CfgNum]->AltInterfaces; j++)
            if ( //(USBCfgs[CfgNum]->Interfaces[j]->bInterfaceNumber == IntfcNum) &&
                (USBCfgs[CfgNum]->Interfaces[j]->bAlternateSetting == alt) ) {

                    IntfcIndex = j;

                    // Actually change to the alt interface, calling the driver
                    bSuccess = IoControl(IOCTL_ADAPT_SELECT_INTERFACE, &alt, 1L);

                    IntfcClass = USBCfgs[CfgNum]->Interfaces[j]->bInterfaceClass;
                    IntfcSubClass = USBCfgs[CfgNum]->Interfaces[j]->bInterfaceSubClass;;
                    IntfcProtocol = USBCfgs[CfgNum]->Interfaces[j]->bInterfaceProtocol;;

                    SetEndPointPtrs();
                    return bSuccess;
            }
    }

    return bSuccess;

}

//______________________________________________________________________________
// Returns the total number of alternate interfaces for the current interface

UCHAR  CCyUSBDevice::AltIntfcCount(void) {

    if (USBCfgs[CfgNum])
        return USBCfgs[CfgNum]->Interfaces[IntfcIndex]->bAltSettings - 1;

    return 0;  // The primary interface is not considered an Alt interface
    // even though it does have an AltSetting value (0)

}


//______________________________________________________________________________

UCHAR  CCyUSBDevice::EndPointCount(void) {

    if (hDevice == INVALID_HANDLE_VALUE) return (UCHAR) NULL;

    if (USBCfgs[CfgNum])
        return USBCfgs[CfgNum]->Interfaces[IntfcIndex]->bNumEndpoints+1; // Include EndPt0

    return (UCHAR) NULL;
}


//______________________________________________________________________________

void  CCyUSBDevice::GetDeviceName(void) {

    ZeroMemory(DeviceName, USB_STRING_MAXLEN);

    if (hDevice == INVALID_HANDLE_VALUE) return;

    IoControl(IOCTL_ADAPT_GET_DEVICE_NAME, (PUCHAR)DeviceName, USB_STRING_MAXLEN);
}

//______________________________________________________________________________

void  CCyUSBDevice::GetFriendlyName(void) {

    ZeroMemory(FriendlyName, USB_STRING_MAXLEN);

    if (hDevice == INVALID_HANDLE_VALUE) return;

    IoControl(IOCTL_ADAPT_GET_FRIENDLY_NAME, (PUCHAR)FriendlyName, USB_STRING_MAXLEN);
}

//______________________________________________________________________________

void  CCyUSBDevice::GetUSBAddress(void) {

    if (hDevice == INVALID_HANDLE_VALUE) {
        USBAddress = 0;
        return;
    }

    bool bRetVal = IoControl(IOCTL_ADAPT_GET_ADDRESS, &USBAddress, 1L);
    if (!bRetVal || BytesXfered == 0) USBAddress = 0;
}

//______________________________________________________________________________

void  CCyUSBDevice::GetDriverVer(void) {

    if (hDevice == INVALID_HANDLE_VALUE) {
        DriverVersion = (ULONG) NULL;
        return;
    }

    bool bRetVal = IoControl(IOCTL_ADAPT_GET_DRIVER_VERSION,  (PUCHAR) &DriverVersion, sizeof(ULONG));
    if (!bRetVal || BytesXfered == 0) DriverVersion = 0;
}

//______________________________________________________________________________

void  CCyUSBDevice::GetUSBDIVer(void) {

    if (hDevice == INVALID_HANDLE_VALUE) {
        USBDIVersion = (ULONG)  NULL;
        return;
    }

    bool bRetVal = IoControl(IOCTL_ADAPT_GET_USBDI_VERSION, (PUCHAR) &USBDIVersion, sizeof(ULONG));
    if (!bRetVal || BytesXfered == 0) USBDIVersion = 0;
}

//______________________________________________________________________________

UCHAR  CCyUSBDevice::DeviceCount(void){

    //SP_DEVINFO_DATA devInfoData;
    SP_DEVICE_INTERFACE_DATA  devInterfaceData;

    //Open a handle to the plug and play dev node.
    //SetupDiGetClassDevs() returns a device information set that contains info on all
    // installed devices of a specified class which are present.
    HDEVINFO hwDeviceInfo = SetupDiGetClassDevs ( (LPGUID) &DrvGuid,
        NULL,
        NULL,
        DIGCF_PRESENT|DIGCF_INTERFACEDEVICE);

    Devices = 0;

    if (hwDeviceInfo != INVALID_HANDLE_VALUE) {

        //SetupDiEnumDeviceInterfaces() returns information about device interfaces
        // exposed by one or more devices. Each call returns information about one interface.
        //The routine can be called repeatedly to get information about several interfaces
        // exposed by one or more devices.

        devInterfaceData.cbSize = sizeof(devInterfaceData);

        // Count the number of devices
        int i=0;
        Devices = 0;
        bool bDone = false;

        while (!bDone) {
            BOOL bRetVal = SetupDiEnumDeviceInterfaces (hwDeviceInfo, 0, (LPGUID) &DrvGuid,
                i, &devInterfaceData);

            if (bRetVal)
                Devices++;
            else {
                DWORD dwLastError = GetLastError();
                if (dwLastError == ERROR_NO_MORE_ITEMS) bDone = TRUE;
            }

            i++;
        }

        SetupDiDestroyDeviceInfoList(hwDeviceInfo);
    }

    return Devices;
}

//______________________________________________________________________________

bool CCyUSBDevice::CreateHandle(UCHAR dev){

    Devices = DeviceCount();

    if (!Devices) return false;

    if (dev > (Devices - 1)) return false; //dev = Devices-1;

    SP_DEVINFO_DATA devInfoData;
    SP_DEVICE_INTERFACE_DATA  devInterfaceData;
    PSP_INTERFACE_DEVICE_DETAIL_DATA functionClassDeviceData;
    SP_INTERFACE_DEVICE_DETAIL_DATA tmpInterfaceDeviceDetailData;

    ULONG requiredLength = 0;
    int deviceNumber = dev;

    HANDLE hFile;

    //Open a handle to the plug and play dev node.
    //SetupDiGetClassDevs() returns a device information set that contains info on all
    // installed devices of a specified class which are present.
    HDEVINFO hwDeviceInfo = SetupDiGetClassDevs ( (LPGUID) &DrvGuid,
        NULL,
        NULL,
        DIGCF_PRESENT|DIGCF_INTERFACEDEVICE);

    if (hwDeviceInfo != INVALID_HANDLE_VALUE) {

        //SetupDiEnumDeviceInterfaces() returns information about device interfaces
        // exposed by one or more devices. Each call returns information about one interface.
        //The routine can be called repeatedly to get information about several interfaces
        // exposed by one or more devices.

        devInterfaceData.cbSize = sizeof(devInterfaceData);

        if (SetupDiEnumDeviceInterfaces ( hwDeviceInfo, 0, (LPGUID) &DrvGuid,
            deviceNumber, &devInterfaceData)) {
                //Allocate a function class device data structure to receive the goods about this
                // particular device.
                SetupDiGetInterfaceDeviceDetail ( hwDeviceInfo, &devInterfaceData, NULL, 0,
                    &requiredLength, NULL);

                ULONG predictedLength = requiredLength;

                functionClassDeviceData = (PSP_INTERFACE_DEVICE_DETAIL_DATA) malloc (predictedLength);
                functionClassDeviceData->cbSize =  sizeof (SP_INTERFACE_DEVICE_DETAIL_DATA);
                devInfoData.cbSize = sizeof(devInfoData);
                //Retrieve the information from Plug and Play */
                if (SetupDiGetInterfaceDeviceDetail (hwDeviceInfo,
                    &devInterfaceData,
                    functionClassDeviceData,
                    predictedLength,
                    &requiredLength,
                    &devInfoData)) {

                        /* NOTE : x64 packing issue ,requiredLength return 5byte size of the (SP_INTERFACE_DEVICE_DETAIL_DATA) and functionClassDeviceData needed sizeof functionClassDeviceData 8byte */
                        int pathLen = requiredLength - (sizeof (tmpInterfaceDeviceDetailData.cbSize)+sizeof (tmpInterfaceDeviceDetailData.DevicePath));
                        //int pathLen = requiredLength - functionClassDeviceData->cbSize;

                        memcpy (DevPath, functionClassDeviceData->DevicePath, pathLen);
                        DevPath[pathLen] = 0;

                        hFile = CreateFile (DevPath,
                            GENERIC_WRITE | GENERIC_READ,
                            FILE_SHARE_WRITE | FILE_SHARE_READ,
                            NULL,
                            OPEN_EXISTING,
                            FILE_FLAG_OVERLAPPED,
                            NULL);

                        DWORD errCode =  GetLastError();
                        free(functionClassDeviceData);
                        SetupDiDestroyDeviceInfoList(hwDeviceInfo);

                        if (hFile == INVALID_HANDLE_VALUE)
                        {
                            LastError = GetLastError();
                            return false;
                        }
                        // hDevice, DevNum, USBDeviceDescriptor, and Configs are data members
                        hDevice = hFile;

                        return true;
                }

        }

        SetupDiDestroyDeviceInfoList(hwDeviceInfo);

    }

    // Got here by failing at some point

    hDevice = INVALID_HANDLE_VALUE;
    DevNum = 0;
    return false;

}


//______________________________________________________________________________

bool CCyUSBDevice::Open(UCHAR dev){

    // If this CCyUSBDevice object already has the driver open, close it.
    if (hDevice != INVALID_HANDLE_VALUE)
        Close();



    if (CreateHandle(dev)) {
        DevNum = dev;

        // Important call: Gets the device descriptor data from the device
        GetDevDescriptor();

        // Gets the language IDs and selects English, if avail
        SetStringDescrLanguage();

        GetString(Manufacturer,USBDeviceDescriptor.iManufacturer);
        GetString(Product,USBDeviceDescriptor.iProduct);
        GetString(SerialNumber,USBDeviceDescriptor.iSerialNumber);

        // Get BOS descriptor
        if ((BcdUSB & BCDUSBJJMASK) == USB30MAJORVER)
        {
            if(GetInternalBosDescriptor()) // USB3.0 specific descriptor
			{
				try
				{
					UsbBos = new CCyUSBBOS(hDevice,pUsbBosDescriptor);
				}
				catch(char *Str)
				{
					char *Str1 = Str;// just to ignore warning
					MessageBox(NULL,"Please correct firmware BOS descriptor table","Wrong BOS Descriptor",(UINT) NULL);
					Close(); // Close the device handle.
					return false;
				}
			}
        }
        GetUSBAddress();
        GetDeviceName();
        GetFriendlyName();
        GetDriverVer();
        GetUSBDIVer();
        GetSpeed();

        // Gets the config (including interface and endpoint) descriptors from the device
        for (int i=0; i<Configs; i++)
		{
            GetCfgDescriptor(i);
			try
			{
				if (USBConfigDescriptors[i])
				{
					if((BcdUSB & BCDUSBJJMASK) == USB20MAJORVER)
						USBCfgs[i] = new CCyUSBConfig(hDevice,USBConfigDescriptors[i]);
					else
					{
						UCHAR usb30Dummy =1;
						USBCfgs[i] = new CCyUSBConfig(hDevice,USBConfigDescriptors[i],usb30Dummy);
					}
				}
			}
			catch(char *Str)
			{
				char *Str1 = Str; // just to ignore warning
				MessageBox(NULL,"Please correct firmware descriptor table","Wrong Device Configuration",(UINT) NULL);
				Close(); // Close the device handle.
				return false;
			}
        }

        // Manually configure the Control Endpoint (EPT 0)
        ControlEndPt = new CCyControlEndPoint(hDevice, NULL);
        ControlEndPt->DscLen     = 7;
        ControlEndPt->DscType    = 5;
        ControlEndPt->Address    = 0;
        ControlEndPt->Attributes = 0;
        ControlEndPt->MaxPktSize = MaxPacketSize;
        ControlEndPt->Interval   = 0;

        // We succeeded in openning a handle to the device.  But, the device
        // is not returning descriptors properly.  We don't call Close( ) because
        // we want to leave the hDevice intact, giving the user the opportunity
        // to call the Reset( ) method
        if (!USBCfgs[0] || !USBCfgs[0]->Interfaces[0])
            return false;

        // This call sets values for ConfigVal, ConfigAttrib, MaxPower, etc.
        SetConfig(0);

        // If we registered for device notification, also register the handle
        // This is necessary in order to receive notification of removal (allowing us to close)
        // before the device re-connects (for such things as our ReConnect method).
        if (hDevNotification) {
            DEV_BROADCAST_HANDLE hFilter;
            hFilter.dbch_size = sizeof(DEV_BROADCAST_HANDLE);
            hFilter.dbch_devicetype = DBT_DEVTYP_HANDLE;
            hFilter.dbch_handle = hDevice;

            hHndNotification = RegisterDeviceNotification(hWnd, (PVOID) &hFilter, DEVICE_NOTIFY_WINDOW_HANDLE);
        }

        return true;
    }


    // Got here by failing at some point

    hDevice = INVALID_HANDLE_VALUE;
    DevNum = 0;
    return false;
}


//______________________________________________________________________________

void CCyUSBDevice::DestroyHandle(void){

    if ( hDevice != INVALID_HANDLE_VALUE ) {

        if (hHndNotification)
            if (! UnregisterDeviceNotification(hHndNotification))
            {
                LastError = GetLastError();
                throw "Failed to close the handle notification handle.";
            }

            if (! CloseHandle(hDevice))
            {
                LastError = GetLastError();
                throw "Failed to close handle to driver.";
            }

            hDevice = INVALID_HANDLE_VALUE;
            hHndNotification = 0;
    }

}
//______________________________________________________________________________

void CCyUSBDevice::Close(void){

    DestroyHandle();
    ZeroMemory(&USBDeviceDescriptor, sizeof(USB_DEVICE_DESCRIPTOR));

    if(pUsbBosDescriptor)
    {
        free(pUsbBosDescriptor);
        pUsbBosDescriptor =NULL;
    }
	if(UsbBos)
	{
		delete UsbBos ;
		UsbBos = NULL;
	}
    // Clean-up dynamically allocated objects
    for (int i=0; i<Configs; i++)
        if (USBConfigDescriptors[i]) {
            free(USBConfigDescriptors[i]);
            USBConfigDescriptors[i] = NULL;
            delete USBCfgs[i];
            USBCfgs[i] = NULL;
        }


        // Reset the private pointers
        ControlEndPt      = NULL;
        IsocInEndPt       = NULL;
        IsocOutEndPt      = NULL;
        BulkInEndPt       = NULL;
        BulkOutEndPt      = NULL;
        InterruptInEndPt  = NULL;
        InterruptOutEndPt = NULL;

        Devices = 0;
        Configs = 0;
        Interfaces = 0;
        AltInterfaces = 0;

}


//______________________________________________________________________________

bool CCyUSBDevice::IoControl(ULONG cmd, PUCHAR XferBuf, ULONG len){

    if ( hDevice == INVALID_HANDLE_VALUE ) return false;
    BOOL bDioRetVal = DeviceIoControl (hDevice, cmd, XferBuf, len, XferBuf, len, &BytesXfered, NULL);
    if(!bDioRetVal)LastError = GetLastError();
    return (bDioRetVal && (BytesXfered == len));
}


//______________________________________________________________________________

bool CCyUSBDevice::GetInternalBosDescriptor()
{
    ULONG length = sizeof(SINGLE_TRANSFER) + sizeof(USB_BOS_DESCRIPTOR);
    PUCHAR buf = new UCHAR[length];
    ZeroMemory (buf, length);

    PSINGLE_TRANSFER pSingleTransfer = (PSINGLE_TRANSFER) buf;
    pSingleTransfer->SetupPacket.bmReqType.Direction = DIR_DEVICE_TO_HOST;
    pSingleTransfer->SetupPacket.bmReqType.Type      = 0;
    pSingleTransfer->SetupPacket.bmReqType.Recipient = 0;
    pSingleTransfer->SetupPacket.bRequest = USB_REQUEST_GET_DESCRIPTOR;
    pSingleTransfer->SetupPacket.wVal.hiByte = USB_BOS_DESCRIPTOR_TYPE;
    pSingleTransfer->SetupPacket.wVal.lowByte = 0;
    pSingleTransfer->SetupPacket.wLength = sizeof(USB_BOS_DESCRIPTOR);
    pSingleTransfer->SetupPacket.ulTimeOut = 5;
    pSingleTransfer->BufferLength = pSingleTransfer->SetupPacket.wLength;
    pSingleTransfer->BufferOffset = sizeof(SINGLE_TRANSFER);

    bool bRetVal = IoControl(IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER, buf, length);
    UsbdStatus = pSingleTransfer->UsbdStatus;
    NtStatus = pSingleTransfer->NtStatus;

    if (bRetVal) {
        PUSB_BOS_DESCRIPTOR BosDesc = (PUSB_BOS_DESCRIPTOR)((PCHAR)pSingleTransfer + pSingleTransfer->BufferOffset);
        USHORT BosDescLength= BosDesc->wTotalLength;

        length = sizeof(SINGLE_TRANSFER) + BosDescLength;
        PUCHAR buf2 = new UCHAR[length];
        ZeroMemory (buf2, length);

        pSingleTransfer = (PSINGLE_TRANSFER) buf2;
        pSingleTransfer->SetupPacket.bmReqType.Direction = DIR_DEVICE_TO_HOST;
        pSingleTransfer->SetupPacket.bmReqType.Type      = 0;
        pSingleTransfer->SetupPacket.bmReqType.Recipient = 0;
        pSingleTransfer->SetupPacket.bRequest = USB_REQUEST_GET_DESCRIPTOR;
        pSingleTransfer->SetupPacket.wVal.hiByte = USB_BOS_DESCRIPTOR_TYPE;
        pSingleTransfer->SetupPacket.wVal.lowByte = 0;
        pSingleTransfer->SetupPacket.wLength = BosDescLength;
        pSingleTransfer->SetupPacket.ulTimeOut = 5;
        pSingleTransfer->BufferLength = pSingleTransfer->SetupPacket.wLength;
        pSingleTransfer->BufferOffset = sizeof(SINGLE_TRANSFER);

        bRetVal = IoControl(IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER, buf2, length);
        UsbdStatus = pSingleTransfer->UsbdStatus;
        NtStatus = pSingleTransfer->NtStatus;

        if (bRetVal)
        {
            pUsbBosDescriptor = (PUSB_BOS_DESCRIPTOR)malloc(BosDescLength);
            memcpy(pUsbBosDescriptor, (PVOID)((PCHAR)pSingleTransfer + pSingleTransfer->BufferOffset), BosDescLength);
        }

        delete[] buf2;

    }

    delete[] buf;
    return bRetVal;
}
void CCyUSBDevice::GetDevDescriptor(void)
{

    ULONG length = sizeof(SINGLE_TRANSFER) + sizeof(USB_DEVICE_DESCRIPTOR);
    PUCHAR buf = new UCHAR[length];
    ZeroMemory (buf, length);

    //USB_DEVICE_DESCRIPTOR devDescriptor;
    ZeroMemory (&USBDeviceDescriptor, sizeof(USB_DEVICE_DESCRIPTOR));

    PSINGLE_TRANSFER pSingleTransfer = (PSINGLE_TRANSFER) buf;
    pSingleTransfer->SetupPacket.bmReqType.Direction = DIR_DEVICE_TO_HOST;
    pSingleTransfer->SetupPacket.bmReqType.Type      = 0;
    pSingleTransfer->SetupPacket.bmReqType.Recipient = 0;
    pSingleTransfer->SetupPacket.bRequest = USB_REQUEST_GET_DESCRIPTOR;
    pSingleTransfer->SetupPacket.wVal.hiByte = USB_DEVICE_DESCRIPTOR_TYPE;
    pSingleTransfer->SetupPacket.wLength = sizeof(USB_DEVICE_DESCRIPTOR);
    pSingleTransfer->SetupPacket.ulTimeOut = 5;
    pSingleTransfer->BufferLength = pSingleTransfer->SetupPacket.wLength;
    pSingleTransfer->BufferOffset = sizeof(SINGLE_TRANSFER);

    bool bRetVal = IoControl(IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER, buf, length);
    UsbdStatus = pSingleTransfer->UsbdStatus;
    NtStatus = pSingleTransfer->NtStatus;

    if (bRetVal) {
        memcpy(&USBDeviceDescriptor, (PVOID)((PCHAR)pSingleTransfer + pSingleTransfer->BufferOffset), sizeof(USB_DEVICE_DESCRIPTOR));

        BcdUSB         = USBDeviceDescriptor.bcdUSB;
        VendorID       = USBDeviceDescriptor.idVendor;
        ProductID      = USBDeviceDescriptor.idProduct;
        DevClass       = USBDeviceDescriptor.bDeviceClass;
        DevSubClass    = USBDeviceDescriptor.bDeviceSubClass;
        DevProtocol    = USBDeviceDescriptor.bDeviceProtocol;
		if((BcdUSB & BCDUSBJJMASK) == USB20MAJORVER)
			MaxPacketSize  = USBDeviceDescriptor.bMaxPacketSize0;
		else
			MaxPacketSize  = (1<<USBDeviceDescriptor.bMaxPacketSize0);

        BcdDevice      = USBDeviceDescriptor.bcdDevice;
        Configs        = USBDeviceDescriptor.bNumConfigurations;
    }

    delete[] buf;

    //return devDescriptor;
}

//______________________________________________________________________________

void CCyUSBDevice::SetStringDescrLanguage(void) {

    // Get the header to find-out the number of languages, size of lang ID list
    ULONG length = sizeof(SINGLE_TRANSFER) + sizeof(USB_COMMON_DESCRIPTOR);
    PUCHAR buf = new UCHAR[length];
    ZeroMemory (buf, length);

    USB_COMMON_DESCRIPTOR cmnDescriptor;
    ZeroMemory (&cmnDescriptor, sizeof(USB_COMMON_DESCRIPTOR));

    PSINGLE_TRANSFER pSingleTransfer = (PSINGLE_TRANSFER) buf;
    pSingleTransfer->SetupPacket.bmReqType.Direction = DIR_DEVICE_TO_HOST;
    pSingleTransfer->SetupPacket.bmReqType.Type      = 0;
    pSingleTransfer->SetupPacket.bmReqType.Recipient = 0;
    pSingleTransfer->SetupPacket.bRequest = USB_REQUEST_GET_DESCRIPTOR;
    pSingleTransfer->SetupPacket.wVal.hiByte = USB_STRING_DESCRIPTOR_TYPE;
    pSingleTransfer->SetupPacket.wVal.lowByte = 0;
    pSingleTransfer->SetupPacket.wLength = sizeof(USB_COMMON_DESCRIPTOR);
    pSingleTransfer->SetupPacket.ulTimeOut = 5;
    pSingleTransfer->BufferLength = pSingleTransfer->SetupPacket.wLength;
    pSingleTransfer->BufferOffset = sizeof(SINGLE_TRANSFER);

    bool bRetVal = IoControl(IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER, buf, length);
    UsbdStatus = pSingleTransfer->UsbdStatus;

    if (bRetVal) {
        memcpy(&cmnDescriptor, (PVOID)((PCHAR)pSingleTransfer + pSingleTransfer->BufferOffset), sizeof(USB_COMMON_DESCRIPTOR));

        int LangIDs = (cmnDescriptor.bLength - 2 ) / 2;

        // Get the entire descriptor, all LangIDs
        length = sizeof(SINGLE_TRANSFER) + cmnDescriptor.bLength;
        PUCHAR buf2 = new UCHAR[length];
        ZeroMemory (buf2, length);

        pSingleTransfer = (PSINGLE_TRANSFER) buf2;
        pSingleTransfer->SetupPacket.bmReqType.Direction = DIR_DEVICE_TO_HOST;
        pSingleTransfer->SetupPacket.bmReqType.Type      = 0;
        pSingleTransfer->SetupPacket.bmReqType.Recipient = 0;
        pSingleTransfer->SetupPacket.bRequest = USB_REQUEST_GET_DESCRIPTOR;
        pSingleTransfer->SetupPacket.wVal.hiByte = USB_STRING_DESCRIPTOR_TYPE;
        pSingleTransfer->SetupPacket.wVal.lowByte = 0;
        pSingleTransfer->SetupPacket.wLength = cmnDescriptor.bLength;
        pSingleTransfer->SetupPacket.ulTimeOut = 5;
        pSingleTransfer->BufferLength = pSingleTransfer->SetupPacket.wLength;
        pSingleTransfer->BufferOffset = sizeof(SINGLE_TRANSFER);

        bRetVal = IoControl(IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER, buf2, length);

        UsbdStatus = pSingleTransfer->UsbdStatus;

        if (bRetVal) {
            PUSB_STRING_DESCRIPTOR IDs = (PUSB_STRING_DESCRIPTOR) (buf2 + sizeof(SINGLE_TRANSFER));

            StrLangID =(LangIDs) ? IDs[0].bString[0] : 0;

            for (int i=0; i<LangIDs; i++) {
                USHORT id = IDs[i].bString[0];
                if (id == 0x0409) StrLangID = id;
            }

        }

        delete[] buf2;

    }

    delete[] buf;

}

//______________________________________________________________________________

void CCyUSBDevice::GetString(wchar_t *str, UCHAR sIndex)
{
    // Get the header to find-out the number of languages, size of lang ID list
    ULONG length = sizeof(SINGLE_TRANSFER) + sizeof(USB_COMMON_DESCRIPTOR);
    PUCHAR buf = new UCHAR[length];
    ZeroMemory (buf, length);

    USB_COMMON_DESCRIPTOR cmnDescriptor;
    ZeroMemory (&cmnDescriptor, sizeof(USB_COMMON_DESCRIPTOR));

    PSINGLE_TRANSFER pSingleTransfer = (PSINGLE_TRANSFER) buf;
    pSingleTransfer->SetupPacket.bmReqType.Direction = DIR_DEVICE_TO_HOST;
    pSingleTransfer->SetupPacket.bmReqType.Type      = 0;
    pSingleTransfer->SetupPacket.bmReqType.Recipient = 0;
    pSingleTransfer->SetupPacket.bRequest = USB_REQUEST_GET_DESCRIPTOR;
    pSingleTransfer->SetupPacket.wVal.hiByte = USB_STRING_DESCRIPTOR_TYPE;
    pSingleTransfer->SetupPacket.wVal.lowByte = sIndex;
    pSingleTransfer->SetupPacket.wIndex = StrLangID;
    pSingleTransfer->SetupPacket.wLength = sizeof(USB_COMMON_DESCRIPTOR);
    pSingleTransfer->SetupPacket.ulTimeOut = 5;
    pSingleTransfer->BufferLength = pSingleTransfer->SetupPacket.wLength;
    pSingleTransfer->BufferOffset = sizeof(SINGLE_TRANSFER);

    bool bRetVal = IoControl(IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER, buf, length);
    UsbdStatus = pSingleTransfer->UsbdStatus;
    NtStatus = pSingleTransfer->NtStatus;

    if (bRetVal) {
        memcpy(&cmnDescriptor, (PVOID)((PCHAR)pSingleTransfer + pSingleTransfer->BufferOffset), sizeof(USB_COMMON_DESCRIPTOR));

        // Get the entire descriptor
        length = sizeof(SINGLE_TRANSFER) + cmnDescriptor.bLength;
        PUCHAR buf2 = new UCHAR[length];
        ZeroMemory (buf2, length);

        pSingleTransfer = (PSINGLE_TRANSFER) buf2;
        pSingleTransfer->SetupPacket.bmReqType.Direction = DIR_DEVICE_TO_HOST;
        pSingleTransfer->SetupPacket.bmReqType.Type      = 0;
        pSingleTransfer->SetupPacket.bmReqType.Recipient = 0;
        pSingleTransfer->SetupPacket.bRequest = USB_REQUEST_GET_DESCRIPTOR;
        pSingleTransfer->SetupPacket.wVal.hiByte = USB_STRING_DESCRIPTOR_TYPE;
        pSingleTransfer->SetupPacket.wVal.lowByte = sIndex;
        pSingleTransfer->SetupPacket.wIndex = StrLangID;
        pSingleTransfer->SetupPacket.wLength = cmnDescriptor.bLength;
        pSingleTransfer->SetupPacket.ulTimeOut = 5;
        pSingleTransfer->BufferLength = pSingleTransfer->SetupPacket.wLength;
        pSingleTransfer->BufferOffset = sizeof(SINGLE_TRANSFER);

        bRetVal = IoControl(IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER, buf2, length);
        UsbdStatus = pSingleTransfer->UsbdStatus;
        NtStatus = pSingleTransfer->NtStatus;

        UCHAR bytes = (buf2[sizeof(SINGLE_TRANSFER)]);
        UCHAR signature = (buf2[sizeof(SINGLE_TRANSFER)+1]);

        if (bRetVal && (bytes>2) && (signature == 0x03)) {
            ZeroMemory (str, USB_STRING_MAXLEN);
            memcpy(str, (PVOID)((PCHAR)pSingleTransfer + pSingleTransfer->BufferOffset+2), bytes-2);
        }

        delete[] buf2;

    }

    delete[] buf;
}

//______________________________________________________________________________

void CCyUSBDevice::GetCfgDescriptor(int descIndex)
{

    ULONG length = sizeof(SINGLE_TRANSFER) + sizeof(USB_CONFIGURATION_DESCRIPTOR);
    PUCHAR buf = new UCHAR[length];
    ZeroMemory (buf, length);

    PSINGLE_TRANSFER pSingleTransfer = (PSINGLE_TRANSFER) buf;
    pSingleTransfer->SetupPacket.bmReqType.Direction = DIR_DEVICE_TO_HOST;
    pSingleTransfer->SetupPacket.bmReqType.Type      = 0;
    pSingleTransfer->SetupPacket.bmReqType.Recipient = 0;
    pSingleTransfer->SetupPacket.bRequest = USB_REQUEST_GET_DESCRIPTOR;
    pSingleTransfer->SetupPacket.wVal.hiByte = USB_CONFIGURATION_DESCRIPTOR_TYPE;
    pSingleTransfer->SetupPacket.wVal.lowByte = descIndex;
    pSingleTransfer->SetupPacket.wLength = sizeof(USB_CONFIGURATION_DESCRIPTOR);
    pSingleTransfer->SetupPacket.ulTimeOut = 5;
    pSingleTransfer->BufferLength = pSingleTransfer->SetupPacket.wLength;
    pSingleTransfer->BufferOffset = sizeof(SINGLE_TRANSFER);

    bool bRetVal = IoControl(IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER, buf, length);
    UsbdStatus = pSingleTransfer->UsbdStatus;
    NtStatus = pSingleTransfer->NtStatus;

    if (bRetVal) {
        PUSB_CONFIGURATION_DESCRIPTOR configDesc = (PUSB_CONFIGURATION_DESCRIPTOR)((PCHAR)pSingleTransfer + pSingleTransfer->BufferOffset);
        USHORT configDescLength= configDesc->wTotalLength;

        length = sizeof(SINGLE_TRANSFER) + configDescLength;
        PUCHAR buf2 = new UCHAR[length];
        ZeroMemory (buf2, length);

        pSingleTransfer = (PSINGLE_TRANSFER) buf2;
        pSingleTransfer->SetupPacket.bmReqType.Direction = DIR_DEVICE_TO_HOST;
        pSingleTransfer->SetupPacket.bmReqType.Type      = 0;
        pSingleTransfer->SetupPacket.bmReqType.Recipient = 0;
        pSingleTransfer->SetupPacket.bRequest = USB_REQUEST_GET_DESCRIPTOR;
        pSingleTransfer->SetupPacket.wVal.hiByte = USB_CONFIGURATION_DESCRIPTOR_TYPE;
        pSingleTransfer->SetupPacket.wVal.lowByte = descIndex;
        pSingleTransfer->SetupPacket.wLength = configDescLength;
        pSingleTransfer->SetupPacket.ulTimeOut = 5;
        pSingleTransfer->BufferLength = pSingleTransfer->SetupPacket.wLength;
        pSingleTransfer->BufferOffset = sizeof(SINGLE_TRANSFER);

        bRetVal = IoControl(IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER, buf2, length);
        UsbdStatus = pSingleTransfer->UsbdStatus;
        NtStatus = pSingleTransfer->NtStatus;

        if (bRetVal){
            USBConfigDescriptors[descIndex] = (PUSB_CONFIGURATION_DESCRIPTOR)malloc(configDescLength);
            memcpy(USBConfigDescriptors[descIndex], (PVOID)((PCHAR)pSingleTransfer + pSingleTransfer->BufferOffset), configDescLength);
        }

        delete[] buf2;

    }

    delete[] buf;
}

//______________________________________________________________________________

bool CCyUSBDevice::Reset()
{
    return IoControl(IOCTL_ADAPT_RESET_PARENT_PORT, NULL, 0);
}

//______________________________________________________________________________

bool CCyUSBDevice::ReConnect()
{
    return IoControl(IOCTL_ADAPT_CYCLE_PORT, NULL, 0);
}


//______________________________________________________________________________

bool CCyUSBDevice::Suspend()
{
    if (PowerState() == 4) return true;

    ULONG state = 4;
    bool ret = IoControl(IOCTL_ADAPT_SET_DEVICE_POWER_STATE, (PUCHAR) &state, sizeof(state));

    if ( 0 == state) return ret;

    return true;

}


//______________________________________________________________________________

bool CCyUSBDevice::Resume()
{
    if (PowerState() == 1) return true;

    ULONG state = 1;
    bool ret = IoControl(IOCTL_ADAPT_SET_DEVICE_POWER_STATE, (PUCHAR) &state, sizeof(state));

    if ( 0 == state) return ret;

    return true;
}


//______________________________________________________________________________

void CCyUSBDevice::GetSpeed()
{
    ULONG speed = 0;
    bHighSpeed = false;
    bSuperSpeed = false;

    IoControl(IOCTL_ADAPT_GET_DEVICE_SPEED, (PUCHAR) &speed, sizeof(speed));

    bHighSpeed = (speed == DEVICE_SPEED_HIGH);
    bSuperSpeed = (speed == DEVICE_SPEED_SUPER);
}


//______________________________________________________________________________

UCHAR CCyUSBDevice::PowerState()
{
    ULONG state;
    bool rval = IoControl(IOCTL_ADAPT_GET_DEVICE_POWER_STATE, (PUCHAR) &state, sizeof(state));

    UCHAR s = (UCHAR) NULL;

    if (rval) s = (UCHAR) state;

    return s;

}


//______________________________________________________________________________

CCyUSBEndPoint * CCyUSBDevice::EndPointOf(UCHAR addr) //throw(...)
{

    if (addr == 0) return ControlEndPt;

    CCyUSBEndPoint *ept;

    int n = EndPointCount();

    for (int i=0; i<n; i++) {
        ept = USBCfgs[CfgNum]->Interfaces[IntfcIndex]->EndPoints[i];

        if (ept)
        {
            if (addr == ept->Address) return ept; }
        else
            throw "Failed to find endpoint.";
    }

    return NULL; // Error

}
//______________________________________________________________________________

bool CCyUSBDevice::GetBosDescriptor(PUSB_BOS_DESCRIPTOR descr)
{
    if ((BcdUSB & BCDUSBJJMASK) == USB30MAJORVER)
    {
        if(UsbBos!=NULL)
        {
            descr->bLength =  UsbBos->bLength;
            descr->bDescriptorType =  UsbBos->bDescriptorType;
            descr->bNumDeviceCaps =  UsbBos->bNumDeviceCaps;
            descr->wTotalLength =  UsbBos->wTotalLength;
            return true;
        }
        return false;
    }
    else
        return false;
}
bool CCyUSBDevice::GetBosUSB20DeviceExtensionDescriptor(PUSB_BOS_USB20_DEVICE_EXTENSION descr)
{
    if ((BcdUSB & BCDUSBJJMASK) == USB30MAJORVER)
    {
        if(UsbBos->pUSB20_DeviceExt != NULL)
        {
            descr->bLength = UsbBos->pUSB20_DeviceExt->bLength;
            descr->bDescriptorType =  UsbBos->pUSB20_DeviceExt->bDescriptorType;
            descr->bDevCapabilityType =  UsbBos->pUSB20_DeviceExt->bDevCapabilityType;
            descr->bmAttribute =  UsbBos->pUSB20_DeviceExt->bmAttribute;
            return true;
        }
        return false;
    }
    else
        return false;
}
bool CCyUSBDevice::GetBosContainedIDDescriptor(PUSB_BOS_CONTAINER_ID descr)
{
    if ((BcdUSB & BCDUSBJJMASK) == USB30MAJORVER)
    {
        if(UsbBos->pContainer_ID != NULL)
        {
            descr->bLength = UsbBos->pContainer_ID->bLength;
            descr->bDescriptorType =  UsbBos->pContainer_ID->bDescriptorType;
            descr->bDevCapabilityType =  UsbBos->pContainer_ID->bDevCapabilityType;
            descr->bReserved =  UsbBos->pContainer_ID->bReserved;
            for (int i=0;i<USB_BOS_CAPABILITY_TYPE_CONTAINER_ID_SIZE;i++)
                descr->ContainerID[i] = UsbBos->pContainer_ID->ContainerID[i];

            return true;
        }
        return false;
    }
    else
        return false;
}
bool CCyUSBDevice::GetBosSSCapabilityDescriptor(PUSB_BOS_SS_DEVICE_CAPABILITY descr)
{
    if ((BcdUSB & BCDUSBJJMASK) == USB30MAJORVER)
    {
        if(UsbBos->pSS_DeviceCap != NULL)
        {
            descr->bLength = UsbBos->pSS_DeviceCap->bLength;
            descr->bDescriptorType =  UsbBos->pSS_DeviceCap->bDescriptorType;
            descr->bDevCapabilityType =  UsbBos->pSS_DeviceCap->bDevCapabilityType;
            descr->bFunctionalitySupporte =  UsbBos->pSS_DeviceCap->bFunctionalitySupporte;
            descr->bmAttribute =  UsbBos->pSS_DeviceCap->bmAttribute;
            descr->bU1DevExitLat =  UsbBos->pSS_DeviceCap->bU1DevExitLat;
            descr->wSpeedsSuported =  UsbBos->pSS_DeviceCap->SpeedsSuported;
            descr->bU2DevExitLat =  UsbBos->pSS_DeviceCap->bU2DevExitLat;
            return true;
        }
        return false;
    }
    else
        return false;
}
//______________________________________________________________________________

void CCyUSBDevice::GetDeviceDescriptor(PUSB_DEVICE_DESCRIPTOR descr)
{
    // Copy the internal private data to the passed parameter
    *descr = USBDeviceDescriptor;
}

//______________________________________________________________________________

void CCyUSBDevice::GetConfigDescriptor(PUSB_CONFIGURATION_DESCRIPTOR descr)
{
    // Copy the internal private data to the passed parameter
    *descr = *USBConfigDescriptors[CfgNum];
}

//______________________________________________________________________________

void CCyUSBDevice::GetIntfcDescriptor(PUSB_INTERFACE_DESCRIPTOR descr)
{
    CCyUSBInterface *i = USBCfgs[CfgNum]->Interfaces[IntfcIndex];

    // Copy the internal private data to the passed parameter
    descr->bLength            = i->bLength;
    descr->bDescriptorType    = i->bDescriptorType;
    descr->bInterfaceNumber   = i->bInterfaceNumber;
    descr->bAlternateSetting  = i->bAlternateSetting;
    descr->bNumEndpoints      = i->bNumEndpoints;
    descr->bInterfaceClass    = i->bInterfaceClass;
    descr->bInterfaceSubClass = i->bInterfaceSubClass;
    descr->bInterfaceProtocol = i->bInterfaceProtocol;
    descr->iInterface         = i->iInterface;
}

//______________________________________________________________________________

CCyUSBConfig CCyUSBDevice::GetUSBConfig(int index)
{
    // Rarely (i.e. never) have more than 1 Configuration
    int i = (index > (Configs-1)) ? 0 : index;

    return *USBCfgs[i];   // Invoke the copy constructor
}

//______________________________________________________________________________

void  CCyUSBDevice::UsbdStatusString(ULONG stat, PCHAR str)
{
    //CString tmp;  // MFC Version
    //String tmp;

    char StateStr[24];
    char StatusStr[64];
    ZeroMemory(StateStr,24);
    ZeroMemory(StatusStr,64);

    if (! USBD_STATUS(stat)) {
        sprintf(StateStr,"[state=SUCCESS ");
        sprintf(StatusStr,"status=USBD_STATUS_SUCCESS]");
    } else {

        switch(USBD_STATE(stat)) {
       case USBD_STATUS_SUCCESS: sprintf(StateStr," [state=SUCCESS "); break;
       case USBD_STATUS_PENDING: sprintf(StateStr," [state=PENDING "); break;
       case USBD_STATUS_HALTED:	 sprintf(StateStr," [state=STALLED "); break;
       case USBD_STATUS_ERROR: 	 sprintf(StateStr," [state=ERROR   "); break;
       default: break;
        }

        switch(stat|0xC0000000L) { // Note: error typedefs have both error and stall bit set
       case USBD_STATUS_CRC:		        sprintf(StatusStr,"status=USBD_STATUS_CRC]"); break;
       case USBD_STATUS_BTSTUFF:	        sprintf(StatusStr,"status=USBD_STATUS_BTSTUFF]"); break;
       case USBD_STATUS_DATA_TOGGLE_MISMATCH:	sprintf(StatusStr,"status=USBD_STATUS_DATA_TOGGLE_MISMATCH]"); break;
       case USBD_STATUS_STALL_PID:		sprintf(StatusStr,"status=USBD_STATUS_STALL_PID]"); break;
       case USBD_STATUS_DEV_NOT_RESPONDING:     sprintf(StatusStr,"status=USBD_STATUS_DEV_NOT_RESPONDING]"); break;
       case USBD_STATUS_PID_CHECK_FAILURE:	sprintf(StatusStr,"status=USBD_STATUS_PID_CHECK_FAILURE]"); break;
       case USBD_STATUS_UNEXPECTED_PID:	        sprintf(StatusStr,"status=USBD_STATUS_UNEXPECTED_PID]"); break;
       case USBD_STATUS_DATA_OVERRUN:		sprintf(StatusStr,"status=USBD_STATUS_DATA_OVERRUN]"); break;
       case USBD_STATUS_DATA_UNDERRUN:		sprintf(StatusStr,"status=USBD_STATUS_DATA_UNDERRUN]"); break;
       case USBD_STATUS_RESERVED1:		sprintf(StatusStr,"status=USBD_STATUS_RESERVED1]"); break;
       case USBD_STATUS_RESERVED2:		sprintf(StatusStr,"status=USBD_STATUS_RESERVED2]"); break;
       case USBD_STATUS_BUFFER_OVERRUN:	        sprintf(StatusStr,"status=USBD_STATUS_BUFFER_OVERRUN]"); break;
       case USBD_STATUS_BUFFER_UNDERRUN:	sprintf(StatusStr,"status=USBD_STATUS_BUFFER_UNDERRUN]"); break;
       case USBD_STATUS_NOT_ACCESSED:		sprintf(StatusStr,"status=USBD_STATUS_NOT_ACCESSED]"); break;
       case USBD_STATUS_FIFO:			sprintf(StatusStr,"status=USBD_STATUS_FIFO]"); break;

       case USBD_STATUS_ENDPOINT_HALTED:	sprintf(StatusStr,"status=USBD_STATUS_ENDPOINT_HALTED]"); break;
       case USBD_STATUS_NO_MEMORY:		sprintf(StatusStr,"status=USBD_STATUS_NO_MEMORY]"); break;
       case USBD_STATUS_INVALID_URB_FUNCTION:	sprintf(StatusStr,"status=USBD_STATUS_INVALID_URB_FUNCTION]"); break;
       case USBD_STATUS_INVALID_PARAMETER:	sprintf(StatusStr,"status=USBD_STATUS_INVALID_PARAMETER]"); break;
       case USBD_STATUS_ERROR_BUSY:		sprintf(StatusStr,"status=USBD_STATUS_ERROR_BUSY]"); break;
       case USBD_STATUS_REQUEST_FAILED:	        sprintf(StatusStr,"status=USBD_STATUS_REQUEST_FAILED]"); break;
       case USBD_STATUS_INVALID_PIPE_HANDLE:	sprintf(StatusStr,"status=USBD_STATUS_INVALID_PIPE_HANDLE]"); break;
       case USBD_STATUS_NO_BANDWIDTH:		sprintf(StatusStr,"status=USBD_STATUS_NO_BANDWIDTH]"); break;
       case USBD_STATUS_INTERNAL_HC_ERROR:	sprintf(StatusStr,"status=USBD_STATUS_INTERNAL_HC_ERROR]"); break;
       case USBD_STATUS_ERROR_SHORT_TRANSFER:	sprintf(StatusStr,"status=USBD_STATUS_ERROR_SHORT_TRANSFER]"); break;
       case USBD_STATUS_BAD_START_FRAME:	sprintf(StatusStr,"status=USBD_STATUS_BAD_START_FRAME]"); break;
       case USBD_STATUS_ISOCH_REQUEST_FAILED:	sprintf(StatusStr,"status=USBD_STATUS_ISOCH_REQUEST_FAILED]"); break;
       case USBD_STATUS_FRAME_CONTROL_OWNED:	sprintf(StatusStr,"status=USBD_STATUS_FRAME_CONTROL_OWNED]"); break;
       case USBD_STATUS_FRAME_CONTROL_NOT_OWNED:sprintf(StatusStr,"status=USBD_STATUS_FRAME_CONTROL_NOT_OWNED]"); break;
       case USBD_STATUS_CANCELED:		sprintf(StatusStr,"status=USBD_STATUS_CANCELED]"); break;
       case USBD_STATUS_CANCELING:		sprintf(StatusStr,"status=USBD_STATUS_CANCELING]"); break;

       default: sprintf(StatusStr,"status=UNKNOWN]"); break;
        }
    }

    sprintf(str, "%s%s", StateStr, StatusStr);

}

//______________________________________________________________________________

// This method is called from Open, just after ControlEndPt has been instantiated
// It also gets called whenever a different Alt Interface setting is made
void CCyUSBDevice::SetEndPointPtrs(void)
{
    if (Configs == 0) return;
    if (Interfaces == 0) return;

    EndPoints = USBCfgs[CfgNum]->Interfaces[IntfcIndex]->EndPoints;
    EndPoints[0] = ControlEndPt;
    int eptCount = EndPointCount() - 1;

    IsocInEndPt       = NULL;
    IsocOutEndPt      = NULL;
    BulkInEndPt       = NULL;
    BulkOutEndPt      = NULL;
    InterruptInEndPt  = NULL;
    InterruptOutEndPt = NULL;

    for (int i=0; i<eptCount; i++) {
        UCHAR bIn = EndPoints[i+1]->Address & 0x80;
        UCHAR attrib = EndPoints[i+1]->Attributes;


        EndPoints[i+1]->XferMode = XMODE_DIRECT;

        if ((IsocInEndPt == NULL) && (attrib == 1) && bIn) IsocInEndPt = (CCyIsocEndPoint *)EndPoints[i+1];
        if ((BulkInEndPt == NULL) && (attrib == 2) && bIn) BulkInEndPt = (CCyBulkEndPoint *)EndPoints[i+1];
        if ((InterruptInEndPt == NULL) && (attrib == 3) && bIn) InterruptInEndPt = (CCyInterruptEndPoint *)EndPoints[i+1];

        if ((IsocOutEndPt == NULL) && (attrib == 1) && !bIn) IsocOutEndPt = (CCyIsocEndPoint *)EndPoints[i+1];
        if ((BulkOutEndPt == NULL) && (attrib == 2) && !bIn) BulkOutEndPt = (CCyBulkEndPoint *)EndPoints[i+1];
        if ((InterruptOutEndPt == NULL) && (attrib == 3) && !bIn) InterruptOutEndPt = (CCyInterruptEndPoint *)EndPoints[i+1];
    }

}

//______________________________________________________________________________

bool CCyUSBDevice::RegisterForPnpEvents(HANDLE Handle)
{
    DEV_BROADCAST_DEVICEINTERFACE dFilter = {0};
    dFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    dFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    dFilter.dbcc_classguid = DrvGuid;

    hDevNotification = RegisterDeviceNotification(Handle, (PVOID) &dFilter, DEVICE_NOTIFY_WINDOW_HANDLE);
    if (!hDevNotification)
    {
        LastError = GetLastError();
        return false;
    }

    return true;
}


////////////////////////////////////////////////////////////////////////////////
//
// The USB Interface Class
//
////////////////////////////////////////////////////////////////////////////////


// The Copy Constructor
CCyUSBInterface::CCyUSBInterface(CCyUSBInterface &ifc)
{
    bLength            = ifc.bLength;
    bDescriptorType    = ifc.bDescriptorType;
    bInterfaceNumber   = ifc.bInterfaceNumber;
    bAlternateSetting  = ifc.bAlternateSetting;
    bNumEndpoints      = ifc.bNumEndpoints;
    bInterfaceClass    = ifc.bInterfaceClass;
    bInterfaceSubClass = ifc.bInterfaceSubClass;
    bInterfaceProtocol = ifc.bInterfaceProtocol;
    iInterface         = ifc.iInterface;

    wTotalLength       = ifc.wTotalLength;

    int i;

    for (i=0; i<MAX_ENDPTS; i++) EndPoints[i] = NULL;

    for (i=0; i<MAX_ENDPTS; i++)
        if (ifc.EndPoints[i]){

            if (ifc.EndPoints[i]->Attributes == 0) {
                CCyControlEndPoint *e = (CCyControlEndPoint*)ifc.EndPoints[i];
                EndPoints[i] = new CCyControlEndPoint(*e);
            }

            if (ifc.EndPoints[i]->Attributes == 1) {
                CCyIsocEndPoint *e = (CCyIsocEndPoint*)ifc.EndPoints[i];
                EndPoints[i] = new CCyIsocEndPoint(*e);
            }

            if (ifc.EndPoints[i]->Attributes == 2) {
                CCyBulkEndPoint *e = (CCyBulkEndPoint*)ifc.EndPoints[i];
                EndPoints[i] = new CCyBulkEndPoint(*e);
            }

            if (ifc.EndPoints[i]->Attributes == 3) {
                CCyInterruptEndPoint *e = (CCyInterruptEndPoint*)ifc.EndPoints[i];
                EndPoints[i] = new CCyInterruptEndPoint(*e);
            }

        }

}


CCyUSBInterface::CCyUSBInterface(HANDLE handle, PUSB_INTERFACE_DESCRIPTOR pIntfcDescriptor)
{

    PUSB_ENDPOINT_DESCRIPTOR endPtDesc;

    bLength            = pIntfcDescriptor->bLength;
    bDescriptorType    = pIntfcDescriptor->bDescriptorType;
    bInterfaceNumber   = pIntfcDescriptor->bInterfaceNumber;
    bAlternateSetting  = pIntfcDescriptor->bAlternateSetting;
    bNumEndpoints      = pIntfcDescriptor->bNumEndpoints;
    bInterfaceClass    = pIntfcDescriptor->bInterfaceClass;
    bInterfaceSubClass = pIntfcDescriptor->bInterfaceSubClass;
    bInterfaceProtocol = pIntfcDescriptor->bInterfaceProtocol;
    iInterface         = pIntfcDescriptor->iInterface;

    bAltSettings = 0;
    wTotalLength = bLength;

    PUCHAR desc = (PUCHAR)pIntfcDescriptor  + pIntfcDescriptor->bLength;

    int i;
    int unexpected = 0;

    for (i=0; i<MAX_ENDPTS; i++) EndPoints[i] = NULL;

    for (i=0; i<bNumEndpoints; i++) {

        endPtDesc = (PUSB_ENDPOINT_DESCRIPTOR) (desc);
        wTotalLength += endPtDesc->bLength;

        // We leave slot [0] empty to hold THE obligatory EndPoint Zero control endpoint
        if (endPtDesc->bDescriptorType == USB_ENDPOINT_DESCRIPTOR_TYPE) {
            switch (endPtDesc->bmAttributes ) {
       case 0:
           EndPoints[i+1] = new CCyControlEndPoint(handle, endPtDesc);
           break;
       case 1:
           EndPoints[i+1] = new CCyIsocEndPoint(handle, endPtDesc);
           break;
       case 2:
           EndPoints[i+1] = new CCyBulkEndPoint(handle, endPtDesc);
           break;
       case 3:
           EndPoints[i+1] = new CCyInterruptEndPoint(handle, endPtDesc);
           break;
            }

            desc += endPtDesc->bLength;
        } else {

            unexpected++;
            if (unexpected < 12) {  // Sanity check - prevent infinite loop

                // This may have been a class-specific descriptor (like HID).  Skip it.
                desc += endPtDesc->bLength;

                // Stay in the loop, grabbing the next descriptor
                i--;
            }

        }

    }


}

CCyUSBInterface::CCyUSBInterface(HANDLE handle, PUSB_INTERFACE_DESCRIPTOR pIntfcDescriptor,UCHAR usb30Dummy)
{

    PUSB_ENDPOINT_DESCRIPTOR endPtDesc;

    bLength            = pIntfcDescriptor->bLength;
    bDescriptorType    = pIntfcDescriptor->bDescriptorType;
    bInterfaceNumber   = pIntfcDescriptor->bInterfaceNumber;
    bAlternateSetting  = pIntfcDescriptor->bAlternateSetting;
    bNumEndpoints      = pIntfcDescriptor->bNumEndpoints;
    bInterfaceClass    = pIntfcDescriptor->bInterfaceClass;
    bInterfaceSubClass = pIntfcDescriptor->bInterfaceSubClass;
    bInterfaceProtocol = pIntfcDescriptor->bInterfaceProtocol;
    iInterface         = pIntfcDescriptor->iInterface;

    bAltSettings = 0;
    wTotalLength = bLength;

    PUCHAR desc = (PUCHAR)pIntfcDescriptor  + pIntfcDescriptor->bLength;

    int i;
    int unexpected = 0;

    for (i=0; i<MAX_ENDPTS; i++) EndPoints[i] = NULL;

    for (i=0; i<bNumEndpoints; i++)
    {
        bool bSSDec = false;
        endPtDesc = (PUSB_ENDPOINT_DESCRIPTOR) (desc);
        desc += endPtDesc->bLength;

        PUSB_SUPERSPEED_ENDPOINT_COMPANION_DESCRIPTOR ssendPtDesc = (PUSB_SUPERSPEED_ENDPOINT_COMPANION_DESCRIPTOR)desc;
        wTotalLength += endPtDesc->bLength;

        // We leave slot [0] empty to hold THE obligatory EndPoint Zero control endpoint
        if (ssendPtDesc != NULL)
            bSSDec = (ssendPtDesc->bDescriptorType == USB_SUPERSPEED_ENDPOINT_COMPANION);

        if ((endPtDesc->bDescriptorType == USB_ENDPOINT_DESCRIPTOR_TYPE) && bSSDec)
        {
            switch (endPtDesc->bmAttributes )
            {
            case 0:
                EndPoints[i+1] = new CCyControlEndPoint(handle, endPtDesc);
                break;
            case 1:
                EndPoints[i+1] = new CCyIsocEndPoint(handle, endPtDesc,ssendPtDesc);
                break;
            case 2:
                EndPoints[i+1] = new CCyBulkEndPoint(handle, endPtDesc,ssendPtDesc);
                break;
            case 3:
                EndPoints[i+1] = new CCyInterruptEndPoint(handle, endPtDesc,ssendPtDesc);
                break;
            }

            desc += ssendPtDesc->bLength;
            wTotalLength += ssendPtDesc->bLength;

        }
        else if (endPtDesc->bDescriptorType == USB_ENDPOINT_DESCRIPTOR_TYPE)
        {
            switch (endPtDesc->bmAttributes )
            {
            case 0:
                EndPoints[i+1] = new CCyControlEndPoint(handle, endPtDesc);
                break;
            case 1:
                EndPoints[i+1] = new CCyIsocEndPoint(handle, endPtDesc);
                break;
            case 2:
                EndPoints[i+1] = new CCyBulkEndPoint(handle, endPtDesc);
                break;
            case 3:
                EndPoints[i+1] = new CCyInterruptEndPoint(handle, endPtDesc);
                break;
            }

            //desc += endPtDesc->bLength; // THis a bug for the composite device

        }
        else
        {

            unexpected++;
            if (unexpected < 12) {  // Sanity check - prevent infinite loop

                // This may have been a class-specific descriptor (like HID).  Skip it.
                desc += endPtDesc->bLength;

                // Stay in the loop, grabbing the next descriptor
                i--;
            }

        }

    }


}

//______________________________________________________________________________

CCyUSBInterface::~CCyUSBInterface(void) {

    for (int i=0; i<MAX_ENDPTS; i++)
        if (EndPoints[i])
        {
            delete EndPoints[i];
            EndPoints[i] = NULL;
        }

}


////////////////////////////////////////////////////////////////////////////////
//
// The USB Config Class
//
////////////////////////////////////////////////////////////////////////////////

CCyUSBConfig::CCyUSBConfig(void)
{
    bLength             = 0;
    bDescriptorType     = 0;
    wTotalLength        = 0;
    bNumInterfaces      = 0;
    AltInterfaces       = 0;
    bConfigurationValue = 0;
    iConfiguration      = 0;
    bmAttributes        = 0;
    MaxPower            = 0;

    for (int i=0; i<MAX_INTERFACES; i++) Interfaces[i] = NULL;

}


// The Copy Constructor
CCyUSBConfig::CCyUSBConfig(CCyUSBConfig &cfg)
{
    bLength             = cfg.bLength;
    bDescriptorType     = cfg.bDescriptorType;
    wTotalLength        = cfg.wTotalLength;
    bNumInterfaces      = cfg.bNumInterfaces;
    AltInterfaces       = cfg.AltInterfaces;
    bConfigurationValue = cfg.bConfigurationValue;
    iConfiguration      = cfg.iConfiguration;
    bmAttributes        = cfg.bmAttributes;
    MaxPower            = cfg.MaxPower;
    AltInterfaces       = cfg.AltInterfaces;

    int i;

    for (i=0; i<MAX_INTERFACES; i++) Interfaces[i] = NULL;

    for (i=0; i<AltInterfaces; i++)
        Interfaces[i] = new CCyUSBInterface(*cfg.Interfaces[i]);

}


CCyUSBConfig::CCyUSBConfig(HANDLE handle, PUSB_CONFIGURATION_DESCRIPTOR pConfigDescr){

    PUSB_INTERFACE_DESCRIPTOR interfaceDesc;
    int i;

    bLength             = pConfigDescr->bLength;
    bDescriptorType     = pConfigDescr->bDescriptorType;
    wTotalLength        = pConfigDescr->wTotalLength;
    bNumInterfaces      = pConfigDescr->bNumInterfaces;
    AltInterfaces       = 0;
    bConfigurationValue = pConfigDescr->bConfigurationValue;
    iConfiguration      = pConfigDescr->iConfiguration;
    bmAttributes        = pConfigDescr->bmAttributes;
    MaxPower            = pConfigDescr->MaxPower;

    int tLen = pConfigDescr->wTotalLength;

    PUCHAR desc = (PUCHAR)pConfigDescr  + pConfigDescr->bLength;
    int bytesConsumed = pConfigDescr->bLength;

    for (i=0; i<MAX_INTERFACES; i++) Interfaces[i] = NULL;

    i = 0;
    do {
        interfaceDesc = (PUSB_INTERFACE_DESCRIPTOR) (desc);

        if (interfaceDesc->bDescriptorType == USB_INTERFACE_DESCRIPTOR_TYPE) {
            Interfaces[i] = new CCyUSBInterface(handle, interfaceDesc);
            i++;
            AltInterfaces++;  // Actually the total number of interfaces for the config
            bytesConsumed += Interfaces[i-1]->wTotalLength;
        } else {
            // Unexpected descriptor type
            // Just skip it and go on  - could have thrown an exception instead
            // since this indicates that the descriptor structure is invalid.
            bytesConsumed += interfaceDesc->bLength;
        }


        desc = (PUCHAR) pConfigDescr + bytesConsumed;

    } while ((bytesConsumed < tLen) && (i<MAX_INTERFACES)); // Only support 8 total interfaces


    // Count the alt interfaces for each interface number
    for (i=0; i<AltInterfaces; i++) {

        Interfaces[i]->bAltSettings = 0;

        for (int j=0; j<AltInterfaces; j++) // Walk the list looking for identical bInterfaceNumbers
            if (Interfaces[i]->bInterfaceNumber == Interfaces[j]->bInterfaceNumber)
                Interfaces[i]->bAltSettings++;

    }

}


CCyUSBConfig::CCyUSBConfig(HANDLE handle, PUSB_CONFIGURATION_DESCRIPTOR pConfigDescr,UCHAR usb30Dummy)
{

    PUSB_INTERFACE_DESCRIPTOR interfaceDesc;
    int i;

    bLength             = pConfigDescr->bLength;
    bDescriptorType     = pConfigDescr->bDescriptorType;
    wTotalLength        = pConfigDescr->wTotalLength;
    bNumInterfaces      = pConfigDescr->bNumInterfaces;
    AltInterfaces       = 0;
    bConfigurationValue = pConfigDescr->bConfigurationValue;
    iConfiguration      = pConfigDescr->iConfiguration;
    bmAttributes        = pConfigDescr->bmAttributes;
    MaxPower            = pConfigDescr->MaxPower;

    int tLen = pConfigDescr->wTotalLength;

    PUCHAR desc = (PUCHAR)pConfigDescr  + pConfigDescr->bLength;
    int bytesConsumed = pConfigDescr->bLength;

    for (i=0; i<MAX_INTERFACES; i++) Interfaces[i] = NULL;

    i = 0;
    do {
        interfaceDesc = (PUSB_INTERFACE_DESCRIPTOR) (desc);

        if (interfaceDesc->bDescriptorType == USB_INTERFACE_DESCRIPTOR_TYPE) {
            Interfaces[i] = new CCyUSBInterface(handle, interfaceDesc,usb30Dummy);
            i++;
            AltInterfaces++;  // Actually the total number of interfaces for the config
            bytesConsumed += Interfaces[i-1]->wTotalLength;
        } else {
            // Unexpected descriptor type
            // Just skip it and go on  - could have thrown an exception instead
            // since this indicates that the descriptor structure is invalid.
            bytesConsumed += interfaceDesc->bLength;
        }


        desc = (PUCHAR) pConfigDescr + bytesConsumed;

    } while ((bytesConsumed < tLen) && (i<MAX_INTERFACES)); // Only support 8 total interfaces


    // Count the alt interfaces for each interface number
    for (i=0; i<AltInterfaces; i++) {

        Interfaces[i]->bAltSettings = 0;

        for (int j=0; j<AltInterfaces; j++) // Walk the list looking for identical bInterfaceNumbers
            if (Interfaces[i]->bInterfaceNumber == Interfaces[j]->bInterfaceNumber)
                Interfaces[i]->bAltSettings++;

    }

}


//______________________________________________________________________________

CCyUSBConfig::~CCyUSBConfig(void){

    for (int i=0; i<MAX_INTERFACES; i++)
        if (Interfaces[i]) {

            // Detect the first non-zero EndPoints[0] - the control endpoint -
            // and delete it.  Then, make sure all other interfaces have 0 as
            // the value for EndPoints[0], as anything else would be a redundant
            // reference to the control endpoint and cause an error when trying
            // to delete it a second time.

            if (Interfaces[i]->EndPoints[0] != 0)
            {
                delete Interfaces[i]->EndPoints[0];

                for (int j=0; j<MAX_INTERFACES; j++)
                    if (Interfaces[j])
                        Interfaces[j]->EndPoints[0] = NULL;
            }

            delete Interfaces[i];
            Interfaces[i] = NULL;
        }

}

////////////////////////////////////////////////////////////////////////////////
//
// The CCyEndPoint ABSTRACT Class
//
////////////////////////////////////////////////////////////////////////////////

CCyUSBEndPoint::CCyUSBEndPoint(void)
{
    hDevice    = INVALID_HANDLE_VALUE;

    DscLen     = 0;
    DscType    = 0;
    Address    = 0;
    Attributes = 0;
    MaxPktSize = 0;
    Interval   = 0;
    TimeOut    = 10000;  // 10 Sec timeout is default
    bIn        = false;
    XferMode   = XMODE_DIRECT;
    LastError  = 0;
    // Initialize the SS companion descriptor with zero, as this constructore will be called for usb2.0 device
    ssdscLen =0;
    ssdscType =0;
    ssmaxburst=0; /* Maximum number of packets endpoint can send in one burst*/
    ssbmAttribute=0; // store endpoint attribute like for bulk it will be number of streams
    ssbytesperinterval=0;
}

// Copy constructor
CCyUSBEndPoint::CCyUSBEndPoint(CCyUSBEndPoint& ept)
{
    hDevice    = ept.hDevice;

    DscLen     = ept.DscLen;
    DscType    = ept.DscType;
    Address    = ept.Address;
    Attributes = ept.Attributes;
    MaxPktSize = ept.MaxPktSize;
    Interval   = ept.Interval;
    TimeOut    = ept.TimeOut;
    bIn        = ept.bIn;
    XferMode   = ept.XferMode;
    ssdscLen =ept.ssdscLen;
    ssdscType =ept.ssdscType;
    ssmaxburst=ept.ssmaxburst; /* Maximum number of packets endpoint can send in one burst*/
    ssbmAttribute=ept.ssbmAttribute; // store endpoint attribute like for bulk it will be number of streams
    ssbytesperinterval=ept.ssbytesperinterval;
    LastError  = 0;
}

CCyUSBEndPoint::CCyUSBEndPoint(HANDLE hDev, PUSB_ENDPOINT_DESCRIPTOR pEndPtDescriptor)
{

    hDevice    = hDev;

    if (pEndPtDescriptor) {
        PktsPerFrame = (pEndPtDescriptor->wMaxPacketSize & 0x1800) >> 11;
        PktsPerFrame++;

        DscLen     = pEndPtDescriptor->bLength;
        DscType    = pEndPtDescriptor->bDescriptorType;
        Address    = pEndPtDescriptor->bEndpointAddress;
        Attributes = pEndPtDescriptor->bmAttributes;
        MaxPktSize = (pEndPtDescriptor->wMaxPacketSize & 0x7ff) * PktsPerFrame;
        Interval   = pEndPtDescriptor->bInterval;
        bIn        = ((Address & 0x80) == 0x80);
    }

    TimeOut   = 10000;  // 10 Sec timeout is default

    XferMode  = XMODE_DIRECT;  // Normally, use Direct xfers

    // Initialize the SS companion descriptor with zero, as this constructore will be called for usb2.0 device
    ssdscLen =0;
    ssdscType =0;
    ssmaxburst=0; /* Maximum number of packets endpoint can send in one burst*/
    ssbmAttribute=0; // store endpoint attribute like for bulk it will be number of streams
    ssbytesperinterval=0;

}


CCyUSBEndPoint::CCyUSBEndPoint(HANDLE hDev, PUSB_ENDPOINT_DESCRIPTOR pEndPtDescriptor,USB_SUPERSPEED_ENDPOINT_COMPANION_DESCRIPTOR* SSEndPtDescriptor)
{

    hDevice    = hDev;

    if (pEndPtDescriptor) {
        PktsPerFrame = (pEndPtDescriptor->wMaxPacketSize & 0x1800) >> 11;
        PktsPerFrame++;

        DscLen     = pEndPtDescriptor->bLength;
        DscType    = pEndPtDescriptor->bDescriptorType;
        Address    = pEndPtDescriptor->bEndpointAddress;
        Attributes = pEndPtDescriptor->bmAttributes;
        MaxPktSize = (pEndPtDescriptor->wMaxPacketSize & 0x7ff) * PktsPerFrame;
        Interval   = pEndPtDescriptor->bInterval;
        bIn        = ((Address & 0x80) == 0x80);
    }

    TimeOut   = 10000;  // 10 Sec timeout is default

    XferMode  = XMODE_DIRECT;  // Normally, use Direct xfers

    if(SSEndPtDescriptor != NULL)
    {
        ssdscLen =SSEndPtDescriptor->bLength;
        ssdscType =SSEndPtDescriptor->bDescriptorType;
        ssmaxburst=SSEndPtDescriptor->bMaxBurst; /* Maximum number of packets endpoint can send in one burst*/
        MaxPktSize *=(ssmaxburst+1);
        ssbmAttribute=SSEndPtDescriptor->bmAttributes; // store endpoint attribute like for bulk it will be number of streams
        if((Attributes & 0x03) ==1) // MULT is valid for Isochronous transfer only
            MaxPktSize*=((SSEndPtDescriptor->bmAttributes & 0x03)+1); // Adding the MULT fields.

        ssbytesperinterval=SSEndPtDescriptor->bBytesPerInterval;

    }
    else
    {
        // Initialize the SS companion descriptor with zero
        ssdscLen =0;
        ssdscType =0;
        ssmaxburst=0; /* Maximum number of packets endpoint can send in one burst*/
        ssbmAttribute=0; // store endpoint attribute like for bulk it will be number of streams
        ssbytesperinterval=0;
    }

}


//______________________________________________________________________________

bool CCyUSBEndPoint::Reset(void)
{
    DWORD dwBytes = 0;
    bool  RetVal = false;

    RetVal  =  (DeviceIoControl(hDevice,
        IOCTL_ADAPT_RESET_PIPE,
        &Address,
        sizeof(UCHAR),
        NULL,
        0,
        &dwBytes,
        0) !=0);

    if(!RetVal) LastError = GetLastError();
    return RetVal;
}

//______________________________________________________________________________

bool CCyUSBEndPoint::Abort(void)
{
	DWORD dwBytes = 0;
	bool  RetVal = false;
	OVERLAPPED ov;

	memset(&ov,0,sizeof(ov));
	ov.hEvent = CreateEvent(NULL,false,false,NULL);

	RetVal  =    (DeviceIoControl(hDevice,
		IOCTL_ADAPT_ABORT_PIPE,
		&Address,
		sizeof(UCHAR),
		NULL,
		0,
		&dwBytes,
		&ov)!=0);
	if(!RetVal)
	{
		DWORD LastError = GetLastError();
		if(LastError == ERROR_IO_PENDING)
			WaitForSingleObject(ov.hEvent,1000);
	}
	CloseHandle(ov.hEvent);
	return true;

}
//______________________________________________________________________________

bool CCyUSBEndPoint::XferData(PUCHAR buf, LONG &bufLen, CCyIsoPktInfo* pktInfos)
{

    OVERLAPPED ovLapStatus;
    memset(&ovLapStatus,0,sizeof(OVERLAPPED));

    ovLapStatus.hEvent = CreateEvent(NULL, false, false, NULL);

    PUCHAR context = BeginDataXfer(buf, bufLen, &ovLapStatus);
    bool   wResult = WaitForIO(&ovLapStatus);
    bool   fResult = FinishDataXfer(buf, bufLen, &ovLapStatus, context, pktInfos);

    CloseHandle(ovLapStatus.hEvent);

    return wResult && fResult;
}

//______________________________________________________________________________

bool CCyUSBEndPoint::XferData(PUCHAR buf, LONG &bufLen, CCyIsoPktInfo* pktInfos, bool pktMode)
{
    if ((bIn == false) || (pktMode == false))
    {
        return XferData(buf, bufLen);
    }
    else
    {
        int size = 0;
        LONG xferLen = (LONG)MaxPktSize;
        bool status = true;
        PUCHAR ptr = buf;

        while (status && (size < bufLen))
        {
            if ((bufLen - size) < MaxPktSize)
                xferLen = bufLen - size;

            status = XferData(ptr, xferLen);
            if (status)
            {
                ptr += xferLen;
                size += xferLen;

                if (xferLen < MaxPktSize)
                    break;
            }
        }

        bufLen = size;
        if (bufLen > 0)
            return true;
        return status;
    }
}

//______________________________________________________________________________

bool CCyUSBEndPoint::FinishDataXfer(PUCHAR buf, LONG &bufLen, OVERLAPPED *ov, PUCHAR pXmitBuf, CCyIsoPktInfo* pktInfos)
{
    DWORD bytes = 0;
    bool rResult = (GetOverlappedResult(hDevice, ov, &bytes, FALSE)!=0);

    PSINGLE_TRANSFER pTransfer = (PSINGLE_TRANSFER) pXmitBuf;
    bufLen = (bytes) ? bytes - pTransfer->BufferOffset : 0;
    bytesWritten = bufLen;

    UsbdStatus = pTransfer->UsbdStatus;
    NtStatus   = pTransfer->NtStatus;

    if (bIn && (XferMode == XMODE_BUFFERED) && (bufLen > 0)) {
        UCHAR *ptr = (PUCHAR)pTransfer + pTransfer->BufferOffset;
        memcpy (buf, ptr, bufLen);
    }

    // If a buffer was provided, pass-back the Isoc packet info records
    if (pktInfos && (bufLen > 0)) {
        ZeroMemory(pktInfos, pTransfer->IsoPacketLength);
        PUCHAR pktPtr = pXmitBuf + pTransfer->IsoPacketOffset;
        memcpy(pktInfos, pktPtr, pTransfer->IsoPacketLength);
    }

    delete[] pXmitBuf;     // [] Changed in 1.5.1.3

    return rResult && (UsbdStatus == 0) && (NtStatus == 0);
}

//______________________________________________________________________________

PUCHAR CCyUSBEndPoint::BeginBufferedXfer(PUCHAR buf, LONG bufLen, OVERLAPPED *ov)
{
    if ( hDevice == INVALID_HANDLE_VALUE ) return NULL;

    int iXmitBufSize = sizeof (SINGLE_TRANSFER) + bufLen;
    PUCHAR pXmitBuf = new UCHAR[iXmitBufSize];
    ZeroMemory (pXmitBuf, iXmitBufSize);

    PSINGLE_TRANSFER pTransfer = (PSINGLE_TRANSFER) pXmitBuf;
    pTransfer->ucEndpointAddress = Address;
    pTransfer->IsoPacketLength = 0;
    pTransfer->BufferOffset = sizeof (SINGLE_TRANSFER);
    pTransfer->BufferLength = bufLen;

    // Copy buf into pXmitBuf
    UCHAR *ptr = (PUCHAR) pTransfer + pTransfer->BufferOffset;
    memcpy (ptr, buf, bufLen);

    DWORD dwReturnBytes;

    DeviceIoControl (hDevice,
        IOCTL_ADAPT_SEND_NON_EP0_TRANSFER,
        pXmitBuf,
        iXmitBufSize,
        pXmitBuf,
        iXmitBufSize,
        &dwReturnBytes,
        ov);

    UsbdStatus = pTransfer->UsbdStatus;
    NtStatus   = pTransfer->NtStatus;

    LastError = GetLastError();
    return pXmitBuf;
}



//______________________________________________________________________________
PUCHAR CCyUSBEndPoint::BeginDirectXfer(PUCHAR buf, LONG bufLen, OVERLAPPED *ov)
{
    if ( hDevice == INVALID_HANDLE_VALUE ) return NULL;

    int iXmitBufSize = sizeof (SINGLE_TRANSFER);
    PUCHAR pXmitBuf = new UCHAR[iXmitBufSize];
    ZeroMemory (pXmitBuf, iXmitBufSize);

    PSINGLE_TRANSFER pTransfer = (PSINGLE_TRANSFER) pXmitBuf;
    pTransfer->ucEndpointAddress = Address;
    pTransfer->IsoPacketLength = 0;
    pTransfer->BufferOffset = 0;
    pTransfer->BufferLength = 0;

    DWORD dwReturnBytes;
    DeviceIoControl (hDevice,
        IOCTL_ADAPT_SEND_NON_EP0_DIRECT,
        pXmitBuf,
        iXmitBufSize,
        buf,
        bufLen,
        &dwReturnBytes,
        ov);

    UsbdStatus = pTransfer->UsbdStatus;
    NtStatus   = pTransfer->NtStatus;

    LastError = GetLastError();
    return pXmitBuf;
}



//______________________________________________________________________________
// Unlike WaitForIO, this method does not record the bytes written or reset the
// endpoint if errors occurred.  This method is called from the application level
// in conjunction with BeginDataXfer and FinishDataXfer

bool CCyUSBEndPoint::WaitForXfer(OVERLAPPED *ov, ULONG tOut)
{

    //if (LastError == ERROR_SUCCESS) return true;  // The command completed

    //if (LastError == ERROR_IO_PENDING)
    {
        DWORD waitResult = WaitForSingleObject(ov->hEvent,tOut);

        if (waitResult == WAIT_TIMEOUT)  return false;

        if (waitResult == WAIT_OBJECT_0) return true;
    }

    return false;
}

//______________________________________________________________________________

bool CCyUSBEndPoint::WaitForIO(OVERLAPPED *ovLapStatus)
{
    LastError = GetLastError();
	DWORD retcode =1;

    if (LastError == ERROR_SUCCESS) return true;  // The command completed

    if (LastError == ERROR_IO_PENDING) {
        DWORD waitResult = WaitForSingleObject(ovLapStatus->hEvent,TimeOut);

        if (waitResult == WAIT_OBJECT_0) return true;

        if (waitResult == WAIT_TIMEOUT)
		{
			Abort();
			//// Wait for the stalled command to complete - should be done already
			//retcode = WaitForSingleObject(ovLapStatus->hEvent,50); // Wait for 50 milisecond
            Sleep(50);

			//if(retcode == WAIT_TIMEOUT || retcode==WAIT_FAILED)
			//{// Worst case condition , in multithreaded environment if user set time out to ZERO and cancel the IO the requiest, rarely first Abort() fail to cancel the IO, so reissueing second Abort(0.
				//Abort();
				//retcode = WaitForSingleObject(ovLapStatus->hEvent,INFINITE);

			//}
        }
    }

	return (retcode==0) ? true:false;
}

//______________________________________________________________________________

ULONG  CCyUSBEndPoint::GetXferSize(void){

    ULONG ulTransferSize;
    DWORD BytesXfered;

    if (hDevice == INVALID_HANDLE_VALUE) return (ULONG) NULL;

    SET_TRANSFER_SIZE_INFO SetTransferInfo;

    ulTransferSize = 0;
    SetTransferInfo.EndpointAddress = Address;
    SetTransferInfo.TransferSize = ulTransferSize;

    BOOL bRetVal = DeviceIoControl(hDevice,
        IOCTL_ADAPT_GET_TRANSFER_SIZE,
        &SetTransferInfo, sizeof(SET_TRANSFER_SIZE_INFO),
        &SetTransferInfo, sizeof(SET_TRANSFER_SIZE_INFO),
        &BytesXfered,
        NULL);

    if (bRetVal && BytesXfered >= sizeof(SET_TRANSFER_SIZE_INFO))
        ulTransferSize = SetTransferInfo.TransferSize;
    else
        LastError = GetLastError();

    return ulTransferSize;
}

//______________________________________________________________________________

void   CCyUSBEndPoint::SetXferSize(ULONG xfer){

    if (hDevice == INVALID_HANDLE_VALUE) return;

    DWORD BytesXfered;
    SET_TRANSFER_SIZE_INFO SetTransferInfo;

	if(MaxPktSize==0)
		return;
    // Force a multiple of MaxPktSize
    ULONG pkts = (xfer % MaxPktSize) ? 1+(xfer / MaxPktSize) : (xfer / MaxPktSize);
    ULONG xferSize = pkts * MaxPktSize;

    SetTransferInfo.EndpointAddress = Address;
    SetTransferInfo.TransferSize = xferSize;

    if(DeviceIoControl(hDevice,
        IOCTL_ADAPT_SET_TRANSFER_SIZE,
        &SetTransferInfo, sizeof(SET_TRANSFER_SIZE_INFO),
        &SetTransferInfo, sizeof(SET_TRANSFER_SIZE_INFO),
        &BytesXfered,
        NULL)==false)
    {
        LastError = GetLastError();
    }

}



////////////////////////////////////////////////////////////////////////////////
//
// The CCyControlEndPoint Class
//
////////////////////////////////////////////////////////////////////////////////

CCyControlEndPoint::CCyControlEndPoint(void):CCyUSBEndPoint()
{

    Target = TGT_DEVICE;
    ReqType = REQ_VENDOR;
    Direction = DIR_TO_DEVICE;

    ReqCode = 0;
    Value = 0;
    Index = 0;

    XferMode = XMODE_BUFFERED;

}

// Copy Constructor
CCyControlEndPoint::CCyControlEndPoint(CCyControlEndPoint& ept):CCyUSBEndPoint(ept)
{

    Target = ept.Target;
    ReqType = ept.ReqType;
    Direction = ept.Direction;

    ReqCode = ept.ReqCode;
    Value = ept.Value;
    Index = ept.Index;

    XferMode = ept.XferMode;
}


CCyControlEndPoint::CCyControlEndPoint(HANDLE h, PUSB_ENDPOINT_DESCRIPTOR pEndPtDescriptor) :
CCyUSBEndPoint(h, pEndPtDescriptor)
{
    Target = TGT_DEVICE;
    ReqType = REQ_VENDOR;
    Direction = DIR_TO_DEVICE;

    ReqCode = 0;
    Value = 0;
    Index = 0;

    XferMode = XMODE_BUFFERED;

}

//______________________________________________________________________________

bool CCyControlEndPoint::Read(PUCHAR buf, LONG &bufLen)
{
    Direction = DIR_FROM_DEVICE;
    return XferData(buf, bufLen);
}


//______________________________________________________________________________

bool CCyControlEndPoint::Write(PUCHAR buf, LONG &bufLen)
{
    Direction = DIR_TO_DEVICE;
    return XferData(buf, bufLen);
}


//______________________________________________________________________________

PUCHAR CCyControlEndPoint::BeginDataXfer(PUCHAR buf, LONG bufLen, OVERLAPPED *ov)
{
    union {
        struct {
            UCHAR Recipient:5;
            UCHAR Type:2;
            UCHAR Direction:1;
        } bmRequest;

        UCHAR bmReq;
    };

    if ( hDevice == INVALID_HANDLE_VALUE ) return NULL;

    bmRequest.Recipient = Target;
    bmRequest.Type      = ReqType;
    bmRequest.Direction = Direction;

    bIn = (Direction == DIR_FROM_DEVICE);

    int iXmitBufSize = sizeof (SINGLE_TRANSFER) + bufLen;
    UCHAR *pXmitBuf = new UCHAR[iXmitBufSize];
    ZeroMemory (pXmitBuf, iXmitBufSize);

    // The Control Endpoint has a 1 sec resolution on its timeout
    // But, TimeOut is in milliseconds.
    ULONG tmo = ((TimeOut > 0) && (TimeOut < 1000)) ? 1 : TimeOut / 1000;

    PSINGLE_TRANSFER pTransfer = (PSINGLE_TRANSFER) pXmitBuf;
    pTransfer->SetupPacket.bmRequest = bmReq;
    pTransfer->SetupPacket.bRequest = ReqCode;
    pTransfer->SetupPacket.wValue = Value;
    pTransfer->SetupPacket.wIndex = Index;
    pTransfer->SetupPacket.wLength = (USHORT)bufLen;
    pTransfer->SetupPacket.ulTimeOut = tmo;  // Seconds, not milliseconds
    pTransfer->ucEndpointAddress = 0x00;     // control pipe
    pTransfer->IsoPacketLength = 0;
    pTransfer->BufferOffset = sizeof (SINGLE_TRANSFER);
    pTransfer->BufferLength = bufLen;

    // Copy buf into pXmitBuf
    UCHAR *ptr = pXmitBuf + sizeof (SINGLE_TRANSFER);
    memcpy (ptr, buf, bufLen);

    DWORD dwReturnBytes;
    DeviceIoControl (hDevice,
        IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER,
        pXmitBuf,
        iXmitBufSize,
        pXmitBuf,
        iXmitBufSize,
        &dwReturnBytes,
        ov);

    // Note that this method leaves pXmitBuf allocated.  It will get deleted in
    // FinishDataXfer.

    LastError = GetLastError();
    return pXmitBuf;

}

////////////////////////////////////////////////////////////////////////////////
//
// The CCyIsocEndPoint Class
//
////////////////////////////////////////////////////////////////////////////////

CCyIsocEndPoint::CCyIsocEndPoint(void) : CCyUSBEndPoint()
{
}

CCyIsocEndPoint::CCyIsocEndPoint(HANDLE h, PUSB_ENDPOINT_DESCRIPTOR pEndPtDescriptor) :
CCyUSBEndPoint(h, pEndPtDescriptor)
{
}
CCyIsocEndPoint::CCyIsocEndPoint(HANDLE h, PUSB_ENDPOINT_DESCRIPTOR pEndPtDescriptor,USB_SUPERSPEED_ENDPOINT_COMPANION_DESCRIPTOR* SSEndPtDescriptor) :
CCyUSBEndPoint(h, pEndPtDescriptor,SSEndPtDescriptor)
{
}

//______________________________________________________________________________

PUCHAR CCyIsocEndPoint::BeginDataXfer(PUCHAR buf, LONG bufLen, OVERLAPPED *ov)
{
    if ( hDevice == INVALID_HANDLE_VALUE ) return NULL;

    if (XferMode == XMODE_DIRECT)
        return BeginDirectXfer(buf, bufLen, ov);
    else
        return BeginBufferedXfer(buf, bufLen, ov);

}

//______________________________________________________________________________

PUCHAR CCyIsocEndPoint::BeginBufferedXfer(PUCHAR buf, LONG bufLen, OVERLAPPED *ov)
{
    if ( hDevice == INVALID_HANDLE_VALUE ) return NULL;

	int pkts;
	if(MaxPktSize)
		pkts = bufLen / MaxPktSize;       // Number of packets implied by bufLen & pktSize
	else
	{
		pkts = 0;
		return NULL;
	}

    if (bufLen % MaxPktSize) pkts++;

    if (pkts == 0) return NULL;

    int iXmitBufSize = sizeof (SINGLE_TRANSFER) + (pkts * sizeof(ISO_PACKET_INFO)) + bufLen;
    UCHAR *pXmitBuf = new UCHAR[iXmitBufSize];
    ZeroMemory (pXmitBuf, iXmitBufSize);

    PSINGLE_TRANSFER pTransfer = (PSINGLE_TRANSFER) pXmitBuf;
    pTransfer->ucEndpointAddress = Address;
    pTransfer->IsoPacketOffset = sizeof (SINGLE_TRANSFER);
    pTransfer->IsoPacketLength = pkts * sizeof(ISO_PACKET_INFO);
    pTransfer->BufferOffset = sizeof (SINGLE_TRANSFER) + pTransfer->IsoPacketLength;
    pTransfer->BufferLength = bufLen;

    // Copy buf into pXmitBuf
    UCHAR *ptr = pXmitBuf + pTransfer->BufferOffset;
    memcpy (ptr, buf, bufLen);

    DWORD dwReturnBytes = 0;

    DeviceIoControl (hDevice,
        IOCTL_ADAPT_SEND_NON_EP0_TRANSFER,
        pXmitBuf,
        iXmitBufSize,
        pXmitBuf,
        iXmitBufSize,
        &dwReturnBytes,
        ov);

    // Note that this method leaves pXmitBuf allocated.  It will get deleted in
    // FinishDataXfer.

    UsbdStatus = pTransfer->UsbdStatus;
    NtStatus   = pTransfer->NtStatus;

    LastError = GetLastError();
    return pXmitBuf;
}

//______________________________________________________________________________

PUCHAR CCyIsocEndPoint::BeginDirectXfer(PUCHAR buf, LONG bufLen, OVERLAPPED *ov)
{
    if ( hDevice == INVALID_HANDLE_VALUE ) return NULL;

	int pkts;
	if(MaxPktSize)
		pkts = bufLen / MaxPktSize;       // Number of packets implied by bufLen & pktSize
	else
	{
		pkts = 0;
		return NULL;
	}

    if (bufLen % MaxPktSize) pkts++;

    if (pkts == 0) return NULL;

    int iXmitBufSize = sizeof (SINGLE_TRANSFER) + (pkts * sizeof(ISO_PACKET_INFO));
    UCHAR *pXmitBuf = new UCHAR[iXmitBufSize];
    ZeroMemory (pXmitBuf, iXmitBufSize);

    PSINGLE_TRANSFER pTransfer = (PSINGLE_TRANSFER) pXmitBuf;
    pTransfer->ucEndpointAddress = Address;
    pTransfer->IsoPacketOffset = sizeof (SINGLE_TRANSFER);
    pTransfer->IsoPacketLength = pkts * sizeof(ISO_PACKET_INFO);
    pTransfer->BufferOffset = 0;
    pTransfer->BufferLength = 0;

    DWORD dwReturnBytes = 0;
    DeviceIoControl (hDevice,
        IOCTL_ADAPT_SEND_NON_EP0_DIRECT,
        pXmitBuf,
        iXmitBufSize,
        buf,
        bufLen,
        &dwReturnBytes,
        ov);

    // Note that this method leaves pXmitBuf allocated.  It will get deleted in
    // FinishDataXfer.

    UsbdStatus = pTransfer->UsbdStatus;
    NtStatus   = pTransfer->NtStatus;

    LastError = GetLastError();
    return pXmitBuf;
}

////////////////////////////////////////////////////////////////////////////////
//
// The CCyFX3Device Class
//
////////////////////////////////////////////////////////////////////////////////
CCyFX3Device::CCyFX3Device()
{
}
CCyFX3Device::~CCyFX3Device()
{
}
bool CCyFX3Device::Ep0VendorCommand(vendorCmdData cmdData)
{
    ControlEndPt->Target    = TGT_DEVICE ;
    ControlEndPt->ReqType   = REQ_VENDOR ;
    ControlEndPt->ReqCode   = cmdData.opCode ;
    ControlEndPt->Direction = (cmdData.isRead) ? DIR_FROM_DEVICE : DIR_TO_DEVICE ;

    ControlEndPt->Value     = (cmdData.addr & 0xFFFF);
    ControlEndPt->Index     = ((cmdData.addr >> 16) & 0xFFFF);

    int maxpkt = ControlEndPt->MaxPktSize;

    long len = cmdData.size;

    /* Handle the case where transfer length is 0 (used to send the Program Entry) */
    if (cmdData.size == 0)
        return ControlEndPt->XferData(cmdData.buf, len, NULL);
    else
    {
        bool bRetCode = false;
        long Stagelen = 0;
        int BufIndex = 0;
        while (len > 0)
        {
            if (len >= 65535)
                Stagelen = 65535;
            else
                Stagelen = (len) % 65535;

            /* Allocate the buffer */
            PUCHAR StageBuf = new UCHAR[Stagelen];

            if (!cmdData.isRead)
            {
                /*write operation */
                for (int i = 0; i < Stagelen; i++)
                    StageBuf[i] = cmdData.buf[BufIndex + i];
            }

            bRetCode = ControlEndPt->XferData(StageBuf, Stagelen, NULL);
            if (!bRetCode)
			{
				if(StageBuf)
				  delete[] StageBuf;

				return false;
			}

            if (cmdData.isRead)
            {
                /*read operation */
                for (int i = 0; i < Stagelen; i++)
                    cmdData.buf[BufIndex + i] = StageBuf[i];
            }

			if(StageBuf)
				  delete[] StageBuf;

            len -= Stagelen;
            BufIndex += Stagelen;
        }

    }

    return true;
}

//______________________________________________________________________________

/* Function to transmit data on the control endpoint. */
bool CCyFX3Device::DownloadBufferToDevice(UINT start_addr, USHORT count, UCHAR *data_buf, UCHAR opCode)
{
    vendorCmdData cmdData;
    cmdData.addr = start_addr;

    cmdData.isRead = 0;
    cmdData.buf = data_buf;
    cmdData.opCode = opCode;
    cmdData.size = count;

    return Ep0VendorCommand(cmdData);
}

//______________________________________________________________________________

/* Function to read data using control endpoint. */
bool CCyFX3Device::UploadBufferFromDevice(UINT start_addr, USHORT count, UCHAR *data_buf, UCHAR opCode)
{
    vendorCmdData cmdData;
    cmdData.addr = start_addr;

    cmdData.isRead = 1;
    cmdData.buf = data_buf;
    cmdData.opCode = opCode;
    cmdData.size = count;

    return Ep0VendorCommand(cmdData);
}

//______________________________________________________________________________

/* This function sends out the 0xA0 vendor request for transferring the control to
program entry point.
*/
bool CCyFX3Device::SetProgramEntry (UCHAR opCode,UINT start_addr)
{
    vendorCmdData cmdData;
    cmdData.addr = ((start_addr >> 16) | (start_addr << 16)); /* swap LSB and MSB */

    cmdData.isRead = 0;
    cmdData.buf = NULL;
    cmdData.opCode = opCode;
    cmdData.size = 0;

    return Ep0VendorCommand(cmdData);
}

//______________________________________________________________________________

bool CCyFX3Device::IsBootLoaderRunning()
{
    /*Dire : in, Target : Device, ReqCode:0xA0,wValue:0x0000,wIndex:0x0000 */
    /* This function checks for bootloader,it will return false if it is not running. */
    UCHAR buf[1];
    long len = 1;
    bool ret;
	UCHAR opCode = 0xA0; // Vendore command

    vendorCmdData cmdData;

    cmdData.addr = 0x0000;

    cmdData.isRead = true;
    cmdData.buf = buf;
    cmdData.opCode = opCode;
    cmdData.size = len;

    /* Value = isErase, index = sector number */
    ret = Ep0VendorCommand(cmdData);
    return ret;
}

//______________________________________________________________________________

FX3_FWDWNLOAD_ERROR_CODE CCyFX3Device::EraseSectorOfSPIFlash(UINT SectorNumber, UCHAR opCode)
{
    vendorCmdData cmdData;
    bool ret;
    UCHAR buf[1];
    UINT buflen = 0;
    buf[0] = 1;

    cmdData.addr = (1 + (SectorNumber << 16));

    cmdData.isRead = false;
    cmdData.buf = buf;
    cmdData.opCode = opCode;
    cmdData.size = buflen;

    /* Value = isErase, index = sector number */
    ret = Ep0VendorCommand(cmdData);
    if (ret)
    {
        /* Check the status of erase. Value should be 0 */
        buflen = 1;
		cmdData.size = buflen;
        while (buf[0] != 0)
        {
            cmdData.isRead = true;
            cmdData.addr = (0 + (SectorNumber << 16));
            if (!Ep0VendorCommand(cmdData))
                return FAILED;
        }
    }
    else
        return FAILED;

    return SUCCESS;
}

//______________________________________________________________________________

bool CCyFX3Device::WriteToSPIFlash(PUCHAR Buf, UINT buflen, UINT ByteAddress, UCHAR opCode)
{
    vendorCmdData cmdData;
    cmdData.addr = ((ByteAddress / SPI_FLASH_PAGE_SIZE_IN_BYTE) << 16); // swap LSB and MSB

    cmdData.isRead = 0;
    cmdData.buf = Buf;
    cmdData.opCode = opCode;
    cmdData.size = buflen;

    return Ep0VendorCommand(cmdData);
}

//______________________________________________________________________________

FX3_FWDWNLOAD_ERROR_CODE CCyFX3Device::DownloadUserIMGtoSPIFLASH(PUCHAR buffer_p, UINT fw_size, UCHAR opCode)
{
    /* The size of the image needs to be rounded to a multiple of the SPI page size. */
    UINT ImageSizeInPage = (fw_size + SPI_FLASH_PAGE_SIZE_IN_BYTE - 1) / SPI_FLASH_PAGE_SIZE_IN_BYTE;
    UINT TotalNumOfByteToWrote = ImageSizeInPage * SPI_FLASH_PAGE_SIZE_IN_BYTE;
    /* Sectors needs to be erased in case of SPI. Sector size = 64k. Page Size = 256 bytes. 1 Sector = 256 pages. */
    /* Calculate the number of sectors needed to write firmware image and erase them. */
    UINT NumOfSector = fw_size / SPI_FLASH_SECTOR_SIZE_IN_BYTE;
    if ((fw_size % SPI_FLASH_SECTOR_SIZE_IN_BYTE) != 0)
        NumOfSector++;

    /* Erase the sectors */
    for (UINT i = 0; i < NumOfSector; i++)
    {
        if (EraseSectorOfSPIFlash(i, opCode) != SUCCESS)
            return SPILASH_ERASE_FAILED;
    }

    /*Write the firmware to the SPI flash */
    UINT numberOfBytesLeftToWrite = TotalNumOfByteToWrote; /* Current number of bytes left to write */

    UINT FwFilePointer = 0;
    UINT massStorageByteAddress = 0; /* Current Mass Storage Byte Address  */

    PUCHAR WriteBuf = new UCHAR[CYWB_BL_MAX_BUFFER_SIZE_WHEN_USING_EP0_TRANSPORT];

    while (numberOfBytesLeftToWrite > 0)
    {
        UINT numberOfBytesToWrite = CYWB_BL_MAX_BUFFER_SIZE_WHEN_USING_EP0_TRANSPORT;

        if (numberOfBytesLeftToWrite < CYWB_BL_MAX_BUFFER_SIZE_WHEN_USING_EP0_TRANSPORT)
        {
            numberOfBytesToWrite = numberOfBytesLeftToWrite;
        }

        /* Trigger a mass storage write... */
        for (UINT i = 0; i < numberOfBytesToWrite; i++)
        {
            if ((FwFilePointer + i) < fw_size)
                WriteBuf[i] = buffer_p[i + FwFilePointer];
        }
		opCode = 0xC2; // Operation code to Write to SPI
        if (WriteToSPIFlash(WriteBuf, numberOfBytesToWrite, massStorageByteAddress, opCode) == false)
		{
			if(WriteBuf)
				delete[] WriteBuf;

            return FAILED;
		}

        /* Adjust pointers */
        numberOfBytesLeftToWrite -= numberOfBytesToWrite;
        FwFilePointer += numberOfBytesToWrite;
        massStorageByteAddress += numberOfBytesToWrite;
    }

	if(WriteBuf)
				delete[] WriteBuf;

    return SUCCESS;
}

//______________________________________________________________________________

FX3_FWDWNLOAD_ERROR_CODE CCyFX3Device::DownloadUserIMGtoI2CE2PROM(PUCHAR buffer_p, UINT fw_size, UCHAR opCode)
{
    int STAGE_SIZE = BUFSIZE_UPORT;
    PUCHAR downloadbuf = new UCHAR[STAGE_SIZE];

    int NoOfStage = ((int)fw_size / STAGE_SIZE);
    long LastStage = ((int)fw_size % STAGE_SIZE);
    UINT DownloadAddress = 0;
    long FwImagePtr = 0;
    long StageSize = STAGE_SIZE;

	//Get the I2C addressing size
    UCHAR ImgI2CSizeByte = buffer_p[2]; // the 2nd byte of the IMG file will tell us the I2EPROM internal addressing.
    UINT AddresingStageSize = 0;
    ImgI2CSizeByte = ((ImgI2CSizeByte >> 1) & 0x07); // Bit3:1 represent the addressing
    bool IsMicroShipE2Prom = false;

    switch (ImgI2CSizeByte)
    {
        case 0:
        case 1:
            return I2CE2PROM_UNKNOWN_I2C_SIZE;
        case 2:
            AddresingStageSize = (4 * 1024); // 4KByte
            break;
        case 3:
            AddresingStageSize = (8 * 1024); // 8KByte
            break;
        case 4:
            AddresingStageSize = (16 * 1024); // 16KByte
            break;
        case 5:
            AddresingStageSize = (32 * 1024); // 32KByte
            break;
        case 6:
            AddresingStageSize = (64 * 1024); // 64KByte
            break;
        case 7:
            IsMicroShipE2Prom = true; // 128KByte Addressing for Microchip.
            AddresingStageSize = (64 * 1024); // 64KByte // case 7 represent 128Kbyte but it follow 64Kbyte addressing
            break;
        default:
			{
				if(downloadbuf)
					delete[] downloadbuf;

				return I2CE2PROM_UNKNOWN_I2C_SIZE;
			}
    }


    ControlEndPt->Target    = TGT_DEVICE ;
    ControlEndPt->ReqType   = REQ_VENDOR ;
    ControlEndPt->ReqCode   = opCode ;
    ControlEndPt->Direction = DIR_TO_DEVICE ;

    ControlEndPt->Value     = (DownloadAddress & 0xFFFF);
    ControlEndPt->Index     = ((DownloadAddress >> 16) & 0xFFFF);
    int maxpkt = ControlEndPt->MaxPktSize;

    for (int i = 0; i < NoOfStage; i++)
    {
        /* Copy data from main buffer to tmp buffer */
        for (long j = 0; j < STAGE_SIZE; j++)
            downloadbuf[j] = buffer_p[FwImagePtr + j];

        if (!ControlEndPt->XferData(downloadbuf, StageSize, NULL))
		{
			if(downloadbuf)
					delete[] downloadbuf;

            return FAILED;
		}

        FwImagePtr += STAGE_SIZE;
        ControlEndPt->Index += (WORD)StageSize;

		/////
		// Address calculation done in the below box
        if (IsMicroShipE2Prom)
        {//Microchip Addressing(0-(1-64),4(64 to 128),1(128 to 192 ),5(192 to 256))
            if (FwImagePtr >= (128 * 1024))
            {
                if ((FwImagePtr % AddresingStageSize) == 0)
                {
                    if (ControlEndPt->Value == 0x04)
                        ControlEndPt->Value = 0x01;
                    else
                        ControlEndPt->Value = 0x05;

                    ControlEndPt->Index = 0;
                }
            }
            else if ((FwImagePtr % AddresingStageSize) == 0)
            {
                ControlEndPt->Value = 0x04;
                ControlEndPt->Index = 0;
            }
        }
        else
        {//ATMEL addressing sequential
            if ((FwImagePtr % AddresingStageSize)==0)
            {// Increament the Value field to represent the address and reset the Index value to zero.
                ControlEndPt->Value += 0x01;
                if(ControlEndPt->Value>=8)
                    ControlEndPt->Value = 0x0; //reset the Address to ZERO

                ControlEndPt->Index = 0;
            }
        }
		/////


    }

    if (LastStage != 0)
    {
        /*check for last stage */

        for (long j = 0; j < LastStage; j++)
            downloadbuf[j] = buffer_p[FwImagePtr + j];

        if ((LastStage % maxpkt) != 0)
        {
            /* make it multiple of max packet size */
            int diff = (maxpkt - (LastStage % maxpkt));
            for (int j = LastStage; j < (LastStage + diff); j++)
                downloadbuf[j] = 0;

            LastStage += diff;
        }

        if (!ControlEndPt->XferData(downloadbuf, LastStage, NULL))
		{
			if(downloadbuf)
					delete[] downloadbuf;

            return FAILED;
		}

		/*Failure Case:
                 The device does not return failure message when file size is more than 128KByte and only one 128Byte E2PROM on the DVK.
                 Solution:
                 Read back the last stage data to confirm that all data transferred successfully.*/
         ControlEndPt->ReqCode = 0xBB;
         ControlEndPt->Direction = DIR_FROM_DEVICE;
         if (!ControlEndPt->XferData(downloadbuf, LastStage, NULL))
         {
			 if(downloadbuf)
					delete[] downloadbuf;

               return FAILED;
         }
    }

	if(downloadbuf)
		delete[] downloadbuf;

    return SUCCESS;
}

//______________________________________________________________________________

FX3_FWDWNLOAD_ERROR_CODE CCyFX3Device::DownloadFwToRam(PUCHAR buffer_p, UINT fw_size, UCHAR opCode)
{
    UCHAR downloadBuffer[BUFSIZE_UPORT];
    UCHAR uploadbuffer[BUFSIZE_UPORT];
    bool isTrue = true;
    UINT ComputeCheckSum = 0;
    UINT ExpectedCheckSum = 0;
    INT fwImagePtr = 0;
    UINT SectionLength = 0;
    UINT SectionAddress;
    UINT DownloadAddress;
    UINT ProgramEntry;

    /* Initialize computed checksum */
    ComputeCheckSum = 0;

    /* Check "CY" signature (0x43,0x59) and download the firmware image */
    if ((buffer_p[fwImagePtr] != 0x43) || (buffer_p[fwImagePtr + 1] != 0x59))
    {	/*signature doesn't match */
        return INVALID_FWSIGNATURE;
    }

    /* Skip the two bytes signature and the following two bytes */
    fwImagePtr += 4;

    /* Download one section at a time to the device, compute checksum, and upload-verify it */
    while (isTrue)
    {
        /* Get section length (4 bytes) and convert it from 32-bit word count to byte count */
        CYWB_BL_4_BYTES_COPY(&SectionLength, &buffer_p[fwImagePtr]);
        fwImagePtr += 4;

        SectionLength = SectionLength << 2;

        /* If length = 0, the transfer is complete */
        if (SectionLength == 0) break;

        /* Get section address (4 bytes) */
        CYWB_BL_4_BYTES_COPY(&SectionAddress, &buffer_p[fwImagePtr]);
        fwImagePtr += 4;

        /* Download BUFSIZE_UPORT maximum bytes at a time */
        INT bytesLeftToDownload = SectionLength;
        DownloadAddress = SectionAddress;

        while (bytesLeftToDownload > 0)
        {
            INT bytesToTransfer = BUFSIZE_UPORT;
            if (bytesLeftToDownload < BUFSIZE_UPORT)
                bytesToTransfer = bytesLeftToDownload;

            /* sanity check for incomplete fw with valid signatures.
            Note: bytesToTransfer should never be greater then fw length i.e buflen */
            if (bytesToTransfer > (INT)fw_size)
                return CORRUPT_FIRMWARE_IMAGE_FILE;

            memcpy(downloadBuffer, (void *)(buffer_p + fwImagePtr), bytesToTransfer);

            /* Compute checksum: Here transferLength is assumed to be a multiple of 4. If it is not, the checksum will fail anyway */
            for (INT index = 0; index < bytesToTransfer; index += 4)
            {
                UINT buf32bits = 0;
                CYWB_BL_4_BYTES_COPY(&buf32bits, &downloadBuffer[index]);
                ComputeCheckSum += buf32bits;
            }

            /*The FPGA does not seem to always be reliable: if an error is encountered, try again twice */
            INT maxTryCount = 3;
            for (INT tryCount = 1; tryCount <= maxTryCount; tryCount++)
            {
                /* Download one buffer worth of data to the device */
                if (!DownloadBufferToDevice(DownloadAddress, bytesToTransfer, downloadBuffer, opCode))
                {
                    /* Check if we exceeded the max try count */
                    if (tryCount == maxTryCount)
                    {
                        /* Failure while downloading firmware to the device. Abort */
                        return FAILED;
                    }
                    else
                    {
                        /* F/W buffer download failure. Trying writing/verifying current buffer again... */
                        continue;
                    }
                }

                memset (uploadbuffer, 0, bytesToTransfer);

                if (!UploadBufferFromDevice(DownloadAddress, bytesToTransfer, uploadbuffer, opCode))
                {
                    /* Check if we exceeded the max try count */
                    if (tryCount == maxTryCount)
                    {
                        /* Failure while uploading firmware from the device for verification. Abort */
                        return FAILED;
                    }
                    else
                    {
                        /*  F/W buffer upload failure. Trying writing/verifying current buffer again... */
                        continue;
                    }
                }

                for (int count = 0; count < bytesToTransfer; count++)
                {
                    if (downloadBuffer[count] != uploadbuffer[count])
                    {
                        /* Check if we exceeded the max try count */
                        if (tryCount == maxTryCount)
                        {
                            /* Uploaded firmware data does not match downloaded data. Abort */
                            return FAILED;
                        }
                        else
                        {
                            /* Uploaded data does not match downloaded data. Trying writing/verifying current buffer again...*/
                            continue;
                        }
                    }
                }
            }

            DownloadAddress += bytesToTransfer;
            fwImagePtr += bytesToTransfer;
            bytesLeftToDownload -= bytesToTransfer;

            /* Sanity check */
            if (fwImagePtr > (INT)fw_size)
                return INCORRECT_IMAGE_LENGTH;
        }
    }

    /* Get Program Entry Address(4 bytes) */
    CYWB_BL_4_BYTES_COPY(&ProgramEntry, &buffer_p[fwImagePtr]);
    fwImagePtr += 4;

    /* Get expected checksum (4 bytes) */
    CYWB_BL_4_BYTES_COPY(&ExpectedCheckSum, &buffer_p[fwImagePtr]);
    fwImagePtr += 4;

    /* Compare computed checksum against expected value */
    if (ComputeCheckSum != ExpectedCheckSum)
    {
        /* CheckSum mismatch. Expected=0x" << std::hex << expectedCheckSum << " Computed=0x" << std::hex << computedCheckSum; */
    }

    /* Transfer execution to Program Entry */
    UCHAR dummyBuffer[1];
	// Few of the xHCI driver stack have issue with Control IN transfer,due to that below request fails ,
	// Control IN transfer is ZERO lenght packet , and it does not have any impact on execution of firmware.
    //if (!DownloadBufferToDevice(ProgramEntry, 0, dummyBuffer, opCode))
    //{
    //    /* Downloading Program Entry failed */
    //    return FAILED;
    //}
	DownloadBufferToDevice(ProgramEntry, 0, dummyBuffer, opCode);

    return SUCCESS;
}

//______________________________________________________________________________

FX3_FWDWNLOAD_ERROR_CODE CCyFX3Device::DownloadFw(char *fileName, FX3_FWDWNLOAD_MEDIA_TYPE enMediaType)
{
    UINT fwSize = 0;
    PUCHAR FwImage;
    FILE *FwImagePtr;
    int error;

//  error = fopen_s(&FwImagePtr, fileName, "rb");
    FwImagePtr = fopen( fileName, "rb");
    if (FwImagePtr == NULL)
        return INVALID_FILE;

    /* Find the length of the image */
    fseek (FwImagePtr, 0, SEEK_END);
    fwSize = ftell(FwImagePtr);
    fseek (FwImagePtr, 0, SEEK_SET);

    /* Allocate memory for the image */
    FwImage =  new unsigned char[fwSize];

    if (FwImage == NULL)
        return INVALID_FILE;

    if (fwSize <= 0)
    {
        fclose (FwImagePtr);
        return INVALID_FILE;
    }

    /* Read into buffer */
    fread (FwImage, fwSize, 1, FwImagePtr);
    fclose (FwImagePtr);

	FX3_FWDWNLOAD_ERROR_CODE ErroCode = SUCCESS;
    // call api to download the image
    if (enMediaType == RAM)
        ErroCode = DownloadFwToRam(FwImage, fwSize, 0xA0);
    else if (enMediaType == I2CE2PROM)
            ErroCode = DownloadUserIMGtoI2CE2PROM(FwImage, fwSize, 0xBA);
    else if (enMediaType == SPIFLASH)
        ErroCode = DownloadUserIMGtoSPIFLASH(FwImage, fwSize, 0xC4);
    else
        ErroCode = INVALID_MEDIA_TYPE;

	if(FwImage)
		delete[] FwImage;

	return ErroCode;
}

//______________________________________________________________________________

CCyIsoPktInfo* CCyIsocEndPoint::CreatePktInfos(LONG bufLen, int &packets)
{
	if(MaxPktSize==0)
		return NULL;

    packets = bufLen / MaxPktSize;       // Number of packets implied by bufLen & pktSize
    if (bufLen % MaxPktSize) packets++;

    if (packets) {
        CCyIsoPktInfo *isoPktInfos = new CCyIsoPktInfo[packets];
        return isoPktInfos;
    } else
        return NULL;

}

////////////////////////////////////////////////////////////////////////////////
//
// The CCyBulkEndPoint Class
//
////////////////////////////////////////////////////////////////////////////////

CCyBulkEndPoint::CCyBulkEndPoint(void) : CCyUSBEndPoint()
{
}

CCyBulkEndPoint::CCyBulkEndPoint(HANDLE h, PUSB_ENDPOINT_DESCRIPTOR pEndPtDescriptor) :
CCyUSBEndPoint(h, pEndPtDescriptor)
{

}
CCyBulkEndPoint::CCyBulkEndPoint(HANDLE h, PUSB_ENDPOINT_DESCRIPTOR pEndPtDescriptor,USB_SUPERSPEED_ENDPOINT_COMPANION_DESCRIPTOR* SSEndPtDescriptor):
CCyUSBEndPoint(h, pEndPtDescriptor,SSEndPtDescriptor)
{

}
//______________________________________________________________________________

PUCHAR CCyBulkEndPoint::BeginDataXfer(PUCHAR buf, LONG bufLen, OVERLAPPED *ov)
{
    if ( hDevice == INVALID_HANDLE_VALUE ) return NULL;

    if (XferMode == XMODE_DIRECT)
        return BeginDirectXfer(buf, bufLen, ov);
    else
        return BeginBufferedXfer(buf, bufLen, ov);
}



////////////////////////////////////////////////////////////////////////////////
//
// The CCyInterruptEndPoint Class
//
////////////////////////////////////////////////////////////////////////////////

CCyInterruptEndPoint::CCyInterruptEndPoint(void) : CCyUSBEndPoint()
{
}

CCyInterruptEndPoint::CCyInterruptEndPoint(HANDLE h, PUSB_ENDPOINT_DESCRIPTOR pEndPtDescriptor) :
CCyUSBEndPoint(h, pEndPtDescriptor)
{

}
CCyInterruptEndPoint::CCyInterruptEndPoint(HANDLE h, PUSB_ENDPOINT_DESCRIPTOR pEndPtDescriptor,USB_SUPERSPEED_ENDPOINT_COMPANION_DESCRIPTOR* SSEndPtDescriptor):
CCyUSBEndPoint(h, pEndPtDescriptor,SSEndPtDescriptor)
{
}

//______________________________________________________________________________

PUCHAR CCyInterruptEndPoint::BeginDataXfer(PUCHAR buf, LONG bufLen, OVERLAPPED *ov)
{
    if ( hDevice == INVALID_HANDLE_VALUE ) return NULL;

    if (XferMode == XMODE_DIRECT)
        return BeginDirectXfer(buf, bufLen, ov);
    else
        return BeginBufferedXfer(buf, bufLen, ov);

}
CCyUSBBOS::CCyUSBBOS(void)
{
	 // initialize to null
    pUSB20_DeviceExt = NULL;
    pSS_DeviceCap = NULL;
    pContainer_ID = NULL;
}
CCyUSBBOS::~CCyUSBBOS()
{
	if(pContainer_ID)
	{
		delete pContainer_ID;
		pContainer_ID = NULL;
	}
	if(pUSB20_DeviceExt)
	{
		delete pUSB20_DeviceExt;
		pUSB20_DeviceExt = NULL;
	}
	if(pSS_DeviceCap)
	{
		delete pSS_DeviceCap;
		pSS_DeviceCap = NULL;
	}
}

CCyUSBBOS::CCyUSBBOS(HANDLE h,PUSB_BOS_DESCRIPTOR pBosDescrData)
{
    // initialize to null
    pUSB20_DeviceExt = NULL;
    pSS_DeviceCap = NULL;
    pContainer_ID = NULL;

    bLength = pBosDescrData->bLength;
    bDescriptorType = pBosDescrData->bDescriptorType;
    bNumDeviceCaps = pBosDescrData->bNumDeviceCaps;
    wTotalLength = pBosDescrData->wTotalLength;

    int totallen = wTotalLength;
    totallen -=pBosDescrData->bLength;

    if (totallen < 0)
        return;

    UCHAR*  DevCap = (byte*)((byte*)pBosDescrData + pBosDescrData->bLength); // get nex descriptor

    for (int i = 0; i < bNumDeviceCaps; i++)
    {
        //check capability type
        switch (DevCap[USB_BOS_DEVICE_CAPABILITY_TYPE_INDEX])
        {
        case USB_BOS_CAPABILITY_TYPE_USB20_EXT:
            {
                PUSB_BOS_USB20_DEVICE_EXTENSION pUSB20_ext = (PUSB_BOS_USB20_DEVICE_EXTENSION)DevCap;
                totallen -= pUSB20_ext->bLength;
                DevCap = (byte *)DevCap +pUSB20_ext->bLength;
                pUSB20_DeviceExt = new CCyBosUSB20Extesnion(h, pUSB20_ext);
                break;
            }
        case USB_BOS_CAPABILITY_TYPE_SUPERSPEED_USB:
            {
                PUSB_BOS_SS_DEVICE_CAPABILITY pSS_Capability = (PUSB_BOS_SS_DEVICE_CAPABILITY)DevCap;
                totallen -= pSS_Capability->bLength;
                DevCap = (byte*)DevCap + pSS_Capability->bLength;
                pSS_DeviceCap = new CCyBosSuperSpeedCapability(h, pSS_Capability);
                break;
            }
        case USB_BOS_CAPABILITY_TYPE_CONTAINER_ID:
            {
                PUSB_BOS_CONTAINER_ID pUSB_ContainerID = (PUSB_BOS_CONTAINER_ID)DevCap;
                totallen -= pUSB_ContainerID->bLength;
                DevCap = (byte*)DevCap + pUSB_ContainerID->bLength;
                pContainer_ID = new CCyBosContainerID(h, pUSB_ContainerID);
                break;
            }
        default:
            {
                break;
            }
        }
        if(totallen<0)
            break;
    }
}
CCyBosUSB20Extesnion::CCyBosUSB20Extesnion(void)
{}
CCyBosUSB20Extesnion::CCyBosUSB20Extesnion(HANDLE h,PUSB_BOS_USB20_DEVICE_EXTENSION pBosUsb20ExtDesc)
{
    bLength = pBosUsb20ExtDesc->bLength;
    bDescriptorType = pBosUsb20ExtDesc->bDescriptorType;
    bDevCapabilityType = pBosUsb20ExtDesc->bDevCapabilityType;
    bmAttribute = pBosUsb20ExtDesc->bmAttribute;
}
CCyBosSuperSpeedCapability::CCyBosSuperSpeedCapability(void)
{}
CCyBosSuperSpeedCapability::CCyBosSuperSpeedCapability(HANDLE h,PUSB_BOS_SS_DEVICE_CAPABILITY pUSB_SuperSpeedUsb)
{
    bLength = pUSB_SuperSpeedUsb->bLength;
    bDescriptorType = pUSB_SuperSpeedUsb->bDescriptorType;
    bDevCapabilityType = pUSB_SuperSpeedUsb->bDevCapabilityType;
    bFunctionalitySupporte = pUSB_SuperSpeedUsb->bFunctionalitySupporte;
    SpeedsSuported = pUSB_SuperSpeedUsb->wSpeedsSuported;
    bmAttribute = pUSB_SuperSpeedUsb->bmAttribute;
    bU1DevExitLat = pUSB_SuperSpeedUsb->bU1DevExitLat;
    bU2DevExitLat = pUSB_SuperSpeedUsb->bU2DevExitLat;
}
CCyBosContainerID::CCyBosContainerID(void)
{}
CCyBosContainerID::CCyBosContainerID(HANDLE handle, PUSB_BOS_CONTAINER_ID pUSB_ContainerID)
{
    bLength = pUSB_ContainerID->bLength;
    bDescriptorType = pUSB_ContainerID->bDescriptorType;
    bDevCapabilityType = pUSB_ContainerID->bDevCapabilityType;
    for (int i = 0; i < USB_BOS_CAPABILITY_TYPE_CONTAINER_ID_SIZE; i++)
        ContainerID[i] = pUSB_ContainerID->ContainerID[i];
}
