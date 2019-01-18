#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/boot.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <LUFA/Drivers/USB/USB.h>

#include "usb_desc.h"

#define SW_BTN              (1 << PB5)
#define LED_RED             (1 << PB6)
#define LED_BLUE            (1 << PB7)

#define BTN_READ()          (PINB & SW_BTN)
#define WDT_CLEAR()         (MCUSR &= ~(1 << WDRF))

#define MEM_FLASH           0xb0
#define MEM_EEPROM          0xb1

#define MEM_FLASH_PAGE      128
#define MEM_EEPROM_PAGE     4

#define MEM_FLASH_SIZE      0x4000
#define MEM_EEPROM_SIZE     0x0200

#define MEM_FLASH_WRITE     0x3000
#define MEM_EEPROM_WRITE    0x0200

#define CMD_SET_ADDR        0x50
#define CMD_SET_TYPE        0x51
#define CMD_GET_ADDR        0xa0
#define CMD_GET_TYPE        0xa1

#define CMD_READ_PAGE       0xa2
#define CMD_WRITE_PAGE      0x52

#define CMD_NOP             0xfe
#define CMD_RESET           0xff

#define ERR_OK              0
#define ERR_CMD             0x80
#define ERR_LEN             0x81
#define ERR_ADDR            0x82
#define ERR_TYPE            0x83
#define ERR_ALIGN           0x84
#define ERR_PAGE            0x85
#define ERR_OVERFLOW        0x86

#define DFU_TICKS           60
#define DFU_BUFFER_SIZE     (MEM_FLASH_PAGE > MEM_EEPROM_PAGE ? MEM_FLASH_PAGE : MEM_EEPROM_PAGE)

static volatile uint8_t _dfu = 1;
static volatile uint8_t _red = 1;
static volatile uint8_t _blue = 1;
static volatile uint8_t _tick = DFU_TICKS;

static uint8_t _cmd_err = ERR_OK;
static uint8_t _mem_type = MEM_FLASH;
static uint16_t _mem_addr = 0x0000;
static uint16_t _mem_size = MEM_FLASH_SIZE;
static uint16_t _mem_page = MEM_FLASH_PAGE;
static uint16_t _mem_write = MEM_FLASH_WRITE;

static uint8_t _page_ptr = 0;
static uint8_t _page_read = 0;
static uint8_t _page_buffer[DFU_BUFFER_SIZE] = {0};

static void hw_init(void)
{
	/* disable watchdog if enabled by bootloader or fuses */
	WDT_CLEAR();
	wdt_disable();

    /* LEDs and button switch */
    DDRB &= ~SW_BTN;
    DDRB |=  LED_RED;
    DDRB |=  LED_BLUE;

    /* initial LED status */
    PORTB |=  LED_RED;
    PORTB &= ~LED_BLUE;
}

static void hw_delay(void)
{
    for (volatile int i = 0; i < 500; i++)
        for (volatile int j = 0; j < 500; j++)
            asm volatile ("nop");
}

static void usb_poll_in(void)
{
    /* select endpoint */
    uint8_t n = 0;
    uint8_t buf[DFU_READ_SIZE] = {};
    Endpoint_SelectEndpoint(DFU_READ_EP);

    /* check for USB input */
    if (!_page_read || !(Endpoint_IsINReady()))
        return;

    /* read by type */
    while (_page_read && (n < DFU_READ_SIZE) && (_mem_addr < _mem_size))
    {
        switch (_mem_type)
        {
            case MEM_FLASH  : buf[n]   = pgm_read_byte   ((uint8_t *)_mem_addr); break;
            case MEM_EEPROM : buf[n]   = eeprom_read_byte((uint8_t *)_mem_addr); break;
            default         : _cmd_err = ERR_TYPE;                               return;
        }

        /* update read pointers */
        n++;
        _mem_addr++;
        _page_read--;
    }

    /* write to host */
    Endpoint_Write_Stream_LE(buf, n, NULL);
    Endpoint_ClearIN();
}

