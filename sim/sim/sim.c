#define _CRT_SECURE_NO_WARNINGS
#include "sim.h"

// error message macro
#define err_msg(msg) \
    fprintf(stderr, "\nError: %s\npc: %d\nline: %d\n\n", msg, pc, __LINE__);

uint16_t pc;
uint8_t irq_busy;
unsigned long disk_last_cmd_cycle;
uint64_t i_mem[MEM_SIZE];
int32_t d_mem[MEM_SIZE];
int32_t r[REG_SIZE];
uint32_t IORegister[IO_REG_SIZE];
uint32_t disk[DISK_SIZE][SECTOR_SIZE];
// disk have 128 sectors, each sector have 512 bytes or 128 lines, each line have 4 bytes
uint8_t monitor[MONITOR_SIZE][MONITOR_SIZE]; // 256x256 pixel monitor, each pixel 8-bit
struct log data_log;
unsigned long cycles;

const char* const IOLabels[] = {
    "irq0enable",
    "irq1enable",
    "irq2enable",
    "irq0status",
    "irq1status",
    "irq2status",
    "irqhandler",
    "irqreturn",
    "clks",
    "leds",
    "display7seg",
    "timerenable",
    "timercurrent",
    "timermax",
    "diskcmd",
    "disksector",
    "diskbuffer",
    "diskstatus",
    "reserved",
    "reserved",
    "monitoraddr",
    "monitordata",
    "monitorcmd",
};


/**
 * Returns: 1 if an irq2in exists in the current cycle.
 *          0 o.w.
 */
int check_irq2in()
{
    struct irq2in* ptr0;
    struct irq2in* ptr1 = data_log.irq2in_head;
    while (ptr1 != NULL && (ptr1->cycle) < cycles)
    {
        // free current ptr1 and jump to the next one (current ptr1 doesn't neccessary anymore)
        ptr0 = ptr1;
        ptr1 = ptr1->next;
        data_log.irq2in_head = ptr1;
        free(ptr0);
    }

    if (ptr1 != NULL)
        // return 1 if the next irq2in occurrs at the current cycle
        return (ptr1->cycle) == cycles;

    // there are no irq2in interrupts left
    return 0;
}

/**
 * ISR handles the processor's interrupts
 */
int interrupt_service_routine()
{
    if (irq_busy)
        return 0;

    IORegister[IRQ2STATUS] = check_irq2in(); // set irq2status interrupt

    int irq = (IORegister[IRQ0ENABLE] & IORegister[IRQ0STATUS]) |
        (IORegister[IRQ1ENABLE] & IORegister[IRQ1STATUS]) |
        (IORegister[IRQ2ENABLE] & IORegister[IRQ2STATUS]);

    if (irq == 1)
    {
        irq_busy = 1;
        IORegister[IRQRETURN] = pc;
        pc = IORegister[IRQHANDLER] & 0xfff; // pc is 12-bit
    }

    return 0;
}

/**
 * Handles processor's timer.
 * If timer enable, checks if timer current reached to timer max.
 *
 * returns: 1 if timerenable == 1 && timercurrent == timermax
 *          0 o.w.
 *
 */
int handle_timer()
{
    if (IORegister[TIMERENABLE] == 0)
        return 0;

    if (IORegister[TIMERCURRENT] == IORegister[TIMERMAX])
    {
        // timercurrent == timermax, raise irq0status to 1
        IORegister[TIMERCURRENT] = 0;
        IORegister[IRQ0STATUS] = 1;
    }
    else
        IORegister[TIMERCURRENT]++;

    return 0;
}
/**
 * copy src to dest.
 *
 * pre: *dest, *src are allocated in memory with size SECTOR_SIZE.
 * post: dest[i] == src[i] for every 0 <= i < SECTOR_SIZE
 */
int sector_copy(uint32_t* dest, uint32_t* src)
{
    uint8_t i;
    for (i = 0; i < SECTOR_SIZE; i++)
        dest[i] = src[i];
    return 0;
}

/**
 * Handle disk read/write instructions.
 */
