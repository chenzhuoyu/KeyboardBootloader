#!/usr/bin/env python
# -*- coding: utf-8 -*-

import usb
import sys
import time
import struct

MEM_FLASH           = 0xb0
MEM_EEPROM          = 0xb1

MEM_FLASH_PAGE      = 128
MEM_EEPROM_PAGE     = 4

CMD_SET_ADDR        = 0x50
CMD_SET_TYPE        = 0x51
CMD_GET_ADDR        = 0xa0
CMD_GET_TYPE        = 0xa1

CMD_READ_PAGE       = 0xa2
CMD_WRITE_PAGE      = 0x52

CMD_NOP             = 0xfe
CMD_RESET           = 0xff

ERR_OK              = 0
ERR_CMD             = 0x80
ERR_LEN             = 0x81
ERR_ADDR            = 0x82
ERR_TYPE            = 0x83
ERR_ALIGN           = 0x84
ERR_PAGE            = 0x85
ERR_OVERFLOW        = 0x86

def load_hex(name, page):
    with open(name, 'rU') as fp:
        end = False
        start = None
        offset = None
        program = bytearray()

        for i, line in enumerate(fp, 1):
            if not line:
                continue

            if end:
                print '- ERROR: content after EOF.'
                exit(1)

            if line[0] != ':':
                print '- ERROR: only support Intel Hex files.'
                exit(1)

            size = int(line[1:3], 16)
            addr = int(line[3:7], 16)
            rcat = int(line[7:9], 16)
            data = bytearray(line[9:9 + size * 2].decode('hex'))
            last = int(line[9 + size * 2:11 + size * 2], 16)

            if last != (-sum(bytearray(line[1:9 + size * 2].decode('hex')), 0) & 0xff):
                print '- ERROR: checksum error on line %d.' % i
                exit(1)

            if rcat == 0x01:
                if not size:
                    end = True
                else:
                    print '- ERROR: EOF is not empty.'
                    exit(1)

            elif rcat == 0x00:
                if start is None:
                    start = addr

                if offset is not None:
                    program += bytearray('\x00') * (addr - offset)

                offset = addr + size
                program += data

    if len(program) % page == 0:
        return start, program
    else:
        npad = page - len(program) % page
        return start, program + bytearray('\x00' * npad)

def find_dev(vid, pid):
    for _ in xrange(10):
        dev = usb.core.find(
            idVendor = vid,
            idProduct = pid
        )

        if dev is not None:
            return dev
        else:
            time.sleep(1)

def poll(dev):
    return dev.ctrl_transfer(0xc0, CMD_NOP, 0, 1, 1)[0]

def reset(dev):
    return dev.ctrl_transfer(0xc0, CMD_RESET, 0, 1, 1)[0]

def get_addr(dev):
    return struct.unpack('<BH', dev.ctrl_transfer(0xc0, CMD_GET_ADDR, 0, 1, 3))

def get_mode(dev):
    return struct.unpack('<BB', dev.ctrl_transfer(0xc0, CMD_GET_TYPE, 0, 1, 2))

def set_addr(dev, addr):
    dev.ctrl_transfer(0x40, CMD_SET_ADDR, 0, 1, struct.pack('<H', addr))
    return poll(dev)

def set_mode(dev, mode):
    dev.ctrl_transfer(0x40, CMD_SET_TYPE, 0, 1, struct.pack('<B', mode))
    return poll(dev)

def read_page(dev):
    ret, size = struct.unpack('<BH', dev.ctrl_transfer(0xc0, CMD_READ_PAGE, 0, 1, 3))
    return (ERR_OK, bytearray(dev.read(0x81, 128))) if ret == ERR_OK else (ret, None)

def write_page(dev, data, page_size):
    while data:
        data = data[dev.write(0x02, data[:min([page_size, 64])]):]
    else:
        return struct.unpack('<BH', dev.ctrl_transfer(0xc0, CMD_WRITE_PAGE, 0, 1, 3))

