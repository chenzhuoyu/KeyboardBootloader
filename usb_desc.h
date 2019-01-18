#ifndef USB_DESC_H
#define USB_DESC_H

#include <avr/pgmspace.h>
#include <LUFA/Drivers/USB/USB.h>

typedef struct _usb_desc_t
{
    USB_Descriptor_Configuration_Header_t   header;
    USB_Descriptor_Interface_t              dfu_intf;
    USB_Descriptor_Endpoint_t               dfu_read_ep;
    USB_Descriptor_Endpoint_t               dfu_write_ep;
} usb_desc_t;

typedef enum _string_desc_t
{
    STRING_Lang     = 0,
    STRING_Vendor   = 1,
    STRING_Product  = 2,
} string_desc_t;

#define DFU_READ_EP         (ENDPOINT_DIR_IN | 1)
#define DFU_READ_SIZE       64

#define DFU_WRITE_EP        (ENDPOINT_DIR_OUT | 2)
#define DFU_WRITE_SIZE      64

#endif