int handle_disk()
{
    if (cycles - disk_last_cmd_cycle == 1024)
    {
        // 1024 cycles passed since the last disk read/write command.
        IORegister[DISKSTATUS] = 0; // free diskstatus
        IORegister[IRQ1STATUS] = 1; // Notify the proccessor: disk finished read or write command
    }
    
    if (IORegister[DISKSTATUS] || !IORegister[DISKCMD])
        // disk busy or doesn't want to do cmd
        return 0;


    disk_last_cmd_cycle = cycles;
    IORegister[DISKSTATUS] = 1;

    int32_t* buffer = &(d_mem[IORegister[DISKBUFFER]]);

    if (IORegister[DISKCMD] == 1)
        // diskcmd == read
        sector_copy(buffer, disk[IORegister[DISKSECTOR]]);

    else if (IORegister[DISKCMD] == 2)
        // diskcmd == write
        sector_copy(disk[IORegister[DISKSECTOR]], buffer);


    IORegister[DISKCMD] = 0; // set diskcmd=no command
    return 0;
}

/**
 * Handle monitor write instructions.
 *
 */
int handle_monitor()
{
    if (!IORegister[MONITORCMD])
        // monitorcmd == 0
        return 0;

    IORegister[MONITORCMD] = 0; // reset monitorcmd since the method about to execute write inst'.

    uint16_t monitoraddr = IORegister[MONITORADDR];
    uint8_t monitordata = IORegister[MONITORDATA];
    
    uint8_t row = monitoraddr >> 8;   // row is the 8-MSB of monitoraddr
    uint8_t col = monitoraddr & 0xff; // col is the 8-LSB of monitoraddr

    monitor[row][col] = monitordata;
    return 0;
}

/**
 * Tick the processor's clock by 1 clock unit.
 * post(clk) == 1 + pre(clk) (mod 32-bit)
 */
int tick_clk()
{
    // type(IORegister[8]) == uint32_t --> resets automatically after 0xffffffff
    IORegister[CLKS]++;
    cycles++;
    return 0;
}


/**
 * read disk file from diskin_file
 * pre: file_name is a vaild file path.
 *
 */
int read_file_diskin(char* diskin_file)
{
    FILE* fdiskin;
    fdiskin = fopen(diskin_file, "r");
    if (fdiskin == NULL)
    {
        err_msg("open file");
        return 1;
    }
    int sector, i, eof_flag = 0;
    for (sector = 0; !eof_flag && sector < DISK_SIZE; sector++)
    {
        for (i = 0; !eof_flag && i < SECTOR_SIZE; i++)
        {
            if (fscanf(fdiskin, "%X", &(disk[sector][i])) != 1)
                eof_flag = 1;
        }
    }

    if (fclose(fdiskin) != 0)
        err_msg("close file");
    return 0;
}

/**
 * write disk file from file_name
 * format:  Each line contains 32-bit data. Placed by sectors s.t. each sector owns 128 rows (512-bytes).
 * post:    The file path diskout_file lead to a valid file
 *          that containing disk data
 */
int write_file_diskout(char* diskout_file)
{
    FILE* fdiskout;
    int last_nonzero_line = -1, sector, i, eof_flag = 0;

    for (sector = 0; sector < DISK_SIZE; sector++)
    {
        for (i = 0; i < SECTOR_SIZE; i++)
        {
            if (disk[sector][i] != 0)
            {
                last_nonzero_line = SECTOR_SIZE * sector + i;
            }
        }
    }

    fdiskout = fopen(diskout_file, "w");
    if (fdiskout == NULL)
    {
        err_msg("open file");
        return 1;
    }

    if (last_nonzero_line == -1)
        eof_flag = 1;

    for (sector = 0; sector < DISK_SIZE && !eof_flag; sector++)
    {
        for (i = 0; i < SECTOR_SIZE && !eof_flag; i++)
        {
            fprintf(fdiskout, "%08X\n", disk[sector][i]);

            if (SECTOR_SIZE * sector + i >= last_nonzero_line)
                // this current line is the last line != 0, stop fprintf
                eof_flag = 1;

        }
    }

    if (fclose(fdiskout) != 0)
        err_msg("close file");
    return 0;
}

/**
 * write monitor file from file_name
 * format:  Each line contains 1 pixel (8-bit), placed by monitor rows.
 * post:    The file path diskout_file lead to a valid file
 *          that containing disk data
 */
int write_file_monitor(char* monitor_file, uint8_t is_binary)
{
    FILE* fmonitor;
    if (is_binary)
        fmonitor = fopen(monitor_file, "wb");
    else
        fmonitor = fopen(monitor_file, "w");

    if (fmonitor == NULL)
    {
        err_msg("open file");
        return 1;
    }

    if (is_binary)
    {
        fwrite(monitor, sizeof(uint8_t), MONITOR_SIZE * MONITOR_SIZE, fmonitor);

        if(fclose(fmonitor) != 0)
            err_msg("close file");

        return 0;
    }


    uint16_t i, j;
    for (i = 0; i < MONITOR_SIZE; i++)
    {
        for (j = 0; j < MONITOR_SIZE; j++)
        {

            fprintf(fmonitor, "%02X\n", monitor[i][j]);
        }
    }

    if (fclose(fmonitor) != 0)
        err_msg("close file");
    return 0;
}

