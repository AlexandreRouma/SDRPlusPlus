/*
 ## Cypress CyAPI C++ library header file (CyAPI.h)
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

#ifndef CyUSBH
#define CyUSBH

#include "cyusb30_def.h"

/* Data straucture for the Vendor request and data */
typedef struct vendorCmdData
{
    UCHAR   *buf;       /* Pointer to the data */
    UCHAR   opCode;     /* Vendor request code */
    UINT    addr;       /* Read/Write address */
    long    size;       /* Size of the read/write */
    bool    isRead;     /* Read or write */
} vendorCmdData ;

#ifndef   __USB200_H__
#define   __USB200_H__
#pragma pack(push,1)
typedef struct _USB_DEVICE_DESCRIPTOR {
    UCHAR   bLength;
    UCHAR   bDescriptorType;
    USHORT  bcdUSB;
    UCHAR   bDeviceClass;
    UCHAR   bDeviceSubClass;
    UCHAR   bDeviceProtocol;
    UCHAR   bMaxPacketSize0;
    USHORT  idVendor;
    USHORT  idProduct;
    USHORT  bcdDevice;
    UCHAR   iManufacturer;
    UCHAR   iProduct;
    UCHAR   iSerialNumber;
    UCHAR   bNumConfigurations;
} USB_DEVICE_DESCRIPTOR, *PUSB_DEVICE_DESCRIPTOR;

typedef struct _USB_ENDPOINT_DESCRIPTOR {
    UCHAR   bLength;
    UCHAR   bDescriptorType;
    UCHAR   bEndpointAddress;
    UCHAR   bmAttributes;
    USHORT  wMaxPacketSize;
    UCHAR   bInterval;
} USB_ENDPOINT_DESCRIPTOR, *PUSB_ENDPOINT_DESCRIPTOR;

typedef struct _USB_CONFIGURATION_DESCRIPTOR {
    UCHAR   bLength;
    UCHAR   bDescriptorType;
    USHORT  wTotalLength;
    UCHAR   bNumInterfaces;
    UCHAR   bConfigurationValue;
    UCHAR   iConfiguration;
    UCHAR   bmAttributes;
    UCHAR   MaxPower;
} USB_CONFIGURATION_DESCRIPTOR, *PUSB_CONFIGURATION_DESCRIPTOR;

typedef struct _USB_INTERFACE_DESCRIPTOR {
    UCHAR   bLength;
    UCHAR   bDescriptorType;
    UCHAR   bInterfaceNumber;
    UCHAR   bAlternateSetting;
    UCHAR   bNumEndpoints;
    UCHAR   bInterfaceClass;
    UCHAR   bInterfaceSubClass;
    UCHAR   bInterfaceProtocol;
    UCHAR   iInterface;
} USB_INTERFACE_DESCRIPTOR, *PUSB_INTERFACE_DESCRIPTOR;

typedef struct _USB_STRING_DESCRIPTOR {
    UCHAR   bLength;
    UCHAR   bDescriptorType;
    WCHAR   bString[1];
} USB_STRING_DESCRIPTOR, *PUSB_STRING_DESCRIPTOR;

typedef struct _USB_COMMON_DESCRIPTOR {
    UCHAR   bLength;
    UCHAR   bDescriptorType;
} USB_COMMON_DESCRIPTOR, *PUSB_COMMON_DESCRIPTOR;
#pragma pack(pop)
#endif

/*******************************************************************************/
class CCyIsoPktInfo {
public:
    LONG Status;
    LONG Length;
};

/*******************************************************************************/


/* {AE18AA60-7F6A-11d4-97DD-00010229B959} */

static GUID CYUSBDRV_GUID = {0xae18aa60, 0x7f6a, 0x11d4, {0x97, 0xdd, 0x0, 0x1, 0x2, 0x29, 0xb9, 0x59}};


