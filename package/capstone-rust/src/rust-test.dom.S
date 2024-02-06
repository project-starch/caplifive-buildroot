#ifndef __CAPSTONE_C_BUILTIN__
#define __CAPSTONE_C_BUILTIN__
#define cincoffset(rd, rs1, rs2) .insn r 0x5b, 0x1, 0xc, rd, rs1, rs2
#define cincoffsetimm(rd, rs, offset) .insn i 0x5b, 0x2, rd, offset(rs)
#define movc(rd, rs) .insn r 0x5b, 0x1, 0xa, rd, rs, x0
#define ldc(rd, rs, offset) .insn i 0x5b, 0x3, rd, offset(rs)
#define stc(rs1, rs2, offset) .insn s 0x5b, 0x4, rs1, offset(rs2)
#define shrinkto(rd, rs, size) .insn i 0x5b, 0x0, rd, size(rs)
#define mrev(rd, rs) .insn r 0x5b, 0x1, 0x8, rd, rs, x0
#define revoke(rs) .insn r 0x5b, 0x1, 0x0, x0, rs, x0
#define seal(rd, rs) .insn r 0x5b, 0x1, 0x7, rd, rs, x0
#define delin(rd) .insn r 0x5b, 0x1, 0x3, rd, x0, x0
#define tighten(rd, rs, imm) .insn r 0x5b, 0x1, 0x2, rd, rs, x##imm
#define domcall(rd, rs) .insn r 0x5b, 0x1, 0x20, rd, rs, x0
#define domreturn(rd, rs1, rs2) .insn r 0x5b, 0x1, 0x21, rd, rs1, rs2
#define ccsrrw(rd, ccsr, rs) .insn i 0x5b, 0x7, rd, ccsr(rs)
#define lcc(rd, rs, imm) .insn r 0x5b, 0x1, 0x4, rd, rs, x##imm
#define scc(rd, rs1, rs2) .insn r 0x5b, 0x1, 0x5, rd, rs1, rs2
#define split(rd, rs1, rs2) .insn r 0x5b, 0x1, 0x6, rd, rs1, rs2
#define ctvec 0x000
#define cih 0x001
#define cepc 0x002
#define cmmu 0x003
#define cscratch 0x004
#define cis 0x800
#define cid 0x801
#define cic 0x802
#define offsetmmu 0x803
#endif

__test_reentry:
  ccsrrw(sp, cscratch, x0)
  ldc(gp, sp, -16)
  ldc(t0, sp, -32)
  ccsrrw(x0, cscratch, t0)
  ld t0, -40(sp)
  csrrw x0, mcause, t0
  ld t0, -48(sp)
  csrrw x0, mtval, t0
  j test
.global _start
_start:
__test_entry:
  ccsrrw(sp, cscratch, x0)
  lcc(t1, sp, 4)
  scc(sp, sp, t1)
  delin(sp)
.global test
test:
  cincoffsetimm(sp, sp, -128)
  stc(ra, sp, 48)
  sd a0, 64(sp)
  stc(a1, sp, 80)
_test.0:
  li t0, 14342874
  .insn r 0x5b, 0x1, 0x43, x0, t0, x0
_test.1:
  li t1, 1
  mv t2, t1
  li s0, 1
  mv s1, s0
  li a0, 1
  mv a1, a0
  sd t2, 96(sp)
  sd s1, 104(sp)
  sd a1, 120(sp)
_test.2:
  ld t0, 120(sp)
  li t1, 10
  sltu t2, t0, t1
  bnez t2, _test.4
_test.3:
  j _test.6
_test.4:
  ld t0, 96(sp)
  ld t1, 104(sp)
  add t2, t0, t1
  mv s0, t2
  mv s1, t1
  mv a0, s0
  sd s1, 96(sp)
  sd a0, 104(sp)
  sd s0, 112(sp)
_test.5:
  ld t0, 120(sp)
  li t1, 1
  add t2, t0, t1
  mv s0, t2
  sd s0, 120(sp)
  j _test.2
_test.6:
  ld t0, 104(sp)
  ldc(t1, sp, 80)
  sd t0, 0(t1)
  lla t2, __test_reentry
  ldc(t0, sp, 48)
  lcc(t1, sp, 4)
  scc(sp, sp, t1)
  stc(gp, sp, -16)
  li ra, 0
  li gp, 0
  li tp, 0
  li t1, 0
  li s0, 0
  li s1, 0
  li a0, 0
  li a1, 0
  li a2, 0
  li a3, 0
  li a4, 0
  li a5, 0
  li a6, 0
  li a7, 0
  li s2, 0
  li s3, 0
  li s4, 0
  li s5, 0
  li s6, 0
  li s7, 0
  li s8, 0
  li s9, 0
  li s10, 0
  li s11, 0
  li t3, 0
  li t4, 0
  li t5, 0
  li t6, 0
  ccsrrw(t1, cscratch, x0)
  stc(t1, sp, -32)
  csrrw t1, mcause, x0
  sd t1, -40(sp)
  csrrw t1, mtval, x0
  sd t1, -48(sp)
  li t1, 0
  ccsrrw(x0, cscratch, sp)
  li sp, 0
  domreturn(t0, t2, x0)
_test.ret:
  j _test.ret

.align 16
.section .gct
