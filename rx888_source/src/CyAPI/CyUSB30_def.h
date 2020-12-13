/*
 ## Cypress CyAPI C++ library USB3.0 defination header file (CyUSB30_def.h)
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
#ifndef _CYUSB30_H
#define _CYUSB30_H

//#pragma pack(1)
#pragma pack(push, 1)
// USB3.0 specific constant defination
#define BCDUSBJJMASK  0xFF00 //(0xJJMN JJ - Major version,M Minor version, N sub-minor vesion)
#define USB30MAJORVER 0x0300
#define USB20MAJORVER 0x0200

#define USB_BOS_DESCRIPTOR_TYPE			      0x0F
#define USB_DEVICE_CAPABILITY                 0x10
#define USB_SUPERSPEED_ENDPOINT_COMPANION     0x30
#define USB_BOS_CAPABILITY_TYPE_Wireless_USB  0x01
#define USB_BOS_CAPABILITY_TYPE_USB20_EXT	  0x02
#define USB_BOS_CAPABILITY_TYPE_SUPERSPEED_USB    0x03
#define USB_BOS_CAPABILITY_TYPE_CONTAINER_ID       0x04
#define USB_BOS_CAPABILITY_TYPE_CONTAINER_ID_SIZE  0x10

#define USB_BOS_DEVICE_CAPABILITY_TYPE_INDEX 0x2
//constant defination
typedef struct _USB_BOS_DESCRIPTOR
{
    UCHAR bLength;/* Descriptor length*/
    UCHAR bDescriptorType;/* Descriptor Type */
    USHORT wTotalLength;/* Total length of descriptor ( icluding device capability*/
    UCHAR bNumDeviceCaps;/* Number of device capability descriptors in BOS  */
}USB_BOS_DESCRIPTOR,*PUSB_BOS_DESCRIPTOR;

typedef struct _USB_BOS_USB20_DEVICE_EXTENSION
{
    UCHAR bLength;/* Descriptor length*/
    UCHAR bDescriptorType;/* Descriptor Type */
    UCHAR bDevCapabilityType;/* Device capability type*/
    UINT bmAttribute;// Bitmap encoding for supprted feature and  Link power managment supprted if set
}USB_BOS_USB20_DEVICE_EXTENSION,*PUSB_BOS_USB20_DEVICE_EXTENSION;

typedef struct _USB_BOS_SS_DEVICE_CAPABILITY
{
    UCHAR bLength;/* Descriptor length*/
    UCHAR bDescriptorType;/* Descriptor Type */
    UCHAR bDevCapabilityType;/* Device capability type*/
    UCHAR bmAttribute;// Bitmap encoding for supprted feature and  Link power managment supprted if set
    USHORT wSpeedsSuported;//low speed supported if set,full speed supported if set,high speed supported if set,super speed supported if set,15:4 nt used
    UCHAR bFunctionalitySupporte;		
    UCHAR bU1DevExitLat;//U1 device exit latency		
    USHORT bU2DevExitLat;//U2 device exit latency        
}USB_BOS_SS_DEVICE_CAPABILITY,*PUSB_BOS_SS_DEVICE_CAPABILITY;

typedef struct _USB_BOS_CONTAINER_ID
{
    UCHAR bLength;/* Descriptor length*/
    UCHAR bDescriptorType;/* Descriptor Type */
    UCHAR bDevCapabilityType;/* Device capability type*/
    UCHAR bReserved; // no use
    UCHAR ContainerID[USB_BOS_CAPABILITY_TYPE_CONTAINER_ID_SIZE];/* UUID */
}USB_BOS_CONTAINER_ID,*PUSB_BOS_CONTAINER_ID;

typedef struct _USB_SUPERSPEED_ENDPOINT_COMPANION_DESCRIPTOR
{
    UCHAR bLength;
    UCHAR bDescriptorType;
    UCHAR bMaxBurst;
    UCHAR bmAttributes;        
    USHORT bBytesPerInterval;
}USB_SUPERSPEED_ENDPOINT_COMPANION_DESCRIPTOR,*PUSB_SUPERSPEED_ENDPOINT_COMPANION_DESCRIPTOR;
#pragma pack(pop)
#endif /*_CYUSB30_H*/

