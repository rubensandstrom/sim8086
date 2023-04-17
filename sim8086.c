/******************************************************************************
 * THIS IS A SIMULATOR OF THE INTEL 8086 PROCESSOR 
 ******************************************************************************/
/******************************************************************************
 * Notes and thoughts.
 ******************************************************************************/
/*
 * Mabye make rm_im and reg_rm into same function with check for im or rm, this 
 * can be done by cheking the highest bit in the first byte and if it is 0 set
 * im to the adress of reg[tmp & REG] and then chek the d bit to know what order
 * to set the argument in the call to arithmetic. (will look into later)
 *
 * es = 0b00 cs = 0b01 ss = 0b10 ds = 0b11
*/
/******************************************************************************
 * Library includes
 ******************************************************************************/

#include <stdint.h> // for uint8_t and uint16_t
#include <stdio.h> // for open and close
#include <stdlib.h> // for early exit

/******************************************************************************
 * Bit patterns
 ******************************************************************************/

#define CARRY_FLAG          1
#define PARITY_FLAG         (1<<2)
#define AUXILIARY_FLAG      (1<<4)
#define ZERO_FLAG           (1<<6)
#define SIGN_FLAG           (1<<7)
#define TRAP_FLAG           (1<<8)
#define INTERUPT_FLAG       (1<<9)
#define DIRECTIONAL_FLAG    (1<<10)
#define OVERFLOW_FLAG       (1<<11)

#define W_BIT                1
#define D_BIT                (1<<1)
#define S_BIT                (1<<1)

#define MOD                 0b11000000
#define OP                  0b00111000
#define REG                 0b00111000
#define RM                  0b00000111

#define AL                  0
#define CL                  2
#define DL                  4
#define BL                  6
#define AH                  1
#define CH                  3
#define DH                  5
#define BH                  7

#define AX                  0
#define CX                  1
#define DX                  2
#define BX                  3
#define SP                  4
#define BP                  5
#define SI                  6
#define DI                  7

#define ES                  0
#define CS                  1
#define SS                  2
#define DS                  3

/******************************************************************************
 * GLOBAL VARIABLES 
 ******************************************************************************/
// MAIN MEMORY
uint8_t memory[1048576]; // 1Mb of ram memory

// REGISTERS
typedef union {
  uint16_t wide[8];
  uint8_t byte[8]; // index needs to be formated with offset
} Register; 
const uint8_t offset[8] = {0, 2, 4, 6, 1, 3, 5, 7}; // offset for memory.byte

Register reg;
uint16_t seg[4] = {0x0, 0x4000, 0x8000, 0xC000};
uint16_t flags;
uint16_t ip;

uint8_t c; // current byte

/******************************************************************************
 * EFFECTIVE ADDRESS CALCULATION
 * AND MOD RM
 ******************************************************************************/

uint16_t bx_si() { return (reg.wide[3] + reg.wide[6]); }
uint16_t bx_di() { return (reg.wide[3] + reg.wide[7]); }
uint16_t bp_si() { return (reg.wide[5] + reg.wide[6]); }
uint16_t bp_di() { return (reg.wide[5] + reg.wide[7]); }
uint16_t si() { return reg.wide[6]; }
uint16_t di() { return reg.wide[7]; }
uint16_t bp() { return reg.wide[5]; }
uint16_t bx() { return reg.wide[3]; }

uint16_t (*effective_address_calculation[8])() = {bx_si, bx_di, bp_si, bp_di, si, di, bp, bx};