/**
 * update processor's log status into a designated linked list.
 */
int update_log_status()
{
    int i;
    struct status* status_p = (struct status*)malloc(sizeof(struct status));
    if (status_p == NULL)
    {
        err_msg("malloc");
        return 1;
    }
    status_p->pc = pc;
    status_p->inst = i_mem[pc];
    status_p->next = NULL;
    for (i = 0; i < REG_SIZE; i++)
        status_p->r[i] = r[i];

    if (data_log.status_head == NULL)
    {
        data_log.status_head = status_p;
        data_log.status_tail = status_p;
    }
    else
    {
        data_log.status_tail->next = status_p;
        data_log.status_tail = status_p;
    }

    return 0;
}

/**
 * update processor's log hardware access into a designated linked list.
 * pre:     1 <= rw <= 2
 *          0 <= IOReg <= 22
 *
 * rw -     1: read
 *          2: write
 * IOReg -  I/O register index that have been accessed.
 */
int update_log_hw_access(uint8_t rw, uint8_t IOReg)
{
    uint32_t data = IORegister[IOReg];
    struct hw_access* hw_acc_p = (struct hw_access*)malloc(sizeof(struct hw_access));
    if (hw_acc_p == NULL)
    {
        err_msg("malloc");
        return 1;
    }
    hw_acc_p->cycle = cycles;
    hw_acc_p->rw = rw;
    hw_acc_p->IOReg = IOReg;
    hw_acc_p->data = data;

    hw_acc_p->next = NULL;

    // Insert the new hw_access to end of linked list
    if (data_log.hw_head == NULL)
    {
        data_log.hw_head = hw_acc_p;
        data_log.hw_tail = hw_acc_p;
    }
    else
    {
        data_log.hw_tail->next = hw_acc_p;
        data_log.hw_tail = hw_acc_p;
    }
    return 0;
}

/**
 * free log status linked list
 */
int free_log_status()
{
    struct status* ptr0, * ptr1 = data_log.status_head;

    while (ptr1 != NULL)
    {
        ptr0 = ptr1;
        ptr1 = ptr1->next;
        free(ptr0);
    }

    return 0;
}

/**
 * free log hardware access linked list
 */
int free_log_hw_access()
{
    struct hw_access* ptr0, * ptr1 = data_log.hw_head;

    while (ptr1 != NULL)
    {
        ptr0 = ptr1;
        ptr1 = ptr1->next;
        free(ptr0);
    }

    return 0;
}

/**
 * free log irq2in linked list
 */
int free_log_irq2in()
{
    // free only irq2in that we havn't reached to them due to lower cycles from their occourences.
    struct irq2in* ptr0, * ptr1 = data_log.irq2in_head;

    while (ptr1 != NULL)
    {
        ptr0 = ptr1;
        ptr1 = ptr1->next;
        free(ptr0);
    }

    return 0;
}

/**
 * reads the content of firq2in into a linked list (log irq2in)
 * s.t. each row turns into a node in the linked list.
 *
 * pre: The lines of file firq2in are at ascending order
 */
int read_file_irq2in(char* irq2in_file)
{
    FILE* firq2in;
    firq2in = fopen(irq2in_file, "r");
    if (firq2in == NULL)
    {
        err_msg("open file");
        return 1;
    }
    unsigned long i;
    while (fscanf(firq2in, "%d", &i) == 1)
    {
        // add i to the end of the linked list
        struct irq2in* irq2in_p = (struct irq2in*)malloc(sizeof(struct irq2in));
        if (irq2in_p == NULL)
        {
            err_msg("malloc");
            return 1;
        }
        irq2in_p->cycle = i;
        irq2in_p->next = NULL;

        if (data_log.irq2in_head == NULL)
        {
            data_log.irq2in_head = irq2in_p;
            data_log.irq2in_tail = irq2in_p;
        }
        else
        {
            data_log.irq2in_tail->next = irq2in_p;
            data_log.irq2in_tail = irq2in_p;
        }
    }

    if (fclose(firq2in) != 0)
        err_msg("close file");
    return 0;
}