static void usb_poll_out(void)
{
    /* select endpoint */
    uint8_t size = DFU_WRITE_SIZE;
    Endpoint_SelectEndpoint(DFU_WRITE_EP);

    /* check for USB output */
    if (!(Endpoint_IsOUTReceived()))
        return;

    /* read input buffer */
    if (_page_ptr < _mem_page)
    {
        /* limit byte size */
        if (size > _mem_page - _page_ptr)
            size = _mem_page - _page_ptr;

        /* read from host */
        Endpoint_Read_Stream_LE(&(_page_buffer[_page_ptr]), size, NULL);
        Endpoint_ClearOUT();
        _page_ptr += size;
    }
}

static void boot_page_flash_safe(uint16_t addr, const uint16_t *buffer, uint16_t words)
{
    while (words--)
    {
        boot_page_fill_safe(addr, *buffer++);
        addr += 2;
    }
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
    while (_dfu)
    {
        USB_USBTask();
        usb_poll_in();
        usb_poll_out();
    }

    /* shutdown USB */
    USB_Disable();
    GlobalInterruptDisable();

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
    bootldr_main();
}

int main(void)
{
    /* wait a bit to stablize the button input */
    hw_init();
    hw_delay();

    /* turn off after holding period */
    PORTB |= LED_RED;
    PORTB |= LED_BLUE;

    /* check for bootloader button */
    if (!(BTN_READ()))
        bootldr_main();

    /* goto application */
    ((void (*)(void))(0x0000))();
    for (;;);
}