uint8_t *mod_byte(uint8_t mod_index, uint8_t rm_index) {
  uint8_t *rm;
  uint16_t displacement;
  switch (mod_index) {
    case 0b00:
      if (rm_index == 0b110) { // DIRECT ADDRESS
        displacement = memory[(seg[CS] << 4) + ip++];
        displacement |= memory[(seg[CS] << 4) + ip++] << 8;
      } else {
        displacement = effective_address_calculation[rm_index]();
      }
      rm = &memory[(seg[DS] << 4) + displacement]; // should mabye check for missalignment and oob.
      break;
    case 0b01:
      displacement = memory[(seg[CS] << 4) + ip++];
      displacement += effective_address_calculation[rm_index]();
      rm = &memory[(seg[DS] << 4) + displacement]; 
      break;
    case 0b10:
      displacement = memory[(seg[CS] << 4) + ip++];
      displacement |= memory[(seg[CS] << 4) + ip++] << 8;
      displacement += effective_address_calculation[rm_index]();
      rm = &memory[(seg[DS] << 4) + displacement]; 
      break;
    case 0b11:
      rm = &reg.byte[offset[rm_index]];
      break;
  }
  return rm;
}
uint16_t *mod_wide(uint8_t mod_index, uint8_t rm_index) {
  uint16_t *rm;
  uint16_t displacement;
  switch (mod_index) {
    case 0b00:
      if (rm_index == 0b110) { // DIRECT ADDRESS
        displacement = memory[(seg[CS] << 4) + ip++];
        displacement |= memory[(seg[CS] << 4) + ip++] << 8;
      } else {
        displacement = effective_address_calculation[rm_index]();
      }
      rm = (uint16_t *)&memory[(seg[DS] << 4) + displacement]; // should mabye check for missalignment and oob.
      break;
    case 0b01:
      displacement = memory[(seg[CS] << 4) + ip++];
      displacement += effective_address_calculation[rm_index]();
      rm = (uint16_t *)&memory[(seg[DS] << 4) + displacement]; 
      break;
    case 0b10:
      displacement = memory[(seg[CS] << 4) + ip++];
      displacement |= memory[(seg[CS] << 4) + ip++] << 8;
      displacement += effective_address_calculation[rm_index]();
      rm = (uint16_t *)&memory[(seg[DS] << 4) + displacement]; 
      break;
    case 0b11:
      rm = &reg.wide[rm_index];
      break;
  }
  return rm;
}
/******************************************************************************
 *  ARITHMETIC
 ******************************************************************************/

void set_flags_byte(uint8_t before, uint8_t after, uint8_t source) {
  if (after < before) { flags |= CARRY_FLAG; } else { flags &= ~CARRY_FLAG; }
  uint8_t ones = 0;
  for (int i = 7; i >= 0; i--) {
    if (after & (1 << i)) { ones++; }
  }
  if ( ones % 2 == 0) { flags |= PARITY_FLAG; } else { flags &= ~PARITY_FLAG; }
  if (before & 0b1111 + source & 0b1111 > 0b1111) { flags |= AUXILIARY_FLAG; } else { flags &= ~AUXILIARY_FLAG; } 
  if (after == 0) { flags |= ZERO_FLAG; } else { flags &= ~ZERO_FLAG; }
  if (after & (1<<7)) { flags |= SIGN_FLAG; } else { flags &= ~SIGN_FLAG; }
  if ((before & (1<<7) && source & (1<<7) && !(after & (1<<7))) || 
      (!(before & (1<<7)) && !(source & (1<<7)) && after & (1<<7))) {
    flags |= OVERFLOW_FLAG;
  } else {
    flags &= ~OVERFLOW_FLAG;
  }
}

void set_flags_wide(uint16_t before, uint16_t after, uint16_t source) {
  if (after < before) { flags |= CARRY_FLAG; } else { flags &= ~CARRY_FLAG; }
  uint8_t ones = 0;
  for (int i = 15; i >= 0; i--) {
    if (after & (1 << i)) { ones++; }
  }
  if ( ones % 2 == 0) { flags |= PARITY_FLAG; } else { flags &= ~PARITY_FLAG; }
  if (before & 0b1111 + source & 0b1111 > 0b1111) { flags |= AUXILIARY_FLAG; } else { flags &= ~AUXILIARY_FLAG; } 
  if (after == 0) { flags |= ZERO_FLAG; } else { flags &= ~ZERO_FLAG; }
  if (after & (1 << 15)) { flags |= SIGN_FLAG; } else { flags &= ~SIGN_FLAG; }
  if ((before & (1 << 15) && source & (1 << 15) && !(after & (1 << 15))) || 
      (!(before & (1 << 15)) && !(source & (1 << 15)) && after & (1 << 15))) {
    flags |= OVERFLOW_FLAG;
  } else {
    flags &= ~OVERFLOW_FLAG;
  }
}