typedef enum {TGT_DEVICE, TGT_INTFC, TGT_ENDPT, TGT_OTHER } CTL_XFER_TGT_TYPE;
typedef enum {REQ_STD, REQ_CLASS, REQ_VENDOR } CTL_XFER_REQ_TYPE;
typedef enum {DIR_TO_DEVICE, DIR_FROM_DEVICE } CTL_XFER_DIR_TYPE;
typedef enum {XMODE_BUFFERED, XMODE_DIRECT } XFER_MODE_TYPE;

const int MAX_ENDPTS = 32;
const int MAX_INTERFACES = 255;
const int USB_STRING_MAXLEN = 256;

#define BUFSIZE_UPORT 2048 //4096 - CDT 130492
typedef enum { RAM = 1, I2CE2PROM, SPIFLASH } FX3_FWDWNLOAD_MEDIA_TYPE ;
typedef enum { SUCCESS = 0, FAILED, INVALID_MEDIA_TYPE, INVALID_FWSIGNATURE, DEVICE_CREATE_FAILED, INCORRECT_IMAGE_LENGTH, INVALID_FILE, SPILASH_ERASE_FAILED, CORRUPT_FIRMWARE_IMAGE_FILE,I2CE2PROM_UNKNOWN_I2C_SIZE } FX3_FWDWNLOAD_ERROR_CODE;

#define CYWB_BL_4_BYTES_COPY(destination,source) {memcpy((void *)(destination), (void *)(source), 4);}

/********************************************************************************
*
* The CCyEndPoint ABSTRACT Class
*
********************************************************************************/
class CCyUSBEndPoint
{
protected:
    bool WaitForIO(OVERLAPPED *ovLapStatus);

    virtual PUCHAR BeginDirectXfer(PUCHAR buf, LONG bufLen, OVERLAPPED *ov);
    virtual PUCHAR BeginBufferedXfer(PUCHAR buf, LONG bufLen, OVERLAPPED *ov);

public:

    CCyUSBEndPoint(void);
    CCyUSBEndPoint(CCyUSBEndPoint& ept);
    CCyUSBEndPoint(HANDLE h, PUSB_ENDPOINT_DESCRIPTOR pEndPtDescriptor);
    CCyUSBEndPoint(HANDLE hDev, PUSB_ENDPOINT_DESCRIPTOR pEndPtDescriptor,USB_SUPERSPEED_ENDPOINT_COMPANION_DESCRIPTOR* SSEndPtDescriptor);

    HANDLE  hDevice;

    /* The fields of an EndPoint Descriptor */
    UCHAR   DscLen;
    UCHAR   DscType;
    UCHAR   Address;
    UCHAR   Attributes;
    USHORT  MaxPktSize;
    USHORT  PktsPerFrame;
    UCHAR   Interval;
    /* This are the fields for Super speed endpoint */
    UCHAR   ssdscLen;
    UCHAR   ssdscType;
    UCHAR   ssmaxburst;     /* Maximum number of packets endpoint can send in one burst */
    UCHAR   ssbmAttribute;  /* store endpoint attribute like for bulk it will be number of streams */
    USHORT  ssbytesperinterval;

    /* Other fields */
    ULONG   TimeOut;
    ULONG   UsbdStatus;
    ULONG   NtStatus;

    DWORD   bytesWritten;
    DWORD   LastError;
    bool    bIn;

    XFER_MODE_TYPE  XferMode;

    bool    XferData(PUCHAR buf, LONG &len, CCyIsoPktInfo* pktInfos = NULL);
    bool    XferData(PUCHAR buf, LONG &bufLen, CCyIsoPktInfo* pktInfos, bool pktMode);
    virtual PUCHAR BeginDataXfer(PUCHAR buf, LONG len, OVERLAPPED *ov) = 0;
    virtual bool FinishDataXfer(PUCHAR buf, LONG &len, OVERLAPPED *ov, PUCHAR pXmitBuf, CCyIsoPktInfo* pktInfos = NULL);
    bool    WaitForXfer(OVERLAPPED *ov, ULONG tOut);
    ULONG   GetXferSize(void);
    void    SetXferSize(ULONG xfer);

    bool Reset(void);
    bool Abort(void);
};


