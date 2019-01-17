#ifndef USB_DESC_H
#define USB_DESC_H

#include <avr/pgmspace.h>
#include <LUFA/Drivers/USB/USB.h>

typedef struct _usb_desc_t
{
    USB_Descriptor_Configuration_Header_t header;
    USB_Descriptor_Interface_t            dfu_intf;
} usb_desc_t;

typedef enum _string_desc_t
{
    STRING_Lang     = 0,
    STRING_Vendor   = 1,
    STRING_Product  = 2,
} string_desc_t;

#endif

