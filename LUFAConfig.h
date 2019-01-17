#ifndef _LUFA_CONFIG_H_
#define _LUFA_CONFIG_H_

/* General USB Driver Related Tokens: */

#define NO_SOF_EVENTS
#define USB_DEVICE_ONLY
#define USE_STATIC_OPTIONS				(USB_DEVICE_OPT_FULLSPEED | USB_OPT_REG_ENABLED | USB_OPT_AUTO_PLL)

/* USB Device Mode Driver Related Tokens: */

#define NO_INTERNAL_SERIAL
#define NO_DEVICE_REMOTE_WAKEUP
#define NO_DEVICE_SELF_POWER

#define USE_RAM_DESCRIPTORS
#define CONTROL_ONLY_DEVICE

#define FIXED_NUM_CONFIGURATIONS		1
#define FIXED_CONTROL_ENDPOINT_SIZE		32

#endif