/********************************************************************************
*
* The Control Endpoint Class
*
********************************************************************************/
class CCyControlEndPoint : public CCyUSBEndPoint
{
public:
    CCyControlEndPoint(void);
    CCyControlEndPoint(CCyControlEndPoint& ept);
    CCyControlEndPoint(HANDLE h, PUSB_ENDPOINT_DESCRIPTOR pEndPtDescriptor);

    CTL_XFER_TGT_TYPE Target;
    CTL_XFER_REQ_TYPE ReqType;
    CTL_XFER_DIR_TYPE Direction;

    UCHAR   ReqCode;
    WORD    Value;
    WORD    Index;

    bool Read(PUCHAR buf, LONG &len);
    bool Write(PUCHAR buf, LONG &len);
    PUCHAR BeginDataXfer(PUCHAR buf, LONG len, OVERLAPPED *ov);
};


/********************************************************************************
*
* The Isoc Endpoint Class
*
********************************************************************************/
class CCyIsocEndPoint : public CCyUSBEndPoint
{

protected:
    virtual PUCHAR BeginDirectXfer(PUCHAR buf, LONG bufLen, OVERLAPPED *ov);
    virtual PUCHAR BeginBufferedXfer(PUCHAR buf, LONG bufLen, OVERLAPPED *ov);

public:
    CCyIsocEndPoint(void);
    CCyIsocEndPoint(HANDLE h, PUSB_ENDPOINT_DESCRIPTOR pEndPtDescriptor);
    CCyIsocEndPoint(HANDLE h, PUSB_ENDPOINT_DESCRIPTOR pEndPtDescriptor,USB_SUPERSPEED_ENDPOINT_COMPANION_DESCRIPTOR* SSEndPtDescriptor);

    PUCHAR BeginDataXfer(PUCHAR buf, LONG len, OVERLAPPED *ov);
    CCyIsoPktInfo* CreatePktInfos(LONG bufLen, int &packets);
};


/********************************************************************************
*
* The Bulk Endpoint Class
*
********************************************************************************/
class CCyBulkEndPoint : public CCyUSBEndPoint
{
public:
    CCyBulkEndPoint(void);
    CCyBulkEndPoint(HANDLE h, PUSB_ENDPOINT_DESCRIPTOR pEndPtDescriptor);
    CCyBulkEndPoint(HANDLE h, PUSB_ENDPOINT_DESCRIPTOR pEndPtDescriptor,USB_SUPERSPEED_ENDPOINT_COMPANION_DESCRIPTOR* SSEndPtDescriptor);

    PUCHAR BeginDataXfer(PUCHAR buf, LONG len, OVERLAPPED *ov);
};


/********************************************************************************
*
* The Interrupt Endpoint Class
*
********************************************************************************/
class CCyInterruptEndPoint : public CCyUSBEndPoint
{
public:
    CCyInterruptEndPoint(void);
    CCyInterruptEndPoint(HANDLE h, PUSB_ENDPOINT_DESCRIPTOR pEndPtDescriptor);
    CCyInterruptEndPoint(HANDLE h, PUSB_ENDPOINT_DESCRIPTOR pEndPtDescriptor,USB_SUPERSPEED_ENDPOINT_COMPANION_DESCRIPTOR* SSEndPtDescriptor);

    PUCHAR BeginDataXfer(PUCHAR buf, LONG len, OVERLAPPED *ov);
};


/********************************************************************************
*
* The Interface Class
*
********************************************************************************/
class CCyUSBInterface
{
public:
    CCyUSBEndPoint *EndPoints[MAX_ENDPTS];  /* Holds pointers to all the interface's endpoints,
                                               plus a pointer to the Control endpoint zero */
    UCHAR   bLength;
    UCHAR   bDescriptorType;
    UCHAR   bInterfaceNumber;
    UCHAR   bAlternateSetting;
    UCHAR   bNumEndpoints;                  /* Not counting the control endpoint */
    UCHAR   bInterfaceClass;
    UCHAR   bInterfaceSubClass;
    UCHAR   bInterfaceProtocol;
    UCHAR   iInterface;

    UCHAR   bAltSettings;
    USHORT  wTotalLength;                   /* Needed in case Intfc has additional (non-endpt) descriptors */

