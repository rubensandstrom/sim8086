/******************************************************************************
 * Notes and thoughts.
 ******************************************************************************/
/*
 * We want a second jumptable, the first one checks for order and size i.e. d 
 * and w if they exist. The second one checks for operation to be performed.
 *
 * es = 0b00 ca = 0b01 ss= 0b10 ds =0b11
*/
/******************************************************************************
 * Library includes
 ******************************************************************************/

#include <stdint.h> // for uint8_t and uint16_t
#include <stdio.h> // for open and close
#include <stdlib.h> // For malloc and free

/******************************************************************************
 * Bit patterns
 ******************************************************************************/

#define CARRY_FLAG          1
#define PARITY_FLAG         1<<2
#define AUXILIARY_FLAG      1<<4
#define ZERO_FLAG           1<<6
#define SIGN_FLAG           1<<7
#define TRAP_FLAG           1<<8
#define INTERUPT_FLAG       1<<9
#define DIRECTIONAL_FLAG    1<<10
#define OVERFLOW_FLAG       1<<11

#define WIDE                1
#define DEST                1<<1
#define SIGN                1<<1

#define BYTE_HIGH           1<<7
#define WIDE_HIGH           1<<15

#define OP                  0b00111000
#define REG                 0b00111000
/******************************************************************************
 * GLOBAL VARIABLES 
 ******************************************************************************/
// MAIN MEMORY
uint8_t *memory;

// REGISTERS
typedef union {
  uint16_t wide[8];
  uint8_t byte[8]; // index needs to be formated with offset
} Register; 
const uint8_t offset[8] = {0, 2, 4, 6, 1, 3, 5, 7};

Register reg;
uint16_t seg[4];
uint16_t flags;
uint16_t ip;

FILE *fp;
uint8_t c;

/******************************************************************************
 *  Op-functions, target for jumptabele_byte/wide
 ******************************************************************************/
void nop_byte(uint8_t *, uint8_t *) {} // this is just for now
void mov_byte(uint8_t *x, uint8_t *y) { *x = *y; }
void add_byte(uint8_t *x, uint8_t *y) { *x += *y; }
void or_byte(uint8_t *x, uint8_t *y) {}
void adc_byte(uint8_t *x, uint8_t *y) { 
  *x += *y; 
  if (flags & CARRY_FLAG) *x += 1; 
}
void sbb_byte(uint8_t *x, uint8_t *y) {}
void and_byte(uint8_t *x, uint8_t *y) {}
void sub_byte(uint8_t *x, uint8_t *y) {}
void xor_byte(uint8_t *x, uint8_t *y) {}
void cmp_byte(uint8_t *x, uint8_t *y) {}

void nop_wide(uint16_t *, uint16_t *) {} // this is just for now
void mov_wide(uint16_t *x, uint16_t *y) { *x = *y; }
void add_wide(uint16_t *x, uint16_t *y) { *x += *y; }
void or_wide(uint16_t *x, uint16_t *y) {}
void adc_wide(uint16_t *x, uint16_t *y) { 
  *x += *y; 
  if (flags & CARRY_FLAG) *x += 1; 
}
void sbb_wide(uint16_t *x, uint16_t *y) {}
void and_wide(uint16_t *x, uint16_t *y) {}
void sub_wide(uint16_t *x, uint16_t *y) {}
void xor_wide(uint16_t *x, uint16_t *y) {}
void cmp_wide(uint16_t *x, uint16_t *y) {}

void (*arithmetic_byte[8])(uint8_t *, uint8_t *) = {
  add_byte, or_byte, adc_byte, sbb_byte, and_byte, sub_byte, xor_byte, cmp_byte, 
};

void (*arithmetic_wide[8])(uint16_t *, uint16_t *) = {
  add_wide, or_wide, adc_wide, sbb_wide, and_wide, sub_wide, xor_wide, cmp_wide, 
};

void (*jumptable_byte[64])(uint8_t * , uint8_t *) = { 
  add_byte, add_byte, nop_byte, nop_byte, adc_byte, adc_byte, nop_byte, nop_byte,
  nop_byte, nop_byte, nop_byte, nop_byte, nop_byte, nop_byte, nop_byte, nop_byte,
  nop_byte, nop_byte, nop_byte, nop_byte, nop_byte, nop_byte, nop_byte, nop_byte,
  nop_byte, nop_byte, nop_byte, nop_byte, nop_byte, nop_byte, nop_byte, nop_byte,
  add_byte, nop_byte, nop_byte, nop_byte, nop_byte, nop_byte, nop_byte, nop_byte,
  nop_byte, nop_byte, nop_byte, nop_byte, nop_byte, nop_byte, nop_byte, nop_byte,
  nop_byte, nop_byte, nop_byte, nop_byte, nop_byte, nop_byte, nop_byte, nop_byte,
  nop_byte, nop_byte, nop_byte, nop_byte, nop_byte, nop_byte, nop_byte, nop_byte,
};

