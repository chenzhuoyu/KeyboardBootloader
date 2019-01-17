#include <avr/io.h>
#include <avr/boot.h>
#include <avr/interrupt.h>

#include <LUFA/Drivers/USB/USB.h>
#include <LUFA/Platform/Platform.h>

#include "usb_desc.h"

#define SW_BTN              (1 << PB5)
#define LED_RED             (1 << PB6)
#define LED_BLUE            (1 << PB7)

static void hw_init(void)
{
	/* Relocate the interrupt vector table to the bootloader section */
	MCUCR = (1 << IVCE);
	MCUCR = (1 << IVSEL);

    /* LEDs and button switch */
    DDRB &= ~SW_BTN;
    DDRB |=  LED_RED;
    DDRB |=  LED_BLUE;

    /* turn off lights when start */
    PORTB |= LED_RED;
    PORTB |= LED_BLUE;

	/* bootloader active LED toggle timer initialization */
	TIMSK1 = (1 << TOIE1);
	TCCR1B = ((1 << CS11) | (1 << CS10));
}

int main(void)
{
    /* initialize GPIO and USB */
    hw_init();
    USB_Init();
    GlobalInterruptEnable();

    /* main event loop */
    for (;;)
    {
        USB_USBTask();
    }
}

void EVENT_USB_Device_ControlRequest(void)
{
	if (!(Endpoint_IsSETUPReceived()))
	    return;
}

ISR(TIMER1_OVF_vect, ISR_BLOCK)
{
	PORTB ^= LED_RED;
	PORTB ^= LED_BLUE;
}