    CCyUSBInterface(HANDLE handle, PUSB_INTERFACE_DESCRIPTOR pIntfcDescriptor,UCHAR usb30Dummy);
    CCyUSBInterface(HANDLE h, PUSB_INTERFACE_DESCRIPTOR pIntfcDescriptor);
    CCyUSBInterface(CCyUSBInterface& ifc);  /* Copy Constructor */
    ~CCyUSBInterface(void);

};


/********************************************************************************
*
* The Config Class
*
********************************************************************************/
class CCyUSBConfig
{
public:
    CCyUSBInterface *Interfaces[MAX_INTERFACES];

    UCHAR   bLength;
    UCHAR   bDescriptorType;
    USHORT  wTotalLength;
    UCHAR   bNumInterfaces;
    UCHAR   bConfigurationValue;
    UCHAR   iConfiguration;
    UCHAR   bmAttributes;
    UCHAR   MaxPower;

    UCHAR AltInterfaces;

    CCyUSBConfig(void);
    CCyUSBConfig(CCyUSBConfig& cfg);  /* Copy Constructor */
    CCyUSBConfig(HANDLE h, PUSB_CONFIGURATION_DESCRIPTOR pConfigDescr);
    CCyUSBConfig(HANDLE h, PUSB_CONFIGURATION_DESCRIPTOR pConfigDescr,UCHAR usb30Dummy);
    ~CCyUSBConfig(void);
};


/********************************************************************************
*
* The Bos USB20 Extesnion Class
*
********************************************************************************/
class CCyBosUSB20Extesnion
{
public:
    UCHAR   bLength;            /* Descriptor length */
    UCHAR   bDescriptorType;    /* Descriptor Type */
    UCHAR   bDevCapabilityType; /* Device capability type */
    UINT    bmAttribute;        /* Bitmap encoding for supprted feature and  Link power managment supprted if set */

    CCyBosUSB20Extesnion(void);
    CCyBosUSB20Extesnion(HANDLE h,PUSB_BOS_USB20_DEVICE_EXTENSION BosUsb20ExtDesc);
};


/********************************************************************************
*
* The Bos SuperSpeed Capability Class
*
********************************************************************************/
class CCyBosSuperSpeedCapability
{
public:
    UCHAR   bLength;
    UCHAR   bDescriptorType;
    UCHAR   bDevCapabilityType;
    UCHAR   bmAttribute;
    USHORT  SpeedsSuported;
    UCHAR   bFunctionalitySupporte;
    UCHAR   bU1DevExitLat;
    USHORT  bU2DevExitLat;

    CCyBosSuperSpeedCapability(void);
    CCyBosSuperSpeedCapability(HANDLE h,PUSB_BOS_SS_DEVICE_CAPABILITY pUSB_SuperSpeedUsb);

};


/********************************************************************************
*
* The Bos Container ID Class
*
********************************************************************************/
class CCyBosContainerID
{
public:
    UCHAR   bLength;            /* Descriptor length */
    UCHAR   bDescriptorType;    /* Descriptor Type */
    UCHAR   bDevCapabilityType; /* Device capability type */
    UCHAR   bReserved;         /* no use */
    UCHAR   ContainerID[USB_BOS_CAPABILITY_TYPE_CONTAINER_ID_SIZE]; /* UUID */

    CCyBosContainerID(void);
    CCyBosContainerID(HANDLE h,PUSB_BOS_CONTAINER_ID pContainerID);
};


/********************************************************************************
*
* The USB BOS Class
*
********************************************************************************/
class CCyUSBBOS
{
public:

    CCyBosContainerID           *pContainer_ID;
    CCyBosUSB20Extesnion        *pUSB20_DeviceExt;
    CCyBosSuperSpeedCapability  *pSS_DeviceCap;

    UCHAR   bLength;            /* Descriptor length */
    UCHAR   bDescriptorType;    /* Descriptor Type */
    USHORT  wTotalLength;      /* Total length of descriptor ( icluding device capabilty */
    UCHAR   bNumDeviceCaps;     /* Number of device capability descriptors in BOS */