void add_byte(uint8_t *x, uint8_t *y) { 
  uint8_t before = *x;
  *x += *y; 
  set_flags_byte(before, *x, *y);
}

void or_byte(uint8_t *x, uint8_t *y) {
  uint8_t before = *x;
  *x |= *y;
  set_flags_byte(before, *x, *y);
}

void adc_byte(uint8_t *x, uint8_t *y) { 
  uint8_t before = *x;
  *x += *y; 
  if (flags & CARRY_FLAG) *x += 1; 
  set_flags_byte(before, *x, *y);
}

void sbb_byte(uint8_t *x, uint8_t *y) {
  uint8_t before = *x;
  *x -= *y;
  if (flags & CARRY_FLAG) *x -= 1;
  set_flags_byte(before, *x, *y);
}

void and_byte(uint8_t *x, uint8_t *y) {
  uint8_t before = *x;
  *x &= *y;
  set_flags_byte(before, *x, *y);
}

void sub_byte(uint8_t *x, uint8_t *y) {
  uint8_t before = *x;
  *x -= *y;
  set_flags_byte(before, *x, *y);
}

void xor_byte(uint8_t *x, uint8_t *y) {
  uint8_t before = *x;
  *x ^= *y;
  set_flags_byte(before, *x, *y);
}

void cmp_byte(uint8_t *x, uint8_t *y) {
  uint8_t before = *x;
  uint8_t after = *x - *y;
  set_flags_byte(before, after, *y);
}


void add_wide(uint16_t *x, uint16_t *y) { 
  uint16_t before = *x;
  *x += *y; 
  set_flags_wide(before, *x, *y);
}

void or_wide(uint16_t *x, uint16_t *y) {
  uint16_t before = *x;
  *x |= *y;
  set_flags_wide(before, *x, *y);
}

void adc_wide(uint16_t *x, uint16_t *y) { 
  uint16_t before = *x;
  *x += *y; 
  if (flags & CARRY_FLAG) *x += 1; 
  set_flags_wide(before, *x, *y);
}

void sbb_wide(uint16_t *x, uint16_t *y) {
  uint16_t before = *x;
  *x -= *y;
  if (flags & CARRY_FLAG) *x -= 1;
  set_flags_wide(before, *x, *y);
}

void and_wide(uint16_t *x, uint16_t *y) {
  uint16_t before = *x;
  *x &= *y;
  set_flags_wide(before, *x, *y);
}

void sub_wide(uint16_t *x, uint16_t *y) {
  uint16_t before = *x;
  *x -= *y;
  set_flags_wide(before, *x, *y);
}

void xor_wide(uint16_t *x, uint16_t *y) {
  uint16_t before = *x;
  *x ^= *y;
  set_flags_wide(before, *x, *y);
}

void cmp_wide(uint16_t *x, uint16_t *y) {
  uint16_t before = *x;
  uint16_t after = *x - *y;
  set_flags_wide(before, after, *y);
}

void (*arithmetic_byte[8])(uint8_t *, uint8_t *) = {
  add_byte, or_byte, adc_byte, sbb_byte, and_byte, sub_byte, xor_byte, cmp_byte, 
};

void (*arithmetic_wide[8])(uint16_t *, uint16_t *) = {
  add_wide, or_wide, adc_wide, sbb_wide, and_wide, sub_wide, xor_wide, cmp_wide, 
};