/**
 * Reads the contend of fdmem, fimem into d_mem, i_mem.
 *
 * pre: dmem_file,imem_file are valid files paths.
 * returns  0 on success
 */
int read_file_dmem_imem(char* dmem_file, char* imem_file)
{
    FILE* fdmem, * fimem;
    fdmem = fopen(dmem_file, "r");
    fimem = fopen(imem_file, "r");
    if (fdmem == NULL || fimem == NULL)
    {
        err_msg("open file");
        return 1;
    }
    uint16_t i;
    for (i = 0; i < MEM_SIZE && fscanf(fdmem, "%x", &d_mem[i]) == 1; i++)
        ;
    for (i = 0; i < MEM_SIZE && fscanf(fimem, "%llx", &i_mem[i]) == 1; i++)
        ;

    if (fclose(fdmem) != 0 || fclose(fimem) != 0)
        err_msg("close file");
    return 0;
}

/**
 * write data memory out file to dmemout_file path
 * format:  Each line contains 32-bit data from d_mem by index order.
 * post:    The file path dmemout_file lead to a valid file
 *          that containing d_mem data
 * post:    d_mem[i] == uint32_t(line(i))
 */
int write_file_dmemout(char* dmemout_file)
{
    int i;

    int last_nonzero_line = -1;

    for (i = 0; i < MEM_SIZE; i++)
    {
        if (d_mem[i] != 0)
            last_nonzero_line = i;
    }


    FILE* fdmemout;
    fdmemout = fopen(dmemout_file, "w");

    if (fdmemout == NULL)
    {
        err_msg("open file");
        return 1;
    }
    for (i = 0; i <= last_nonzero_line; i++)
        fprintf(fdmemout, "%08X\n", d_mem[i]);

    if (fclose(fdmemout) != 0)
        err_msg("close file");
    return 0;
}

int write_file_trace(char* trace_file)
{
    FILE* ftrace;
    ftrace = fopen(trace_file, "w");

    if (ftrace == NULL)
    {
        err_msg("open file");
        return 1;
    }
    uint8_t i;
    struct status* status_p = data_log.status_head;
    while (status_p != NULL)
    {
        // write to trace file
        fprintf(ftrace, "%03X ", status_p->pc);     // pc
        fprintf(ftrace, "%012llX ", status_p->inst); // inst
        for (i = 0; i < REG_SIZE - 1; i++)
            fprintf(ftrace, "%08x ", status_p->r[i]); // R[0] ... R[14]
        fprintf(ftrace, "%08x\n", status_p->r[i]);    // R[15]

        // jump to next status
        status_p = status_p->next;
    }

    if (fclose(ftrace) != 0)
        err_msg("close file");
    return 0;
}

/**
 * Writes to file paths: hwregtrace_file, leds_file, display7seg_file.
 * Writes to leds_file and display7seg_file only WRITE instructions log.
 *
 * hwregrage format:    cycle READ/WRITE IORegister data
 * leds format:         cycle data
 * display7seg format:  cycle data
 *
 * returns: 0 on success
 */
int write_file_hwregtrace_leds_display7seg(char* hwregtrace_file, char* leds_file, char* display7seg_file)
{
    FILE* fhwregtrace, * fleds, * fdisplay7seg;
    fhwregtrace = fopen(hwregtrace_file, "w");
    fleds = fopen(leds_file, "w");
    fdisplay7seg = fopen(display7seg_file, "w");
    if (fhwregtrace == NULL || fleds == NULL || fdisplay7seg == NULL)
    {
        err_msg("open file");
        return 1;
    }

    struct hw_access* hw_p = data_log.hw_head;
    char text_read[] = "READ", text_write[] = "WRITE";
    while (hw_p != NULL)
    {
        // write file hwregtrace
        fprintf(fhwregtrace, "%lu ", hw_p->cycle);

        if (hw_p->rw == 1)
            fprintf(fhwregtrace, "%s ", text_read);
        else if (hw_p->rw == 2)
            fprintf(fhwregtrace, "%s ", text_write);

        fprintf(fhwregtrace, "%s ", IOLabels[hw_p->IOReg]);
        fprintf(fhwregtrace, "%08x\n", hw_p->data);

        if (hw_p->rw == 2)
        {
            // only WRITE hw access
            if (hw_p->IOReg == LEDS)
            {
                // write file leds
                fprintf(fleds, "%lu ", hw_p->cycle);
                fprintf(fleds, "%08x\n", hw_p->data);
            }
            else if (hw_p->IOReg == DISPLAY7SEG)
            {
                // write file display7seg (
                fprintf(fdisplay7seg, "%lu ", hw_p->cycle);
                fprintf(fdisplay7seg, "%08x\n", hw_p->data);
            }
        }

        hw_p = hw_p->next;
    }

    if (fclose(fhwregtrace) != 0 || fclose(fleds) != 0 || fclose(fdisplay7seg) != 0)
        err_msg("close file");

    return 0;
}

