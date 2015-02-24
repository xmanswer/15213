/* 
 * CS:APP Data Lab 
 * 
 * <Please put your name and userid here>
 * 
 * bits.c - Source file with your solutions to the Lab.
 *          This is the file you will hand in to your instructor.
 *
 * WARNING: Do not include the <stdio.h> header; it confuses the dlc
 * compiler. You can still use printf for debugging without including
 * <stdio.h>, although you might get a compiler warning. In general,
 * it's not good practice to ignore compiler warnings, but in this
 * case it's OK.  
 */

#if 0
/*
 * Instructions to Students:
 *
 * STEP 1: Read the following instructions carefully.
 */

You will provide your solution to the Data Lab by
editing the collection of functions in this source file.

INTEGER CODING RULES:
 
  Replace the "return" statement in each function with one
  or more lines of C code that implements the function. Your code 
  must conform to the following style:
 
  int Funct(arg1, arg2, ...) {
      /* brief description of how your implementation works */
      int var1 = Expr1;
      ...
      int varM = ExprM;

      varJ = ExprJ;
      ...
      varN = ExprN;
      return ExprR;
  }

  Each "Expr" is an expression using ONLY the following:
  1. Integer constants 0 through 255 (0xFF), inclusive. You are
      not allowed to use big constants such as 0xffffffff.
  2. Function arguments and local variables (no global variables).
  3. Unary integer operations ! ~
  4. Binary integer operations & ^ | + << >>
    
  Some of the problems restrict the set of allowed operators even further.
  Each "Expr" may consist of multiple operators. You are not restricted to
  one operator per line.

  You are expressly forbidden to:
  1. Use any control constructs such as if, do, while, for, switch, etc.
  2. Define or use any macros.
  3. Define any additional functions in this file.
  4. Call any functions.
  5. Use any other operations, such as &&, ||, -, or ?:
  6. Use any form of casting.
  7. Use any data type other than int.  This implies that you
     cannot use arrays, structs, or unions.

 
  You may assume that your machine:
  1. Uses 2s complement, 32-bit representations of integers.
  2. Performs right shifts arithmetically.
  3. Has unpredictable behavior when shifting an integer by more
     than the word size.

EXAMPLES OF ACCEPTABLE CODING STYLE:
  /*
   * pow2plus1 - returns 2^x + 1, where 0 <= x <= 31
   */
  int pow2plus1(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     return (1 << x) + 1;
  }

  /*
   * pow2plus4 - returns 2^x + 4, where 0 <= x <= 31
   */
  int pow2plus4(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     int result = (1 << x);
     result += 4;
     return result;
  }

FLOATING POINT CODING RULES

For the problems that require you to implent floating-point operations,
the coding rules are less strict.  You are allowed to use looping and
conditional control.  You are allowed to use both ints and unsigneds.
You can use arbitrary integer and unsigned constants.

You are expressly forbidden to:
  1. Define or use any macros.
  2. Define any additional functions in this file.
  3. Call any functions.
  4. Use any form of casting.
  5. Use any data type other than int or unsigned.  This means that you
     cannot use arrays, structs, or unions.
  6. Use any floating point data types, operations, or constants.


NOTES:
  1. Use the dlc (data lab checker) compiler (described in the handout) to 
     check the legality of your solutions.
  2. Each function has a maximum number of operators (! ~ & ^ | + << >>)
     that you are allowed to use for your implementation of the function. 
     The max operator count is checked by dlc. Note that '=' is not 
     counted; you may use as many of these as you want without penalty.
  3. Use the btest test harness to check your functions for correctness.
  4. Use the BDD checker to formally verify your functions
  5. The maximum number of ops for each function is given in the
     header comment for each function. If there are any inconsistencies 
     between the maximum ops in the writeup and in this file, consider
     this file the authoritative source.

/*
 * STEP 2: Modify the following functions according the coding rules.
 * 
 *   IMPORTANT. TO AVOID GRADING SURPRISES:
 *   1. Use the dlc compiler to check that your solutions conform
 *      to the coding rules.
 *   2. Use the BDD checker to formally verify that your solutions produce 
 *      the correct answers.
 */


#endif
/* 
 * bitAnd - x&y using only ~ and | 
 *   Example: bitAnd(6, 5) = 4
 *   Legal ops: ~ |
 *   Max ops: 8
 *   Rating: 1
 */
