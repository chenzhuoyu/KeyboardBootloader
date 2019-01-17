#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/boot.h>
#include <avr/interrupt.h>
#include <LUFA/Drivers/USB/USB.h>

#include "usb_desc.h"

#define SW_BTN              (1 << PB5)
#define LED_RED             (1 << PB6)
#define LED_BLUE            (1 << PB7)

#define BTN_READ()          (PINB & SW_BTN)
#define WDT_STOP()          (MCUSR &= ~(1 << WDRF))

#define RED_STOP()          (PORTB |= LED_RED)
#define RED_START()         (PORTB &= ~LED_RED)

#define BLUE_STOP()         (PORTB |= LED_BLUE)
#define BLUE_START()        (PORTB &= ~LED_BLUE)

static uint8_t _led = 0;

static void hw_init(void)
{
	/* disable watchdog if enabled by bootloader or fuses */
	WDT_STOP();
	wdt_disable();

    /* LEDs and button switch */
    DDRB &= ~SW_BTN;
    DDRB |=  LED_RED;
    DDRB |=  LED_BLUE;
}

static void bootldr_main(void)
{
    /* relocate the interrupt vector table to the bootloader section */
    MCUCR = (1 << IVCE);
    MCUCR = (1 << IVSEL);

    /* bootloader active LED toggle timer initialization */
    TCCR1B = (1 << CS11) | (1 << CS10);
    TIMSK1 = (1 << TOIE1);

    /* initialize USB */
    USB_Init();
    GlobalInterruptEnable();

    /* main event loop */
    for (;;)
        USB_USBTask();

    /* relocate the interrupt vector table back to the application section */
    MCUCR = (1 << IVCE);
    MCUCR = 0;

    /* force a watchdog reset */
    wdt_enable(WDTO_15MS);
    for (;;);
}

void enter_bootloader(void);
void enter_bootloader(void)
{
    hw_init();
    BLUE_START();
    bootldr_main();
}

int main(void)
{
    /* turn on lights when start */
    hw_init();
    BLUE_START();

    /* wait a bit to stablize the button input */
    for (volatile int i = 0; i < 500; i++)
        for (volatile int j = 0; j < 500; j++)
            asm volatile ("nop");

    /* turn off after holding period */
    RED_STOP();
    BLUE_STOP();

    /* check for bootloader button */
    if (!(BTN_READ()))
        enter_bootloader();

    /* goto application */
    ((void (*)(void))(0x0000))();
    for (;;);
}

void EVENT_USB_Device_ControlRequest(void)
{

}

ISR(TIMER1_OVF_vect, ISR_BLOCK)
{
    if (_led++ == 3)
    {
        _led = 0;
        PORTB ^= LED_BLUE;
    }
}