/**
 * Write to file paths: cycles_file, regout_file
 *
 * cycles format:   Number of cycles took to finish the execution.
 * regout format:   last state of the registers [R3 R4 ... R15] (new line each)
 *
 * returns: 0 on success
 */
int write_file_cycles_regout(char* cycles_file, char* regout_file)
{
    FILE* fcycles, * fregout;
    fcycles = fopen(cycles_file, "w");
    fregout = fopen(regout_file, "w");

    if (fcycles == NULL || fregout == NULL)
    {
        err_msg("open file");
        return 1;
    }

    fprintf(fcycles, "%lu\n", cycles);
    uint8_t i;
    for (i = 3; i < REG_SIZE; i++)
        fprintf(fregout, "%08x\n", r[i]);

    if (fclose(fcycles) != 0 || fclose(fregout) != 0)
        err_msg("close file");

    return 0;
}

/**
 * extend sign from left to register
 * i.e. reg=0x00000fab, sign_bit=11 -> result=0xffffffab
 *
 */
uint32_t extend_sign(uint32_t reg, uint8_t sign_bit)
{
    int sign = (reg >> sign_bit) & 1;
    if (sign)
        reg |= ~0 << sign_bit;
    return reg;
}
/**
 * Execute the instruction in the current program counter (pc)
 *
 * returns  0 - executed instruction successfully without halt.
 *          1 - executed halt instruction
 *          2 - received invalid opcode.
 * */
int execute_instruction()
{

    // Made jump or branch instruction.
    uint64_t inst = i_mem[pc];
    uint16_t prev_pc = pc; // later check if instruction changed pc -> don't increament by 1
    uint16_t opcode, rd, rs, rt, rm, imm1, imm2;
    imm2 = (inst >> 0) & 0xfff;
    imm1 = (inst >> 12) & 0xfff;
    rm = (inst >> 24) & 0xf;
    rt = (inst >> 28) & 0xf;
    rs = (inst >> 32) & 0xf;
    rd = (inst >> 36) & 0xf;
    opcode = (inst >> 40) & 0xff;

    if (opcode < 0 || opcode > 21)
        return 2;

    r[0] = 0;                     // constant register
    r[1] = extend_sign(imm1, 11); // immediate register (with extended sign)
    r[2] = extend_sign(imm2, 11); // immediate register (with extended sign)

    update_log_status();

    switch (opcode)
    {
        // add
    case 0:
        r[rd] = r[rs] + r[rt] + r[rm];
        break;

        // sub
    case 1:
        r[rd] = r[rs] - r[rt] - r[rm];
        break;

        // mac
    case 2:
        r[rd] = r[rs] * r[rt] + r[rm];
        break;

        // and
    case 3:
        r[rd] = r[rs] & r[rt] & r[rm];
        break;

        // or
    case 4:
        r[rd] = r[rs] | r[rt] | r[rm];
        break;

        // xor
    case 5:
        r[rd] = r[rs] ^ r[rt] ^ r[rm];
        break;

        // sll
    case 6:
        r[rd] = r[rs] << r[rt];
        break;

        // sra
    case 7:
        r[rd] = r[rs] >> r[rt];
        r[rd] = extend_sign(r[rd], 31 - r[rt]);
        break;

        // srl
    case 8:
        r[rd] = r[rs] >> r[rt];
        break;

        // beq
    case 9:
        if (r[rs] == r[rt])
            pc = r[rm] & 0xfff;
        break;

        // bne
    case 10:
        if (r[rs] != r[rt])
            pc = r[rm] & 0xfff;
        break;

        // blt
    case 11:
        if (r[rs] < r[rt])
            pc = r[rm] & 0xfff;
        break;

        // bgt
    case 12:
        if (r[rs] > r[rt])
            pc = r[rm] & 0xfff;

        break;

        // ble
    case 13:
        if (r[rs] <= r[rt])
            pc = r[rm] & 0xfff;
        break;

        // bge
    case 14:
        if (r[rs] >= r[rt])
            pc = r[rm] & 0xfff;
        break;

        // jal
    case 15:
        r[rd] = (pc + 1) & 0xfff;
        pc = r[rm] & 0xfff;
        break;

        // lw
    case 16:
        r[rd] = d_mem[(r[rs] + r[rt]) & 0xfff] + r[rm];
        break;

        // sw
    case 17:
        d_mem[(r[rs] + r[rt]) & 0xfff] = r[rm] + r[rd];
        break;

        // reti
    case 18:
        pc = IORegister[7];
        irq_busy = 0;
        break;

        // in
    case 19:
        if (r[rs] + r[rt] >= IO_REG_SIZE)
            break;
        r[rd] = IORegister[r[rs] + r[rt]];
        update_log_hw_access(1, r[rs] + r[rt]);
        break;

        // out
    case 20:

        if (r[rs] + r[rt] >= IO_REG_SIZE)
            break;

        IORegister[r[rs] + r[rt]] = r[rm];
        update_log_hw_access(2, r[rs] + r[rt]);
        break;

        // halt
    case 21:
        return 1;
    }

    if (prev_pc == pc)
        // if pc changed due to instruction, increamenting pc isn't needed.
        // i.e. branch, jal, reti.
        pc = (pc + PC_ADDR_SIZE) & 0xfff; // pc is 12-bit

    r[0] = 0;  // constant register

    return 0;
}