void byte_reg_rm() {
  uint8_t tmp = c;
  c = memory[(seg[1] << 4) + ip++];
  uint8_t mod_index = (c & 0b11000000) >> 6;
  uint8_t rm_index = (c & 0b00000111);
  uint8_t *rm = mod_byte(mod_index, rm_index);
  uint8_t *r = &reg.byte[offset[(tmp & REG) >> 3]];

  if (tmp & 0b10) {
    (*arithmetic_byte[(tmp & OP) >> 3])(r, rm);
  } else {
    (*arithmetic_byte[(tmp & OP) >> 3])(rm, r);
  }
} // NEEDS TESTING

void wide_reg_rm() {
  uint8_t tmp = c;
  c = memory[(seg[1] << 4) + ip++];
  uint8_t mod_index = (c & 0b11000000) >> 6;
  uint8_t rm_index = (c & 0b00000111);
  uint16_t *rm = mod_wide(mod_index, rm_index);
  uint16_t *r = &reg.wide[(tmp & REG) >> 3];

  if (tmp & 0b10) {
    (*arithmetic_wide[(tmp & OP) >> 3])(r, rm);
  } else {
    (*arithmetic_wide[(tmp & OP) >> 3])(rm, r);
  }
} // NEEDS TESTING

void byte_rm_im() {
  uint8_t tmp = c;
  c = memory[(seg[1] << 4) + ip++];
  uint8_t mod_index = (c & 0b11000000) >> 6;
  uint8_t rm_index = (c & 0b00000111);
  uint8_t *rm = mod_byte(mod_index, rm_index);
  uint8_t im = memory[(seg[CS] << 4) + ip++];

  (*arithmetic_byte[(c & OP) >> 3])(rm, &im);
} // NEEDS TESTING

void wide_rm_im() {
  uint8_t tmp = c;
  c = memory[(seg[1] << 4) + ip++];
  uint8_t mod_index = (c & 0b11000000) >> 6;
  uint8_t rm_index = (c & 0b00000111);
  uint16_t *rm = mod_wide(mod_index, rm_index);
  uint16_t im = memory[(seg[CS] << 4) + ip++];

  if (tmp & S_BIT) { 
    if (im & (1<<7)) im |= 0xff00;
  } else im |= memory[(seg[CS] << 4) + ip++] << 8; 

  (*arithmetic_wide[(c & OP) >> 3])(rm, &im);
} // NEEDS TESTING 


void byte_acc_im() {
  uint8_t tmp = c;
  uint8_t *acc = &reg.byte[AL];
  uint8_t im = memory[(seg[CS] << 4) + ip++];

  (*arithmetic_byte[(tmp & OP) >> 3])(acc, &im);
} // NEEDS TESTING

void wide_acc_im() {
  uint8_t tmp = c;
  uint16_t *acc = &reg.wide[AX];
  uint16_t im = memory[(seg[CS] << 4) + ip++];
  im |= memory[(seg[CS] << 4) + ip++] << 8;

  (*arithmetic_wide[(tmp & OP) >> 3])(acc, &im);
} // NEEDS TESTING

/*''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''*
 * END OF ARITHMETIC 
 *''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''*/

/******************************************************************************
 * MOV
 ******************************************************************************/

void mov_byte_reg_rm() {
  uint8_t tmp = c;
  c = memory[(seg[1] << 4) + ip++];
  uint8_t mod_index = (c & 0b11000000) >> 6;
  uint8_t rm_index = (c & 0b00000111);
  uint8_t *rm = mod_byte(mod_index, rm_index);
  uint8_t *r = &reg.byte[offset[(tmp & REG) >> 3]];

  if (tmp == D_BIT) {
    *r = *rm;
  } else {
    *rm = *r;
  }
} // NEEDS TESTING

void mov_wide_reg_rm() {
  uint8_t tmp = c;
  c = memory[(seg[1] << 4) + ip++];
  uint8_t mod_index = (c & 0b11000000) >> 6;
  uint8_t rm_index = (c & 0b00000111);
  uint16_t *rm = mod_wide(mod_index, rm_index);
  uint16_t *r = &reg.wide[(tmp & REG) >> 3];

  if (tmp == D_BIT) {
    *r = *rm;
  } else {
    *rm = *r;
  }
} // NEEDS TESTING