void EVENT_USB_Device_ControlRequest(void)
{
	/* ignore any requests that aren't directed to the DFU interface */
	if ((USB_ControlRequest.wIndex != 1) ||
        (USB_ControlRequest.wLength >= FIXED_CONTROL_ENDPOINT_SIZE))
	    return;

    /* check for control request */
    switch (USB_ControlRequest.bRequest)
    {
        /* NOP command */
        case CMD_NOP:
        {
            Endpoint_ClearSETUP();
            Endpoint_Write_Control_Stream_LE(&_cmd_err, 1);
            Endpoint_ClearOUT();
            break;
        }

        /* RESET command */
        case CMD_RESET:
        {
            _dfu = 0;
            _tick = 0;
            _cmd_err = ERR_OK;
            Endpoint_ClearSETUP();
            Endpoint_Write_Control_Stream_LE(&_cmd_err, sizeof(uint8_t));
            Endpoint_ClearOUT();
            break;
        }

        /* get memory address */
        case CMD_GET_ADDR:
        {
            /* output buffer */
            uint8_t outbuf[] = {
                ERR_OK,
                (uint8_t)(_mem_addr >> 0),
                (uint8_t)(_mem_addr >> 8),
            };

            /* send output buffer */
            _cmd_err = ERR_OK;
            Endpoint_ClearSETUP();
            Endpoint_Write_Control_Stream_LE(outbuf, sizeof(outbuf));
            Endpoint_ClearOUT();
            break;
        }

        /* get memory type */
        case CMD_GET_TYPE:
        {
            /* output buffer */
            uint8_t outbuf[] = {
                ERR_OK,
                _mem_type,
            };

            /* send output buffer */
            _cmd_err = ERR_OK;
            Endpoint_ClearSETUP();
            Endpoint_Write_Control_Stream_LE(outbuf, sizeof(outbuf));
            Endpoint_ClearOUT();
            break;
        }

        /* set memory address */
        case CMD_SET_ADDR:
        {
            /* read address */
            uint16_t addr;
            Endpoint_ClearSETUP();
            Endpoint_Read_Control_Stream_LE(&addr, sizeof(uint16_t));
            Endpoint_ClearIN();

            /* must be alighed with page boundary */
            if (addr % _mem_page)
            {
                _cmd_err = ERR_ALIGN;
                break;
            }

            /* check for range */
            if (addr >= _mem_size)
            {
                _cmd_err = ERR_ADDR;
                break;
            }

            /* it's an error */
            _cmd_err = ERR_OK;
            _mem_addr = addr;
            break;
        }

        /* set memory type */
        case CMD_SET_TYPE:
        {
            /* read type */
            uint8_t type;
            Endpoint_ClearSETUP();
            Endpoint_Read_Control_Stream_LE(&type, sizeof(uint8_t));
            Endpoint_ClearIN();

            /* check for memory type */
            switch (type)
            {
                /* FLASH memory */
                case MEM_FLASH:
                {
                    _mem_type = MEM_FLASH;
                    _mem_size = MEM_FLASH_SIZE;
                    _mem_page = MEM_FLASH_PAGE;
                    _mem_write = MEM_FLASH_WRITE;
                    break;
                }

                /* EEPROM memory */
                case MEM_EEPROM:
                {
                    _mem_type = MEM_EEPROM;
                    _mem_size = MEM_EEPROM_SIZE;
                    _mem_page = MEM_EEPROM_PAGE;
                    _mem_write = MEM_EEPROM_WRITE;
                    break;
                }

                /* other unknown types */
                default:
                {
                    _cmd_err = ERR_TYPE;
                    break;
                }
            }

            /* reset memory address pointer */
            _cmd_err = ERR_OK;
            _mem_addr = 0x0000;
            break;
        }

        /* read one page */
        case CMD_READ_PAGE:
        {
            /* page info */
            uint8_t page[] = {
                ERR_OK,
                (uint8_t)(_mem_page >> 0),
                (uint8_t)(_mem_page >> 8),
            };

            /* start reading pages */
            _cmd_err = ERR_OK;
            _page_read = _mem_page;

            /* send output buffer */
            Endpoint_ClearSETUP();
            Endpoint_Write_Control_Stream_LE(&page, sizeof(page));
            Endpoint_ClearOUT();
            break;
        }

        /* write one page */
        case CMD_WRITE_PAGE:
        {
            /* check for page buffer */
            if (_page_ptr != _mem_page)
            {
                _cmd_err = ERR_PAGE;
                break;
            }

            /* check for overflow */
            if (_mem_addr + _mem_page > _mem_write)
            {
                _cmd_err = ERR_OVERFLOW;
                break;
            }

            /* check for memory type */
            switch (_mem_type)
            {
                /* writing FLASH page */
                case MEM_FLASH:
                {
                    _cmd_err = ERR_OK;
                    boot_page_erase_safe(_mem_addr);
                    boot_page_flash_safe(_mem_addr, (uint16_t *)_page_buffer, sizeof(_page_buffer) / 2);
                    boot_page_write_safe(_mem_addr);
                    boot_rww_enable_safe();
                    break;
                }

                /* writing EEPROM page */
                case MEM_EEPROM:
                {
                    _cmd_err = ERR_OK;
                    eeprom_write_block(_page_buffer, (void *)_mem_addr, _mem_page);
                    break;
                }

                /* other unknown types */
                default:
                {
                    _cmd_err = ERR_TYPE;
                    break;
                }
            }

            /* page info */
            uint8_t page[] = {
                _cmd_err,
                (uint8_t)(_mem_addr >> 0),
                (uint8_t)(_mem_addr >> 8),
            };

            /* move to next page if success */
            if (_cmd_err == ERR_OK)
            {
                _mem_addr += _mem_page;
                _page_ptr -= _mem_page;
            }

            /* send output buffer */
            Endpoint_ClearSETUP();
            Endpoint_Write_Control_Stream_LE(&page, sizeof(page));
            Endpoint_ClearOUT();
            break;
        }

        /* other unknown commands */
        default:
        {
            _cmd_err = ERR_CMD;
            break;
        }
    }

    /* refresh DFU ticks */
    _red = 1;
    _tick = DFU_TICKS;
    PORTB &= ~LED_RED;
}

void EVENT_USB_Device_ConfigurationChanged(void)
{
    Endpoint_ConfigureEndpoint(DFU_READ_EP, EP_TYPE_BULK, DFU_READ_SIZE, 1);
    Endpoint_ConfigureEndpoint(DFU_WRITE_EP, EP_TYPE_BULK, DFU_WRITE_SIZE, 1);
}

ISR(TIMER1_OVF_vect, ISR_BLOCK)
{
    /* tick the DFU timeout timer */
    if (_tick == 0)
    {
        _dfu = 0;
        return;
    }

    /* set the red LED flag */
    if (_red == 0)
        PORTB |= LED_RED;
    else
        PORTB ^= LED_RED;

    /* tick the blue LED timer */
    if (_blue == 3)
    {
        _blue = 0;
        PORTB ^= LED_BLUE;
    }

    /* clear red flags */
    _red = 0;
    _tick--;
    _blue++;
}
