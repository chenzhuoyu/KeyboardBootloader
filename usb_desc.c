#include "usb_desc.h"

static const USB_Descriptor_Device_t _usb_device =
{
    .Header = {
        .Size = sizeof(USB_Descriptor_Device_t),
        .Type = DTYPE_Device
    },

    .USBSpecification       = VERSION_BCD(1, 1, 0),
    .Class                  = USB_CSCP_NoDeviceClass,
    .SubClass               = USB_CSCP_NoDeviceSubclass,
    .Protocol               = USB_CSCP_NoDeviceProtocol,
    .Endpoint0Size          = FIXED_CONTROL_ENDPOINT_SIZE,
    .VendorID               = 0x01a1,       /* = 0417 */
    .ProductID              = 0x07c8,       /* = 1992 */
    .ReleaseNumber          = VERSION_BCD(0, 0, 1),
    .ManufacturerStrIndex   = STRING_Vendor,
    .ProductStrIndex        = STRING_Product,
    .SerialNumStrIndex      = NO_DESCRIPTOR,
    .NumberOfConfigurations = FIXED_NUM_CONFIGURATIONS
};

static const usb_desc_t _usb_desc =
{
    .header = {
        .Header = {
            .Size = sizeof(USB_Descriptor_Configuration_Header_t),
            .Type = DTYPE_Configuration
        },

        .TotalConfigurationSize = sizeof(usb_desc_t),
        .TotalInterfaces        = 1,
        .ConfigurationNumber    = 1,
        .ConfigurationStrIndex  = NO_DESCRIPTOR,
        .ConfigAttributes       = USB_CONFIG_ATTR_RESERVED,
        .MaxPowerConsumption    = USB_CONFIG_POWER_MA(100)
    },

    .dfu_intf = {
        .Header = {
            .Size = sizeof(USB_Descriptor_Interface_t),
            .Type = DTYPE_Interface
        },

        .InterfaceNumber    = 0,
        .AlternateSetting   = 0x00,
        .TotalEndpoints     = 0,
        .Class              = 0xff,
        .SubClass           = 0x00,
        .Protocol           = 0x00,
        .InterfaceStrIndex  = NO_DESCRIPTOR
    }
};

static const USB_Descriptor_String_t _str_lang    = USB_STRING_DESCRIPTOR_ARRAY(LANGUAGE_ID_ENG);
static const USB_Descriptor_String_t _str_vendor  = USB_STRING_DESCRIPTOR(L"Oxygen");
static const USB_Descriptor_String_t _str_product = USB_STRING_DESCRIPTOR(L"Oxygen's Keyboard DFU Mode");

uint16_t CALLBACK_USB_GetDescriptor(const uint16_t value, const uint16_t index, const void **desc)
{
    switch (value >> 8)
    {
        case DTYPE_Device:
        {
            *desc = &_usb_device;
            return sizeof(USB_Descriptor_Device_t);
        }

        case DTYPE_Configuration:
        {
            *desc = &_usb_desc;
            return sizeof(usb_desc_t);
        }

        case DTYPE_String:
        {
            switch (value & 0xff)
            {
                case STRING_Lang:
                {
                    *desc = &_str_lang;
                    return _str_lang.Header.Size;
                }

                case STRING_Vendor:
                {
                    *desc = &_str_vendor;
                    return _str_vendor.Header.Size;
                }

                case STRING_Product:
                {
                    *desc = &_str_product;
                    return _str_product.Header.Size;
                }

                default:
                {
                    *desc = NULL;
                    return NO_DESCRIPTOR;
                }
            }
        }

        default:
        {
            *desc = NULL;
            return NO_DESCRIPTOR;
        }
    }
}