    CCyUSBBOS(void);
    CCyUSBBOS(HANDLE h,PUSB_BOS_DESCRIPTOR pBosDescrData);
	~CCyUSBBOS();
};

/*********************************************************************************
*
* The USB Device Class - This is the main class that contains members of all the
* other classes.
*
* To use the library, create an instance of this Class and call it's Open method.
*
*********************************************************************************/
class CCyUSBDevice
{
    /* The public members are accessible (i.e. corruptible) by the user of the library
     * Algorithms of the class don't rely on any public members.  Instead, they use the
     * private members of the class for their calculations. */

public:

    CCyUSBDevice(HANDLE hnd = NULL, GUID guid = CYUSBDRV_GUID, BOOL bOpen = true);
    ~CCyUSBDevice(void);

    CCyUSBEndPoint  **EndPoints;     /* Shortcut to USBCfgs[CfgNum]->Interfaces[IntfcIndex]->Endpoints */
    CCyUSBEndPoint  *EndPointOf(UCHAR addr);

    CCyUSBBOS               *UsbBos;
    CCyIsocEndPoint         *IsocInEndPt;
    CCyIsocEndPoint         *IsocOutEndPt;
    CCyBulkEndPoint         *BulkInEndPt;
    CCyBulkEndPoint         *BulkOutEndPt;
    CCyControlEndPoint      *ControlEndPt;
    CCyInterruptEndPoint    *InterruptInEndPt;
    CCyInterruptEndPoint    *InterruptOutEndPt;


    USHORT      StrLangID;
    ULONG       LastError;
    ULONG       UsbdStatus;
    ULONG       NtStatus;
    ULONG       DriverVersion;
    ULONG       USBDIVersion;
    char        DeviceName[USB_STRING_MAXLEN];
    char        FriendlyName[USB_STRING_MAXLEN];
    wchar_t     Manufacturer[USB_STRING_MAXLEN];
    wchar_t     Product[USB_STRING_MAXLEN];
    wchar_t     SerialNumber[USB_STRING_MAXLEN];

    CHAR        DevPath[USB_STRING_MAXLEN];

    USHORT      BcdUSB;
    USHORT      VendorID;
    USHORT      ProductID;
    UCHAR       USBAddress;
    UCHAR       DevClass;
    UCHAR       DevSubClass;
    UCHAR       DevProtocol;
    INT       MaxPacketSize;
    USHORT      BcdDevice;

    UCHAR       ConfigValue;
    UCHAR       ConfigAttrib;
    UCHAR       MaxPower;

    UCHAR       IntfcClass;
    UCHAR       IntfcSubClass;
    UCHAR       IntfcProtocol;
    bool        bHighSpeed;
    bool        bSuperSpeed;

    DWORD       BytesXfered;

    UCHAR       DeviceCount(void);
    UCHAR       ConfigCount(void);
    UCHAR       IntfcCount(void);
    UCHAR       AltIntfcCount(void);
    UCHAR       EndPointCount(void);

    void        SetConfig(UCHAR cfg);
    UCHAR       Config(void)     { return CfgNum; }    /* Normally 0 */
    UCHAR       Interface(void)  { return IntfcNum; }  /* Usually 0 */

    /* No SetInterface method since only 1 intfc per device (per Windows) */
    UCHAR       AltIntfc(void);
    bool        SetAltIntfc(UCHAR alt);

    GUID        DriverGUID(void) { return DrvGuid; }
    HANDLE      DeviceHandle(void) { return hDevice; }
    void        UsbdStatusString(ULONG stat, PCHAR s);
    bool        CreateHandle(UCHAR dev);
    void        DestroyHandle();

    bool        Open(UCHAR dev);
    void        Close(void);
    bool        Reset(void);
    bool        ReConnect(void);
    bool        Suspend(void);
    bool        Resume(void);
    bool        IsOpen(void)    { return (hDevice != INVALID_HANDLE_VALUE); }

    UCHAR       PowerState(void);