void mov_byte_rm_im() {
  uint8_t tmp = c;
  c = memory[(seg[1] << 4) + ip++];
  uint8_t mod_index = (c & 0b11000000) >> 6;
  uint8_t rm_index = (c & 0b00000111);
  uint8_t *rm = mod_byte(mod_index, rm_index);
  uint8_t im = memory[(seg[CS] << 4) + ip++];

  *rm = im;
} // NEEDS TESTING

void mov_wide_rm_im() {
  uint8_t tmp = c;
  c = memory[(seg[1] << 4) + ip++];
  uint8_t mod_index = (c & 0b11000000) >> 6;
  uint8_t rm_index = (c & 0b00000111);
  uint16_t *rm = mod_wide(mod_index, rm_index);
  uint16_t im = memory[(seg[CS] << 4) + ip++];
  im |= memory[(seg[CS] << 4) + ip++];

  *rm = im;
} // NEEDS TESTING 

void mov_reg_im() {
  if (c & 0b1000) {
    uint16_t im = memory[(seg[CS] << 4) + ip++];
    im |= memory[(seg[CS] << 4) + ip++] << 8;
    reg.wide[c & 0b111] = im;
  } else {
    uint8_t im = memory[(seg[CS] << 4) + ip++];
    reg.byte[offset[c & 0b111]] = im;
  }
} // NEEDS TESTING

void mov_acc_mem() {
  uint16_t address = memory[(seg[CS] << 4) + ip++];
  address |= memory[(seg[CS] << 4) + ip++];
  if (c & D_BIT) {
    if (c & W_BIT) {
      reg.wide[AX] = *((uint16_t *)&memory[(seg[DS] << 4) + address]); // UNSURE 
    } else {
      reg.byte[AL] = memory[(seg[DS] << 4) + address];
    } 
  } else {
    if (c & W_BIT) {
      *((uint16_t *)&memory[(seg[DS] << 4) + address]) = reg.wide[AX]; // UNSURE 
    } else {
      memory[(seg[DS] << 4) + address] = reg.byte[AL];
    } 
  }
} // NEEDS TESTING

void mov_seg_rm() {
  c = memory[(seg[1] << 4) + ip++];
  uint8_t mod_index = (c & 0b11000000) >> 6;
  uint8_t rm_index = (c & 0b00000111);
  uint16_t *rm = mod_wide(mod_index, rm_index);
  uint16_t displacement;

  if (c & D_BIT) {
    seg[c & 0b00011000] = *rm;
  } else {
    *rm = seg[c & 0b00011000];
  }
} // NEEDS TESTING


/*''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''*
 * END OF MOV
 *''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''*/


/******************************************************************************
 * JUMPS
 ******************************************************************************/

void jo() {
  ip++;
  if (flags & OVERFLOW_FLAG) {
    ip += memory[(seg[CS] << 4) + ip] -1;
  } else {
    ip++;
  }
} // NEEDS TESTING

void jno() {
  ip++;
  if (!(flags & OVERFLOW_FLAG)) {
    ip += memory[(seg[CS] << 4) + ip] -1;
  } else {
    ip++;
  }
} // NEEDS TESTING

void jb() {
  ip++;
  if (flags & CARRY_FLAG) {
    ip += memory[(seg[CS] << 4) + ip] -1;
  } else {
    ip++;
  }
} // NEEDS TESTING

void jnb() {
  ip++;
  if (!(flags & CARRY_FLAG)) {
    ip += memory[(seg[CS] << 4) + ip] -1;
  } else {
    ip++;
  }
} // NEEDS TESTING

void je() {
  ip++;
  if (flags & ZERO_FLAG) {
    ip += memory[(seg[CS] << 4) + ip] -1;
  } else {
    ip++;
  }
} // NEEDS TESTING

void jne() {
  ip++;
  if (!(flags & ZERO_FLAG)) {
    ip += memory[(seg[CS] << 4) + ip] -1;
  } else {
    ip++;
  }
} // NEEDS TESTING

