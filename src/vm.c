#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

// Registers
enum
{
    R_R0,
    R_R1,
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
    R_PC,
    R_COND,
    R_COUNT
};

// Instructions
enum
{
    OP_BR = 0, // branch
    OP_ADD,    // add
    OP_LD,     // load
    OP_ST,     // store
    OP_JSR,    // jump register
    OP_AND,    // bitwise and
    OP_LDR,    // load register
    OP_STR,    // store register
    OP_RTI,    // unused
    OP_NOT,    // bitwise not
    OP_LDI,    // load indirect
    OP_STI,    // store indirect
    OP_JMP,    // jump
    OP_RES,    //reserved
    OP_LEA,    // load effective address
    OP_TRAP,   // trap
};

// Condition flags
enum
{
    FL_POS = 1 << 0, // P 001
    FL_ZRO = 1 << 1, // Z 010
    FL_NEG = 1 << 2, // N 100
};

// Trap codes
enum
{
    TRAP_GETC = 0x20,  // Get char from keyboard. Not echoed onto term
    TRAP_OUT = 0x21,   // Output a char
    TRAP_PUTS = 0x22,  // Output a word string
    TRAP_IN = 0x23,    // Get char from keyboard, echo onto term
    TRAP_PUTSP = 0x24, // Output a byte string
    TRAP_HALT = 0x25,  // Halt program
};

uint16_t memory[UINT16_MAX];
uint16_t registers[R_COUNT];

void mem_write(uint16_t addr, uint16_t val)
{
    memory[addr] = val;
}

uint16_t mem_read(uint16_t addr)
{
    return memory[addr];
}

uint16_t sign_extend(uint16_t x, int bit_count)
{
    // If the most significant bit is 1 (x is negative number)
    if ((x >> (bit_count - 1)) & 1)
    {
        // Shift all 1 bits over by x's bit count and fill in the space with x's data
        x |= (0xFFFF << bit_count);
    }
    return x;
}

void update_condition(uint16_t r)
{
    if (registers[r] == 0)
        registers[R_COND] = FL_ZRO;
    else if (registers[r] >> 15) // If left most bit is 1, the number is negative
        registers[R_COND] = FL_NEG;
    else
        registers[R_COND] = FL_POS;
}

uint16_t swap16(uint16_t x)
{
    return (x << 8) | (x >> 8);
}

void read_image_file(FILE *file)
{

    // Memory origin where image should be placed
    uint16_t origin;

    // Read 16 bits into 'origin' from 'file'
    fread(&origin, sizeof(origin), 1, file);

    // Swap endianness
    origin = swap16(origin);

    // Max memory to read
    uint16_t max_read = UINT16_MAX - origin;

    // Point to origin
    uint16_t *p = memory + origin;

    // Set stream file into *p
    size_t read = fread(p, sizeof(uint16_t), max_read, file);

    while (read-- > 0)
    {
        *p = swap16(*p);
        ++p;
    }
}

void read_image(const char *image_path)
{
    FILE *file = fopen(image_path, "rb");
    if (!file)
        return 0;

    read_image_file(file);
    fclose(file);
    return 1;
}