int bitAnd(int x, int y) { 
  /* exploit the and of two using only or and not */
  return ~(~x | ~y);
}

/* 
 * tmin - return minimum two's complement integer 
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 4
 *   Rating: 1
 */
int tmin(void) {
  /* the tmin is 0x80000000 */
  return 1 << 31;
}
/* 
 * negate - return -x 
 *   Example: negate(1) = -1.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 5
 *   Rating: 2
 */
int negate(int x) {
  /* reverse x and add 1 to it can negate x */
  return (~x + 1);
}
/* 
 * allEvenBits - return 1 if all even-numbered bits in word set to 1
 *   Examples allEvenBits(0xFFFFFFFE) = 0, allEvenBits(0x55555555) = 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 2
 */
int allEvenBits(int x) {
  /* create a mask 010101..010101 to mask out the odd bits and compare
   * with this mask
   */
  int mask;
  mask = 0x55 + (0x55 << 8);
  mask = mask + (mask << 16);
  return !((x & mask) ^ mask);
}
/*
 * bitCount - returns count of number of 1's in word
 *   Examples: bitCount(5) = 2, bitCount(7) = 3
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 40
 *   Rating: 4
 */
int bitCount(int x) {
  /* use hamming weight to sum the number of 1's in a 
   * divide and conquer manner, create a series of masks
   * first in order to sum the 1's in a group of 2, 4, 8,
   * 16, and eventually sum up the final results from 2 groups*/
  int mask1, mask2, mask3, mask4, mask5;
  /* mask1 = 0101...0101 */
  mask1 = 0x55 + (0x55 << 8);
  mask1 = mask1 + (mask1 << 16); 
  /* mask2 = 00110011...00110011*/
  mask2 = 0x33 + (0x33 << 8);
  mask2 = mask2 + (mask2 << 16);
  /* mask3 = 0x0f0f0f0f*/
  mask3 = 0x0f + (0x0f << 8);
  mask3 = mask3 + (mask3 << 16);
  /* mask4 = 0x00ff00ff*/
  mask4 = 0xff + (0xff << 16);
  mask5 = 0xff;
  /* summing 1's in a divide and conquer manner*/
  x = (x & mask1) + ((x >> 1) & mask1);
  x = (x & mask2) + ((x >> 2) & mask2);
  x = (x & mask3) + ((x >> 4) & mask3);
  x = (x & mask4) + ((x >> 8) & mask4);
  return (x & mask5) + ((x >> 16) & mask5);
}
/* 
 * logicalShift - shift x to the right by n, using a logical shift
 *   Can assume that 0 <= n <= 31
 *   Examples: logicalShift(0x87654321,4) = 0x08765432
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 20
 *   Rating: 3 
 */
int logicalShift(int x, int n) {
  /* mark the bits filled by right shift to 0 */
  int mask = ~(((1 << 31) >> n) << 1); 
  return (x >> n) & mask;
}
/* 
 * isNegative - return 1 if x < 0, return 0 otherwise 
 *   Example: isNegative(-1) = 1.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 6
 *   Rating: 2
 */
int isNegative(int x) {
  /* shift sign of x to the LSB and compare with 0 */
  return !(!((x >> 31) ^ 0));
}
/* 
 * isGreater - if x > y  then return 1, else return 0 
 *   Example: isGreater(4,5) = 0, isGreater(5,4) = 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 24
 *   Rating: 3
 */
int isGreater(int x, int y) {
  /* x is greater than y if 
   * (A) x is positive and y is negative
   * (B) x and y have the same sign but the rest of x is larger than y, 
   *     this can be determined from the sign of the substraction result of 
   *     x and y (with sign = 0)
   */
  int mask, sub, caseA, caseB;
  mask = ~(1 << 31); 
  sub = (x & mask) + (~(y & mask) + 1);
  caseB = (x >> 31) + (~(y >> 31) + 1);
  /*case1 for sign of x is + and y is - */
  caseA = !(caseB ^ 1);
  /*case0 for the same sign, but x is higher than y*/
  caseB = (!caseB) & (!((sub >> 31) ^ 0)) & (!(!sub));
  
  return caseA | caseB;
}
/*
 * isPower2 - returns 1 if x is a power of 2, and 0 otherwise
 *   Examples: isPower2(5) = 0, isPower2(8) = 1, isPower2(0) = 0
 *   Note that no negative number is a power of 2.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 20
 *   Rating: 4
 */