void jbe() {
  ip++;
  if ((flags & CARRY_FLAG) || (flags & ZERO_FLAG)) {
    ip += memory[(seg[CS] << 4) + ip] -1;
  } else {
    ip++;
  }
} // NEEDS TESTING

void jnbe() {
  ip++;
  if (!(flags & CARRY_FLAG) && !(flags & ZERO_FLAG)) {
    ip += memory[(seg[CS] << 4) + ip] -1;
  } else {
    ip++;
  }
} // NEEDS TESTING

void js() {
  ip++;
  if (flags & SIGN_FLAG) {
    ip += memory[(seg[CS] << 4) + ip] -1;
  } else {
    ip++;
  }
} // NEEDS TESTING

void jns() {
  ip++;
  if (!(flags & SIGN_FLAG)) {
    ip += memory[(seg[CS] << 4) + ip] -1;
  } else {
    ip++;
  }
} // NEEDS TESTING

void jp() {
  ip++;
  if (flags & PARITY_FLAG) {
    ip += memory[(seg[CS] << 4) + ip] -1;
  } else {
    ip++;
  }
} // NEEDS TESTING

void jnp() {
  ip++;
  if (!(flags & PARITY_FLAG)) {
    ip += memory[(seg[CS] << 4) + ip] -1;
  } else {
    ip++;
  }
} // NEEDS TESTING

void jl() {
  ip++;
  if (!(flags & PARITY_FLAG)) {
    ip += memory[(seg[CS] << 4) + ip] -1;
  } else {
    ip++;
  }
} // NEEDS TESTING

void jge() {
  ip++;
  if ((flags & OVERFLOW_FLAG) >> 4 == (flags & SIGN_FLAG)) {
    ip += memory[(seg[CS] << 4) + ip] -1;
  } else {
    ip++;
  }
} // NEEDS TESTING

void jle() {
  ip++;
  if ((flags & ZERO_FLAG) || ((flags & OVERFLOW_FLAG) >> 4 != (flags & SIGN_FLAG))) {
    ip += memory[(seg[CS] << 4) + ip] -1;
  } else {
    ip++;
  }
} // NEEDS TESTING

void jnle() {
  ip++;
  if (!(flags & ZERO_FLAG) && ((flags & OVERFLOW_FLAG) >> 4 == (flags & SIGN_FLAG))) {
    ip += memory[(seg[CS] << 4) + ip] -1;
  } else {
    ip++;
  }
} // NEEDS TESTING

/*''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''*
 * END OF JUMPS
 *''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''*/

/******************************************************************************
 * MISC
 ******************************************************************************/

void seg_override() {}
void decimal_adjust_for_add() {}
void decimal_adjust_for_subrtact() {}
void ascii_adjust_for_add() {}
void ascii_adjust_for_subtract() {}

/*''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''*
 * END OF MISC
 *''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''*/

/******************************************************************************
 * PUSH AND POP
 ******************************************************************************/
void push_rm() {
  c = memory[(seg[CS] << 4) + ip++];
  uint8_t mod_index = (c & MOD) >> 6;
  uint8_t rm_index = c & RM;
  uint16_t *rm = mod_wide(mod_index, rm_index);
  reg.wide[4]++;
  memory[reg.wide[4]] = *rm;
  ip++;
}

void push_reg() {
  reg.wide[4]++;
  memory[reg.wide[4]] = reg.wide[c & 0b111];
  ip++;
}

void push_seg() {
  reg.wide[4]++;
  memory[reg.wide[4]] = seg[(c & REG) >> 3];
  ip++;
} // NEEDS TESTING


void pop_rm() {
  c = memory[(seg[CS] << 4) + ip++];
  uint8_t mod_index = (c & MOD) >> 6;
  uint8_t rm_index = c & RM;
  uint16_t *rm = mod_wide(mod_index, rm_index);
  *rm = memory[reg.wide[4]];
  reg.wide[4]--;
  ip++;
}

