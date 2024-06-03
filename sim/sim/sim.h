#ifndef SIM_H
#define SIM_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define IRQ0ENABLE  0
#define IRQ1ENABLE  1
#define IRQ2ENABLE  2
#define IRQ0STATUS  3
#define IRQ1STATUS  4
#define IRQ2STATUS  5
#define IRQHANDLER  6
#define IRQRETURN   7
#define CLKS        8
#define LEDS        9
#define DISPLAY7SEG 10
#define TIMERENABLE 11
#define TIMERCURRENT 12
#define TIMERMAX    13
#define DISKCMD     14
#define DISKSECTOR  15
#define DISKBUFFER  16
#define DISKSTATUS  17
#define RESERVED0   18
#define RESERVED1   19
#define MONITORADDR 20
#define MONITORDATA 21
#define MONITORCMD  22


#define MEM_SIZE 4096
#define REG_SIZE 16
#define IO_REG_SIZE 23
#define PC_ADDR_SIZE 1
#define SECTOR_SIZE 128
#define DISK_SIZE 128
#define MONITOR_SIZE 256



struct log
{
    struct status
    {
        unsigned long cycle;
        uint16_t pc;
        uint64_t inst;
        int32_t r[REG_SIZE];
        struct status* next;

    } *status_head, * status_tail;

    struct hw_access
    {
        unsigned long cycle;
        uint8_t rw;    // read:1, write:2
        uint8_t IOReg; // 0 <= IOReg <= 22
        uint32_t data;
        struct hw_access* next;

    } *hw_head, * hw_tail;

    struct irq2in
    {
        // interrupt 2 in at cycle 'cycle'.
        unsigned long cycle;
        struct irq2in* next;

    } *irq2in_head, * irq2in_tail;
};

#endif