void (*jumptable_wide[64])(uint16_t *, uint16_t *) = { 
  add_wide, add_wide, nop_wide, nop_wide, adc_wide, adc_wide, nop_wide, nop_wide,
  nop_wide, nop_wide, nop_wide, nop_wide, nop_wide, nop_wide, nop_wide, nop_wide,
  nop_wide, nop_wide, nop_wide, nop_wide, nop_wide, nop_wide, nop_wide, nop_wide,
  nop_wide, nop_wide, nop_wide, nop_wide, nop_wide, nop_wide, nop_wide, nop_wide,
  add_wide, nop_wide, mov_wide, mov_wide, nop_wide, nop_wide, nop_wide, nop_wide,
  nop_wide, nop_wide, nop_wide, nop_wide, nop_wide, nop_wide, nop_wide, nop_wide,
  nop_wide, nop_wide, nop_wide, nop_wide, nop_wide, nop_wide, nop_wide, nop_wide,
  nop_wide, nop_wide, nop_wide, nop_wide, nop_wide, nop_wide, nop_wide, nop_wide,
};

/******************************************************************************
 * Instructions
 ******************************************************************************/
/*''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''*
 * ARITHMETIC 
 *''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''*/

void byte_reg_rm() {
  uint8_t tmp = c;
  c = getc(fp);
  uint8_t mod_index = (c & 0b11000000) >> 6;
  uint8_t r_index = (c & 0b00111000) >> 3;
  uint8_t rm_index = (c & 0b00000111);
  uint8_t *r;
  uint8_t *rm;
  // Memory instructions not implemented
  switch (mod_index) {
    case 0b00:
      //TODO
      break;
    case 0b01:
      //TODO
      break;
    case 0b10:
      //TODO
      break;
    case 0b11:
      rm = &reg.byte[offset[rm_index]]; 
      r = &reg.byte[offset[r_index]];
      break;
  }
  if (tmp & DEST){
    (*arithmetic_byte[(tmp & OP) >> 3])(r, rm);
  } else {
    (*arithmetic_byte[(tmp & OP) >> 3])(rm, r);
  }
}

void wide_reg_rm() {
  uint8_t tmp = c;
  c = getc(fp);
  uint8_t mod_index = (c & 0b11000000) >> 6;
  uint8_t r_index = (c & 0b00111000) >> 3;
  uint8_t rm_index = (c & 0b00000111);
  uint16_t *r;
  uint16_t *rm;
  // Memory instructions not implemented
  switch (mod_index) {
    case 0b00:
      //TODO
      break;
    case 0b01:
      //TODO
      break;
    case 0b10:
      //TODO
      break;
    case 0b11:
      rm = &reg.wide[rm_index]; 
      r = &reg.wide[r_index];
      break;
  }
  if (tmp & DEST){
    (*arithmetic_wide[(tmp & OP) >> 3])(r, rm);
  } else {
    (*arithmetic_wide[(tmp & OP) >> 2])(rm, r);
  }
}

void byte_rm_im() {
  uint8_t tmp = c;
  c = getc(fp);
  uint8_t mod_index = (c & 0b11000000) >> 6;
  uint8_t rm_index = (c & 0b00000111);
  uint8_t *rm;
  uint8_t *im;

  // Memory instructions not implemented
  switch (mod_index) {
    case 0b00:
      //TODO
      break;
    case 0b01:
      //TODO
      break;
    case 0b10:
      //TODO
      break;
    case 0b11:
      rm = &reg.byte[offset[rm_index]];
      *im = getc(fp);
      break;
  }
  (*arithmetic_byte[(tmp & OP) >> 3])(rm, im);
}

void wide_rm_im() {
  uint8_t tmp = c;
  c = getc(fp);
  uint8_t mod_index = (c & 0b11000000) >> 6;
  uint8_t rm_index = (c & 0b00000111);
  uint16_t *rm;
  uint16_t *im;

  // Memory instructions not implemented
  switch (mod_index) {
    case 0b00:
      //TODO
      break;
    case 0b01:
      //TODO
      break;
    case 0b10:
      //TODO
      break;
    case 0b11:
      rm = &reg.wide[rm_index];
      break;
  }
  *im = getc(fp);
  if (tmp & SIGN) { 
    if (*im & BYTE_HIGH) *im |= 0xff00;
  } else *im |= getc(fp) << 8; 

  (*arithmetic_wide[(tmp & OP) >> 3])(rm, im);
}