int main(int argc, const char *argv[])
{

    // Check for code passed to vm
    if (argc < 2)
    {
        printf("lc3 [image-file1] ..\n");
        exit(2);
    }

    // Make sure programs can be read
    for (int j = 1; j < argc; j++)
    {
        if (!read_image(argv[j]))
        {
            printf("failed to load image: %s\n", argv[j]);
            exit(2);
        }
    }

    // Set program counter to starting position
    enum
    {
        PC_START = 0x3000
    };
    registers[R_PC] = PC_START;

    int running = 1;
    while (running)
    {
        // Read instruction at program counter and increment
        uint16_t instruction = mem_read(registers[R_PC]++);
        uint16_t op = instruction >> 12;

        // Execute op
        switch (op)
        {
        case OP_ADD:
        {
            //  Get DR, bits 9 to 11
            uint16_t r0 = (instruction >> 9) & 0x7;

            // Get SR1 (left operand), bits 6 to 8
            uint16_t r1 = (instruction >> 6) & 0x7;

            // Read the imm flag in bit 5
            uint16_t imm_flag = (instruction >> 5) & 0x1;

            if (imm_flag)
            {
                // sign extend the right most 5 bits
                uint16_t imm5 = sign_extend(instruction & 0x1F, 5);

                // Add values in registers
                registers[r0] = registers[r1] + imm5;
            }
            else
            {
                // Get SR2 (right operand), bits 0 to 2
                uint16_t r2 = instruction & 0x7;

                // Add values in registers
                registers[r0] = registers[r1] + registers[r2];
            }

            // Update conditional register using value from destination register
            update_condition(r0);
        }
        break;
        case OP_AND:
        {
            //  Get DR, bits 9 to 11
            uint16_t r0 = (instruction >> 9) & 0x7;

            // Get SR1 (left operand), bits 6 to 8
            uint16_t r1 = (instruction >> 6) & 0x7;

            // Read the imm flag in bit 5
            uint16_t imm_flag = (instruction >> 5) && 0x1;

            if (imm_flag)
            {
                // Sign extend right most 5 bits
                uint16_t imm5 = sign_extend(instruction & 0x1F, 5);

                // Bitwise and of value in SR1 and imm5
                registers[r0] = registers[r1] & imm5;
            }
            else
            {
                // Get SR2, bits 0 to 2
                uint16_t r2 = instruction & 0x7;

                registers[r0] = registers[r1] & registers[r2];
            }

            update_condition(r0);
        }
        break;
        case OP_NOT:
        {
            // Get DR, bits 9 to 11
            uint16_t r0 = (instruction >> 9) & 0x7;

            // Get SR, bits 6 to 8
            uint16_t r1 = (instruction >> 6) & 0x7;

            // Flip bits
            registers[r0] = ~registers[r1];
            update_condition(r0);
        }
        break;
        case OP_BR:
        {
            // Get conditional bits 9 to 11
            uint16_t cond = (instruction >> 9) & 0x7;

            // Mask cond bits with conditional register
            if (cond & registers[R_COND])
                registers[R_PC] += sign_extend(instruction & 0x1FF, 9);
        }
        break;
        case OP_JMP:
        {
            // Get BaseR bits 6 to 8
            uint16_t br = (instruction >> 6) & 0x7;

            // Set PC to br
            registers[R_PC] = registers[br];
        }
        break;
        case OP_JSR:
        {
            // Save PC to r7
            registers[R_R7] = registers[R_PC];

            // Read bit 11
            uint16_t b11 = (instruction >> 11) & 0x1;

            if (b11)
            {
                // Set PC to PC (saved in r7) + sign extension of the last 11 bits
                registers[R_PC] += sign_extend(instruction & 0x7FF, 11);
            }
            else
            {
                // Get BaseR, bits 6 to 8
                uint16_t br = (instruction >> 6) & 0x7;

                // Set PC to value in br
                registers[R_PC] = registers[br];
            }
        }
        break;
        case OP_LD:
        {
            // Get DR, bits 9 to 11
            uint16_t r0 = (instruction >> 9) & 0x7;

            // Sign extend right most 9 bits
            uint16_t offset = sign_extend(instruction & 0x1FF, 9);

            // Put the value at address PC + offset into DR
            registers[r0] = mem_read(registers[R_PC] + offset);

            update_condition(r0);
        }
        break;
        case OP_LDI:
        {
            //  Get DR, bits 9 to 11
            uint16_t r0 = (instruction >> 9) & 0x7;

            // Sign extend the last 9 bits to get offset
            uint16_t offset = sign_extend(instruction & 0x1FF, 9);

            // Get address from memory at location PC + OFFSET
            uint16_t addr = mem_read(registers[R_PC] + offset);

            // Populate DR with value at addr
            r0 = mem_read(addr);

            update_condition(r0);
        }
        break;
        case OP_LDR:
        {
            //  Get DR, bits 9 to 11
            uint16_t r0 = (instruction >> 9) & 0x7;

            // Get BaseR, bits 6 to 8
            uint16_t br = (instruction >> 6) & 0x7;

            // Sign extend right most 6 bits for offset
            uint16_t offset = sign_extend(instruction & 0x3F, 6);

            // Populate DR with value in base register br + offset
            registers[r0] = mem_read(registers[br] + offset);

            update_condition(r0);
        }
        break;
        case OP_LEA:
        {
            //  Get DR, bits 9 to 11
            uint16_t r0 = (instruction >> 9) & 0x7;

            // Sign extend the last 9 bits to get offset
            uint16_t offset = sign_extend(instruction & 0x1FF, 9);

            // Populate DR with address calculated by PC + offset
            registers[r0] = registers[R_PC] + offset;

            update_condition(r0);
        }
        break;
        case OP_ST:
        {
            //  Get SR, bits 9 to 11
            uint16_t r0 = (instruction >> 9) & 0x7;

            // Sign extend the last 9 bits to get offset
            uint16_t offset = sign_extend(instruction & 0x1FF, 9);

            // Write value in r0 to mem addr PC + offset
            mem_write(registers[R_PC] + offset, registers[r0]);
        }
        break;
        case OP_STI:
        {
            //  Get SR, bits 9 to 11
            uint16_t r0 = (instruction >> 9) & 0x7;

            // Sign extend the last 9 bits to get offset
            uint16_t offset = sign_extend(instruction & 0x1FF, 9);

            // Get address by reading memory at PC + offset
            uint16_t addr = mem_read(registers[R_PC] + offset);

            // Write value in r0 to address at the address PC + offset
            mem_write(addr, registers[r0]);
        }
        break;
        case OP_STR:
        {
            //  Get SR, bits 9 to 11
            uint16_t r0 = (instruction >> 9) & 0x7;

            // Get BaseR, bits 6 to 8
            uint16_t br = (instruction >> 6) & 0x7;

            // Sign extend the last 6 bits to get offset
            uint16_t offset = sign_extend(instruction & 0x2F, 6);

            // Write value in r0 to addr of BaseR + offset
            mem_write(registers[br] + offset, registers[r0]);
        }
        break;
        case OP_TRAP:
        {
            switch (instruction & 0xFF)
            {
            case TRAP_GETC:
            {
                // Store ascii char in register 0
                registers[R_R0] = (uint16_t)getchar();
            }
            break;
            case TRAP_OUT:
            {
                putc((char)registers[R_R0], stdout);
                fflush(stdout);
            }
            break;
            case TRAP_PUTS:
            {
                // Get memory
                uint16_t *c = memory + registers[R_R0];
                while (*c)
                {
                    putc((char)*c, stdout);
                    ++c;
                }

                fflush(stdout);
            }
            break;
            case TRAP_IN:
            {
                printf("Enter a character: ");
                char c = getchar();
                putc(c, stdout);
                registers[R_R0] = (uint16_t)c;
            }
            break;
            case TRAP_PUTSP:
            {
                uint16_t *c = memory + registers[R_R0];
                while (*c)
                {
                    // First 8 bits
                    char c1 = (*c) & 0xFF;
                    putc(c1, stdout);

                    // Next 8 bits
                    char c2 = (*c) >> 8;
                    if (c2)
                        putc(c2, stdout);

                    ++c;
                }
                fflush(stdout);
            }
            break;
            case TRAP_HALT:
            {
                puts("HALT");
                fflush(stdout);
                running = 0;
            }
            break;
            }
        }
        break;
        case OP_RES:
        case OP_RTI:
        default:
        {
        }
        break;
        }
    }
}