def main():
    if len(sys.argv) != 3:
        print 'usage: %s [flash|eeprom] <file-name>' % sys.argv[0]
        exit(1)

    if sys.argv[1] == 'flash':
        mem = MEM_FLASH
        mode = 'FLASH'
        page = MEM_FLASH_PAGE
    elif sys.argv[1] == 'eeprom':
        mem = MEM_EEPROM
        mode = 'EEPROM'
        page = MEM_EEPROM_PAGE
    else:
        print 'usage: %s [flash|eeprom] <file-name>' % sys.argv[0]
        exit(1)

    print '* Loading %s image %r with page size %d ...' % (mode, sys.argv[2], page)
    start, program = load_hex(sys.argv[2], page)

    print '* Waiting for keyboard to enter DFU mode ...'
    dev = find_dev(0x01a1, 0x07c8)
    if dev is None:
        print '- ERROR: keyboard not found or not in DFU mode.'
        exit(1)

    print '* Setting address mode to %s ...' % mode
    ret = set_mode(dev, mem)
    if ret != ERR_OK:
        print '- ERROR: cannot set address mode to %s, error code: %02x.' % (mode, ret)
        exit(1)

    ret, rmem = get_mode(dev)
    if ret != ERR_OK:
        print '- ERROR: cannot verify address mode with %s, error code: %02x.' % (mode, ret)
        exit(1)
    if rmem != mem:
        print '- ERROR: cannot verify address mode with %s, mode mismatch: %02x.' % (mode, rmem)
        exit(1)

    print '* Programming ...'
    ret = set_addr(dev, start)
    if ret != ERR_OK:
        print '- ERROR: cannot set address to %s, error code: %02x.' % (mode, ret)
        exit(1)

    ret, raddr = get_addr(dev)
    if ret != ERR_OK:
        print '- ERROR: cannot verify address with %s, error code: %02x.' % (mode, ret)
        exit(1)
    if raddr != start:
        print '- ERROR: cannot verify address with %s, address mismatch: %04x.' % (mode, start)
        exit(1)

    i = 0
    addr = start
    data = program[:]
    while data:
        sys.stdout.write('\r* Writing page %d ... ' % i)
        sys.stdout.flush()

        ret, raddr = write_page(dev, data[:page], page)
        if ret != ERR_OK:
            print '\n- ERROR: cannot write to %s page %d, error code: %02x.' % (mode, i, ret)
            exit(1)
        if raddr != addr:
            print '\n- ERROR: wrong address of %s page %d: %02x.' % (mode, i, raddr)
            exit(1)

        i += 1
        addr += page
        data = data[page:]

    print
    print '* Verifying ...'
    ret = set_addr(dev, start)
    if ret != ERR_OK:
        print '- ERROR: cannot set address to %s, error code: %02x.' % (mode, ret)
        exit(1)

    ret, raddr = get_addr(dev)
    if ret != ERR_OK:
        print '- ERROR: cannot verify address with %s, error code: %02x.' % (mode, ret)
        exit(1)
    if raddr != start:
        print '- ERROR: cannot verify address with %s, mode mismatch: %04x.' % (mode, start)
        exit(1)

    i = 0
    addr = start
    data = program
    while data:
        sys.stdout.write('\r* Verifying page %d ... ' % i)
        sys.stdout.flush()

        ret, rpage = read_page(dev)
        if ret != ERR_OK:
            print '\n- ERROR: cannot verify address with %s, error code: %02x.' % (mode, ret)
            exit(1)
        if rpage != data[:page]:
            print '\n- ERROR: page %d does not match.' % i
            exit(1)

        i += 1
        addr += page
        data = data[page:]

    print
    print '* Resetting ...'
    ret = reset(dev)
    if ret != ERR_OK:
        print '- ERROR: cannot reset device, error code: %02x.' % ret
        exit(1)

    print '* Done'
    dev = None

if __name__ == '__main__':
    main()