void pop_reg() {
  reg.wide[c & 0b111] = memory[reg.wide[4]];
  reg.wide[4]--;
  ip++;
} // NEEDS TESTING

void pop_seg() {
  seg[(c & REG) >> 3] = memory[reg.wide[4]];
  reg.wide[4]--;
  ip++;
} // NEEDS TESTING

/*''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''*
 * END OF PUSH AND POP
 *''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''*/


void hlt() {
  exit(0);
}


/******************************************************************************
 * Jumptable
 ******************************************************************************/
void fail() {
  printf("failed at %x", memory[(seg[1] << 4) + ip]);
  exit(1);
}

void (*jumptable_mod[256])() = { 
  /* ARITHMETIC */
  byte_reg_rm, wide_reg_rm, byte_reg_rm, wide_reg_rm, byte_acc_im, wide_acc_im, push_seg, pop_seg,
  byte_reg_rm, wide_reg_rm, byte_reg_rm, wide_reg_rm, byte_acc_im, wide_acc_im, push_seg, pop_seg, 
  byte_reg_rm, wide_reg_rm, byte_reg_rm, wide_reg_rm, byte_acc_im, wide_acc_im, push_seg, pop_seg,  
  byte_reg_rm, wide_reg_rm, byte_reg_rm, wide_reg_rm, byte_acc_im, wide_acc_im, push_seg, pop_seg,   
  byte_reg_rm, wide_reg_rm, byte_reg_rm, wide_reg_rm, byte_acc_im, wide_acc_im, seg_override, decimal_adjust_for_add, 
  byte_reg_rm, wide_reg_rm, byte_reg_rm, wide_reg_rm, byte_acc_im, wide_acc_im, seg_override, decimal_adjust_for_subrtact,
  byte_reg_rm, wide_reg_rm, byte_reg_rm, wide_reg_rm, byte_acc_im, wide_acc_im, seg_override, ascii_adjust_for_add, 
  byte_reg_rm, wide_reg_rm, byte_reg_rm, wide_reg_rm, byte_acc_im, wide_acc_im, seg_override, ascii_adjust_for_subtract,

  fail, fail, fail, fail, fail, fail, fail, fail, 
  fail, fail, fail, fail, fail, fail, fail, fail, 
  push_reg, push_reg, push_reg, push_reg, push_reg, push_reg, push_reg, push_reg, 
  pop_reg, pop_reg, pop_reg, pop_reg, pop_reg, pop_reg, pop_reg, pop_reg, 
  fail, fail, fail, fail, fail, fail, fail, fail, 
  fail, fail, fail, fail, fail, fail, fail, fail, 
  jo, jno, jb, jnb, je, jne, jbe, jnbe, 
  js, jns, jp, jnp, jl, jge, jle, jnle, 

  byte_rm_im, wide_rm_im, byte_rm_im, wide_rm_im, fail, fail, fail, pop_rm, 
  mov_byte_reg_rm, mov_wide_reg_rm, mov_byte_reg_rm, mov_wide_reg_rm, mov_seg_rm, fail, mov_seg_rm, fail, 
  fail, fail, fail, fail, fail, fail, fail, fail, fail, fail, fail, fail, fail, fail, fail, fail, 
  mov_acc_mem, mov_acc_mem, mov_acc_mem, mov_acc_mem, fail, fail, fail, fail, fail, fail, fail, fail, fail, fail, fail, fail, 
 
  mov_reg_im, mov_reg_im, mov_reg_im, mov_reg_im, mov_reg_im, mov_reg_im, mov_reg_im, mov_reg_im,
  mov_reg_im, mov_reg_im, mov_reg_im, mov_reg_im, mov_reg_im, mov_reg_im, mov_reg_im, mov_reg_im,   

  fail, fail, fail, fail, fail, fail, mov_byte_rm_im, mov_wide_rm_im, fail, fail, fail, fail, fail, fail, fail, fail, 
  fail, fail, fail, fail, fail, fail, fail, fail, fail, fail, fail, fail, fail, fail, fail, fail, 
  fail, fail, fail, fail, fail, fail, fail, fail, fail, fail, fail, fail, fail, fail, fail, fail, 
  fail, fail, fail, fail, hlt, fail, fail, fail, fail, fail, fail, fail, fail, fail, fail, push_rm 
};
 // 176 - 183  or  0b10110xxx
 // 184 - 191  or  0b10111xxx