void byte_acc_im() {
  uint8_t tmp = c;
  uint8_t *acc = &reg.byte[0];
  uint8_t *im;
  *im = getc(fp);
  (*arithmetic_byte[(tmp & OP) >> 3])(acc, im);
}

void wide_acc_im() {
  uint8_t tmp = c;
  uint16_t *acc = &reg.wide[0];
  uint16_t *im;
  *im = getc(fp);
  *im |= getc(fp)<<8;
  (*arithmetic_wide[(tmp & OP) >> 3])(acc, im);
}

/*''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''*
 * END OF ARITHMETIC 
 *''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''*/

void wide_seg_rm() {
  uint8_t tmp = c;
  c = getc(fp);
  uint8_t mod_index = (c & 0b11000000) >> 6;
  uint8_t sr_index = (c & 0b00011000) >> 3;
  uint8_t rm_index = (c & 0b00000111);
  uint16_t *sr;
  uint16_t *rm;

  // Memory instructions not implemented
  switch (mod_index) {
    case 0b00:
      //TODO
      break;
    case 0b01:
      //TODO
      break;
    case 0b10:
      //TODO11
      break;
    case 0b11:
      sr = &seg[sr_index];
      rm = &reg.wide[rm_index];
      break;
  }
  if (tmp & DEST) {
    (*jumptable_wide[tmp>>2])(sr, rm);
  } else {
    (*jumptable_wide[tmp>>2])(rm, sr);
  }
}

void mov_byte_reg_im() {
  reg.byte[offset[c & 0b00000111]] = getc(fp);
}

void mov_wide_reg_im() {
  reg.wide[c & 0b00000111] = getc(fp);
  reg.wide[c & 0b00000111] |= getc(fp) << 8;
}


void hlt() {
  exit(0);
}
void push_seg() {
  memory[reg.wide[4]] = seg[(c & REG) >> 3];
  reg.wide[4]++;

}

/******************************************************************************
 * Jumptable
 ******************************************************************************/
void nop() {}

void (*jumptable_mod[256])() = { 
  /* ARITHMETIC */
  byte_reg_rm, wide_reg_rm, byte_reg_rm, wide_reg_rm, byte_acc_im, wide_acc_im, push_seg, NULL,
  byte_reg_rm, wide_reg_rm, byte_reg_rm, wide_reg_rm, byte_acc_im, wide_acc_im, push_seg, NULL, 
  byte_reg_rm, wide_reg_rm, byte_reg_rm, wide_reg_rm, byte_acc_im, wide_acc_im, push_seg, NULL,  
  byte_reg_rm, wide_reg_rm, byte_reg_rm, wide_reg_rm, byte_acc_im, wide_acc_im, push_seg, NULL,   
  byte_reg_rm, wide_reg_rm, byte_reg_rm, wide_reg_rm, byte_acc_im, wide_acc_im, NULL, NULL, 
  byte_reg_rm, wide_reg_rm, byte_reg_rm, wide_reg_rm, byte_acc_im, wide_acc_im, NULL, NULL,
  byte_reg_rm, wide_reg_rm, byte_reg_rm, wide_reg_rm, byte_acc_im, wide_acc_im, NULL, NULL, 
  byte_reg_rm, wide_reg_rm, byte_reg_rm, wide_reg_rm, byte_acc_im, wide_acc_im, NULL, NULL,

  nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, 
  nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, 
  nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, 
  nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, 

  byte_rm_im, wide_rm_im, byte_rm_im, wide_rm_im, nop, nop, nop, nop, 
  byte_reg_rm, wide_reg_rm, byte_reg_rm, wide_reg_rm, wide_seg_rm, nop, wide_seg_rm, nop, 
  nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, 
  nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, 
 
  mov_byte_reg_im, mov_byte_reg_im, mov_byte_reg_im, mov_byte_reg_im, mov_byte_reg_im, mov_byte_reg_im, mov_byte_reg_im, mov_byte_reg_im,
  mov_wide_reg_im, mov_wide_reg_im, mov_wide_reg_im, mov_wide_reg_im, mov_wide_reg_im, mov_wide_reg_im, mov_wide_reg_im, mov_wide_reg_im,   

  nop, nop, nop, nop, nop, nop, byte_rm_im, wide_rm_im, nop, nop, nop, nop, nop, nop, nop, nop, 
  nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, 
  nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, 
  nop, nop, nop, nop, hlt, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, 
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

  fp = fopen(argv[1], "r");
  void *memory = malloc(32);//1048576
  if (memory == NULL) {
    printf("Could not allocate 1Mb of memory.");
    return 0;
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

  while ((c = getc(fp)) != 0xF4) {
    ip++;
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

  free(memory); // free memory before ending program.
  fclose(fp); // close file before ending program.
  return 0;
}