/**
 * Reads input files into program's data structures.
 *
 * pre: imemin_path, dmemin_path, diskin_path, irq_path are valid file paths.
 * returns: 0 on success
 *          1 on failure
 */
int init(char* imemin_path, char* dmemin_path, char* diskin_path, char* irq_path)
{
    pc = 0;
    cycles = 0;
    data_log.status_head = NULL;
    data_log.hw_head = NULL;
    data_log.irq2in_head = NULL;
    irq_busy = 0;
    disk_last_cmd_cycle = ~0;

    if (read_file_dmem_imem(dmemin_path, imemin_path) != 0 ||
        read_file_diskin(diskin_path) != 0 ||
        read_file_irq2in(irq_path))
        return 1;

    return 0;
}

/**
 * Writes output files and free dynamically allocated memory.
 * post:    dmemout_path, regout_path, trace_path, hwregtrace_path, cycles_path,
 *          leds_path, display7seg_path, diskout_path, monitor_txt_path, monitor_yuv_path
 *          -> are paths to valid files containing program's output data.
 *
 * returns 0 on success
 */
int finalization(char* dmemout_path, char* regout_path, char* trace_path, char* hwregtrace_path, char* cycles_path,
    char* leds_path, char* display7seg_path, char* diskout_path, char* monitor_txt_path, char* monitor_yuv_path)
{

    if (write_file_dmemout(dmemout_path) != 0 ||
        write_file_diskout(diskout_path) != 0 ||
        write_file_trace(trace_path) != 0 ||
        write_file_hwregtrace_leds_display7seg(hwregtrace_path, leds_path, display7seg_path) != 0 ||
        write_file_cycles_regout(cycles_path, regout_path) != 0 ||
        write_file_monitor(monitor_txt_path, 0) != 0 ||
        write_file_monitor(monitor_yuv_path, 1) != 0)
        return 1;

    free_log_status();
    free_log_hw_access();
    free_log_irq2in();
    return 0;
}

int main(int argc, char* argv[])
{
    // execute with: 
    // sim.exe imemin.txt dmemin.txt diskin.txt irq2in.txt dmemout.txt regout.txt trace.txt hwregtrace.txt cycles.txt leds.txt display7seg.txt diskout.txt monitor.txt monitor.yuv
    if (argc != 15)
        return 1;

    if (init(argv[1], argv[2], argv[3], argv[4]) != 0)
        return 1;


    int halt_flag = 0;

    while (pc < MEM_SIZE && !halt_flag)
    {
        switch (execute_instruction())
        {
        case 1:
            // HALT
            halt_flag = 1;
            break;

        case 2:
            // Error: Invalid opcode.
            err_msg("Invalid opcode");
            return 1;
        }

        handle_monitor();
        handle_timer();
        handle_disk();

        interrupt_service_routine();

        tick_clk(); // iteration's end!
    }

    if (finalization(argv[5], argv[6], argv[7], argv[8], argv[9], argv[10], argv[11], argv[12], argv[13], argv[14]) != 0)
        return 1;

    return 0;
}