/******************************************************************************
 * Main loop
 ******************************************************************************/

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("Must provide a file to be interpreted as a 8086 binary to run\n");
    return 1;
  }

  FILE *fp = fopen(argv[1], "rb");
  if (fp == NULL) {
    printf("Couldn't open %s.", argv[1]);
    exit(1);
  }
  fseek(fp, 0L, SEEK_END); 
  long size = ftell(fp);  
  if (size >= 65536) {
    printf("File must be less than 64 Kb in size.\n");
    printf("Current file size is: %i\n", (int)ftell(fp));
    exit(1);
  }
  if (seg[CS] + size >= 1<<20) {
    printf("Not enough space in code segment.");
    printf("CS: %i", seg[CS]);
    exit(1);
  }
  rewind(fp);
  fread(memory + (seg[CS] << 4), size, 1, fp);
  fclose(fp); // close file before ending program.

  printf("al: %#04x\n", reg.byte[0]);
  printf("cl: %#04x\n", reg.byte[2]);
  printf("dl: %#04x\n", reg.byte[4]);
  printf("bl: %#04x\n", reg.byte[6]);
  printf("ah: %#04x\n", reg.byte[1]);
  printf("ch: %#04x\n", reg.byte[3]);
  printf("dh: %#04x\n", reg.byte[5]);
  printf("bh: %#04x\n", reg.byte[7]);
  printf("ax: %#06x\n", reg.wide[0]);
  printf("cx: %#06x\n", reg.wide[1]);
  printf("dx: %#06x\n", reg.wide[2]);
  printf("bx: %#06x\n", reg.wide[3]);
  printf("sp: %#06x\n", reg.wide[4]);
  printf("bp: %#06x\n", reg.wide[5]);
  printf("si: %#06x\n", reg.wide[6]);
  printf("di: %#06x\n", reg.wide[7]);
  printf("\n");
  printf("es: %#06x\n", seg[0]);
  printf("cs: %#06x\n", seg[1]);
  printf("ss: %#06x\n", seg[2]);
  printf("ds: %#06x\n", seg[3]);
  printf("\n");

  while ((c = memory[(seg[CS] << 4) + ip++]) != 0xF4) { // 0xF4 = hlt
    (*jumptable_mod[c])();
  }

  printf("al: %#04x\n", reg.byte[0]);
  printf("cl: %#04x\n", reg.byte[2]);
  printf("dl: %#04x\n", reg.byte[4]);
  printf("bl: %#04x\n", reg.byte[6]);
  printf("ah: %#04x\n", reg.byte[1]);
  printf("ch: %#04x\n", reg.byte[3]);
  printf("dh: %#04x\n", reg.byte[5]);
  printf("bh: %#04x\n", reg.byte[7]);
  printf("ax: %#06x\n", reg.wide[0]);
  printf("cx: %#06x\n", reg.wide[1]);
  printf("dx: %#06x\n", reg.wide[2]);
  printf("bx: %#06x\n", reg.wide[3]);
  printf("sp: %#06x\n", reg.wide[4]);
  printf("bp: %#06x\n", reg.wide[5]);
  printf("si: %#06x\n", reg.wide[6]);
  printf("di: %#06x\n", reg.wide[7]);
  printf("\n");
  printf("es: %#06x\n", seg[0]);
  printf("cs: %#06x\n", seg[1]);
  printf("ss: %#06x\n", seg[2]);
  printf("ds: %#06x\n", seg[3]);
  printf("\n");

  printf("flags: ");
  for (int i = 15; i >= 0; i--)
    printf("%d", (flags & (1 << i)) >> i );
  printf("\n");
  return 0;
}
