#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "cpu.h"

void pbyte(const uint8_t b) {
  printf("0x%02hhX\n", b);
}

void pshort(const uint16_t s) {
  printf("0x%04hX\n", s);
}

static uint16_t load_16 (const size_t index, const uint8_t* mem) {
  return (mem[index + 1] << 8) | mem[index];
}

static uint16_t load_d16 (const struct cpu* const lr35902) {
  uint16_t pc = lr35902->registers.pc;
  uint8_t* mem = lr35902->memory;

  size_t pc8 = pc << 1;
  size_t addr_of_d16 = pc8 + 1;
  uint16_t d16 = load_16(addr_of_d16, mem);
  return d16;
}

static void LD_SP_d16 (struct cpu* const lr35902) {
  printf("LD SP,d16\n");
  pshort(lr35902->registers.sp);
  lr35902->registers.sp = load_d16(lr35902);
  pshort(lr35902->registers.sp);
  lr35902->registers.pc += 3;
}

/*opcodes[0x31] = &LD_SP_d16;*/
static const instr opcodes [256] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 1x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 2x
  0, &LD_SP_d16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 3x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 4x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 5x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 6x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 7x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 8x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 9x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Ax
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Bx
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Cx
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Dx
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Ex
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  // Fx
};

instr decode (const struct cpu* const cpu) {
  uint16_t pc = cpu->registers.pc;
  uint8_t opcode = cpu->memory[pc];
  pbyte(opcode);
  instr i = opcodes[opcode];
  // unknown instr
  assert(i != 0);
  return i;
}



/*void handle_cb_prefix (struct cpu* const lr35902, uint8_t* const memory, uint8_t* pc) {*/
  /*[>printf("CB ");<]*/
  /*switch (*pc) {*/
    /*case 0x7C:*/
      /*printf("BIT 7, H\n");*/
      /*printf("h: %d\n", lr35902->registers.h);*/
      /*printf("z: %d\n", lr35902->registers.f.z);*/
      /*lr35902->registers.f.z = (lr35902->registers.h & (1 << 6)) != 0;*/
      /*printf("z: %d\n", lr35902->registers.f.z);*/
      /*lr35902->registers.f.n = 0;*/
      /*lr35902->registers.f.h = 0;*/
      /*break;*/
    /*default:*/
      /*printf("unknown\n");*/
      /*break;*/
  /*}*/
/*}*/

/*// print 8b register*/
/*#define PR8(reg) pbyte(lr35902.registers.reg)*/

/*void handle_instr (struct cpu* const lr35902, uint8_t* const memory) {*/
  /*// http://pastraiser.com/cpu/gameboy/gameboy_opcodes.html*/
  /*switch (lr35902->registers.pc) {*/
    /*case 0x0E:*/
      /*printf("LD C,d8\n");*/
      /*PR8(c);*/
      /*lr35902.registers.c = *(lr35902->registers.pc + 1);*/
      /*PR8(c);*/
      /*lr35902->registers.pc += 2;*/
      /*break;*/
    /*case 0x20:*/
      /*// Jump Relative*/
      /*// relative to the end of the full instruction*/
      /*printf("JR NZ,r8\n");*/
      /*printf("where r8 == %d\n", (signed char) *(lr35902->registers.pc + 1));*/
      /*if (lr35902->registers.f.z) {*/
        /*printf("not jumping\n");*/
        /*lr35902->registers.pc += 2;*/
        /*[>pc += (signed char) *(pc + 1);<]*/
      /*} else {*/
        /*printf("jumping\n");*/
        /*lr35902->registers.pc += ((signed char) *(lr35902->registers.pc + 1)) + 2;*/
      /*}*/
      /*// minus two because the instruction length of 0x20 is 2,*/
      /*// and we haven't modified the pc yet.*/
      /*[>pc += lr35902.registers.f.z ? 2 : ((signed char) *(pc + 1)) + 2;<]*/
      /*break;*/
    /*case 0x21:*/
      /*printf("LD HL,d16\n");*/
      /*pshort(lr35902.registers.hl);*/
      /*lr35902.registers.hl = *(uint16_t*)(lr35902->registers.pc + 1);*/
      /*pshort(lr35902.registers.hl);*/
      /*lr35902->registers.pc += 3;*/
      /*break;*/
      /*break;*/
    /*case 0x32:*/
      /*printf("LD (HL-),A\n");*/
      /*[>pshort(lr35902.registers.hl);<]*/
      /*pbyte(memory[lr35902.registers.hl]);*/
      /*memory[lr35902.registers.hl] = lr35902.registers.a;*/
      /*pbyte(memory[lr35902.registers.hl]);*/
      /*pshort(lr35902.registers.hl);*/
      /*--lr35902.registers.hl;*/
      /*pshort(lr35902.registers.hl);*/
      /*++lr35902->registers.pc;*/
      /*break;*/
    /*case 0xAF:*/
      /*printf("XOR A\n");*/
      /*lr35902.registers.a = 0;*/
      /*++lr35902->registers.pc;*/
      /*break;*/
      /*// these use length-2 instructions always*/
    /*case 0xCB:*/
      /*handle_cb_prefix(&lr35902, memory, ++lr35902->registers.pc);*/
      /*++lr35902->registers.pc;*/
      /*break;*/
    /*default:*/
      /*printf("unknown instruction ");*/
      /*pbyte(*lr35902->registers.pc);*/
      /*++lr35902->registers.pc;*/
      /*break;*/
  /*}*/
/*}*/