    bool        GetBosDescriptor(PUSB_BOS_DESCRIPTOR descr);
    bool        GetBosUSB20DeviceExtensionDescriptor(PUSB_BOS_USB20_DEVICE_EXTENSION descr);
    bool        GetBosContainedIDDescriptor(PUSB_BOS_CONTAINER_ID descr);
    bool        GetBosSSCapabilityDescriptor(PUSB_BOS_SS_DEVICE_CAPABILITY descr);

    void        GetDeviceDescriptor(PUSB_DEVICE_DESCRIPTOR descr);
    void        GetConfigDescriptor(PUSB_CONFIGURATION_DESCRIPTOR descr);
    void        GetIntfcDescriptor(PUSB_INTERFACE_DESCRIPTOR descr);
    CCyUSBConfig    GetUSBConfig(int index);

private:

    USB_DEVICE_DESCRIPTOR           USBDeviceDescriptor;
    PUSB_CONFIGURATION_DESCRIPTOR   USBConfigDescriptors[2];
    PUSB_BOS_DESCRIPTOR             pUsbBosDescriptor;

    CCyUSBConfig    *USBCfgs[2];

    HANDLE      hWnd;
    HANDLE      hDevice;
    HANDLE      hDevNotification;
    HANDLE      hHndNotification;

    GUID        DrvGuid;

    UCHAR       Devices;
    UCHAR       Interfaces;
    UCHAR       AltInterfaces;
    UCHAR       Configs;

    UCHAR       DevNum;
    UCHAR       CfgNum;
    UCHAR       IntfcNum;   /* The current selected interface's bInterfaceNumber */
    UCHAR       IntfcIndex; /* The entry in the Config's interfaces table matching to IntfcNum and AltSetting */

    bool        GetInternalBosDescriptor();
    void        GetDevDescriptor(void);
    void        GetCfgDescriptor(int descIndex);
    void        GetString(wchar_t *s, UCHAR sIndex);
    void        SetStringDescrLanguage(void);
    void        SetAltIntfcParams(UCHAR alt);
    bool        IoControl(ULONG cmd, PUCHAR buf, ULONG len);

    void        SetEndPointPtrs(void);
    void        GetDeviceName(void);
    void        GetFriendlyName(void);
    void        GetDriverVer(void);
    void        GetUSBDIVer(void);
    void        GetSpeed(void);
    void        GetUSBAddress(void);
    //void      CloseEndPtHandles(void);

    bool        RegisterForPnpEvents(HANDLE h);
};


/********************************************************************************
*
* The FX3 Device Class
*
********************************************************************************/
class CCyFX3Device: public CCyUSBDevice
{
public:
    CCyFX3Device(void);
    ~CCyFX3Device(void);
    bool IsBootLoaderRunning();
    FX3_FWDWNLOAD_ERROR_CODE DownloadFw(char *fileName, FX3_FWDWNLOAD_MEDIA_TYPE enMediaType);

private:

    bool Ep0VendorCommand(vendorCmdData cmdData);
    bool SetProgramEntry(UCHAR opCode,UINT start_addr);

    bool DownloadBufferToDevice(UINT start_addr, USHORT count, UCHAR *data_buf, UCHAR opCode);
    bool UploadBufferFromDevice(UINT start_addr, USHORT count, UCHAR *data_buf, UCHAR opCode);

    FX3_FWDWNLOAD_ERROR_CODE DownloadFwToRam(PUCHAR buffer_p, UINT fw_size, UCHAR opCode);
    FX3_FWDWNLOAD_ERROR_CODE DownloadUserIMGtoI2CE2PROM(PUCHAR buffer_p, UINT fw_size, UCHAR opCode);
    FX3_FWDWNLOAD_ERROR_CODE DownloadUserIMGtoSPIFLASH(PUCHAR buffer_p, UINT fw_size, UCHAR opCode);

    FX3_FWDWNLOAD_ERROR_CODE EraseSectorOfSPIFlash(UINT SectorNumber, UCHAR opCode);
    bool WriteToSPIFlash(PUCHAR Buf, UINT buflen, UINT ByteAddress, UCHAR opCode);
};

/********************************************************************************/

#endif