int isPower2(int x) {
  /* only 1 bit is allowed to be 1 in x, x & (x - 1) will be 0 if that
   * is the case, and x cannot be negative or 0
   */
  return !((x >> 31) | (x & (x + (~0))) | (!x));
}
/* 
 * fitsBits - return 1 if x can be represented as an 
 *  n-bit, two's complement integer.
 *   1 <= n <= 32
 *   Examples: fitsBits(5,3) = 0, fitsBits(-4,3) = 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 15
 *   Rating: 2
 */
int fitsBits(int x, int n) {
  /* x need to have n-1 significant bits, so remove the non-significant
   * (32 - n) bits of x and compare with the origincal x
   */
  int shift = 33 + (~n);
  int xNew = (x << shift) >> shift;
  return !(xNew ^ x);
}
/* 
 * conditional - same as x ? y : z 
 *   Example: conditional(2,4,5) = 4
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 16
 *   Rating: 3
 */
int conditional(int x, int y, int z) {
  /* compare x to 0, if x is 0, make x all 1, if not, make x all 0 */
  x = !(x ^ 0) + (~0);
  return (y & x) + (z & (~x));
}
/* 
 * greatestBitPos - return a mask that marks the position of the
 *               most significant 1 bit. If x == 0, return 0
 *   Example: greatestBitPos(96) = 0x40
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 70
 *   Rating: 4 
 */
int greatestBitPos(int x) {
  /* shift x and or with the previous x to mark all the bits after the
   * the greatest bit (including itself) to 1
   */
  int signMask = x & (1 << 31);
  x = (x >> 1) | x;
  x = (x >> 2) | x;
  x = (x >> 4) | x;
  x = (x >> 8) | x;
  x = (x >> 16) | x;
  /*add sign back to take care of the case of negative x*/
  return (x ^ (x >> 1)) + signMask;
}
/* 
 * float_i2f - Return bit-level equivalent of expression (float) x
 *   Result is returned as unsigned int, but
 *   it is to be interpreted as the bit-level representation of a
 *   single-precision floating point values.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned float_i2f(int x) {
  /* shift x to find the first bit of significant and count the correct value 
   * of exp, use GRS bits of significant to determine different rounding cases 
   * for fraction, assmble sign, exp and frac in the end
   */
  unsigned signMask, sign, exp, frac, guard, round, sticky, result;
  /*special case for x = 0 and 0x80000000 */
  signMask = 1 << 31;
  if (!x) return 0;
  if (x == signMask) return (0xcf << 24);
  /*mask out sign, get abs of x as initial frac*/
  sign = x & signMask;
  frac = sign ? (-x) : x;
  /*start with exp = 158, count down until see the first 1 (MSB of significant) */
  exp = 158;
  while ( !(frac & signMask)){
    frac <<= 1;
    exp --;
  }
  /*mask out GRS bits for rounding cases*/
  guard = frac & (0x80 << 1);
  round = frac & 0x80;
  sticky = frac & 0x7f;
  /*assemble exp and frac together*/
  exp = exp << 23;
  frac = (frac << 1) >> 9;
  result = exp + frac;
  /*rounding the results based on GRS bits */
  if ((round != 0) && ((sticky != 0) || ((guard != 0) && (!sticky)))) 
    result ++;
  return result | sign;
}
/* 
 * float_abs - Return bit-level equivalent of absolute value of f for
 *   floating point argument f.
 *   Both the argument and result are passed as unsigned int's, but
 *   they are to be interpreted as the bit-level representations of
 *   single-precision floating point values.
 *   When argument is NaN, return argument..
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 10
 *   rks@andrew.cmu.edu
 *   Rating: 2
 */
unsigned float_abs(unsigned uf) {
  /* determine whether input is NaN, if yes, return input, 
   * if not, return result with MSB = 0*/
  unsigned floatMin, mask;
  /* generate mask = 0x80000000, min NaN in float = 0x7f800000 */
  mask = 1 << 31;
  floatMin = 0xff << 23; 
  if (((uf > floatMin) && (uf < mask)) || (uf > (floatMin + mask))) {
    return uf;
  }
  else
    return (uf << 1) >> 1;
}
