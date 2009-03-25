
#include <stdio.h>
#include "iferret_log.h"
#include "iferret_info_flow.h"
#include "taint.h"


extern unsigned long long ifregaddr[];

extern uint64_t safe_address_for_arbitrary_tainting;


extern uint64_t fake_base_addr_for_env;

uint32_t if_debug = 0;



char* info_flow_reg_str[16] = {
  "EAX",
  "ECX",
  "EDX",
  "EBX",
  "ESP",
  "EBP",
  "ESI",
  "EDI",
  "T0",
  "T1",
  "A0",
  "Q0",
  "Q1",
  "Q2",
  "Q3",
  "Q4"
};

// array of flags to indicate if any part of a register is tainted
uint8_t if_reg_taint[16] = {
  FALSE,
  FALSE,
  FALSE,
  FALSE,
  FALSE,
  FALSE,
  FALSE,
  FALSE,
  FALSE,
  FALSE,
  FALSE,
  FALSE,
  FALSE,
  FALSE,
  FALSE,
  FALSE
};



struct timeval last_time;

char *if_reg_str(int if_regnum) {
  return (info_flow_reg_str[if_regnum]);
}


void timer_reset (void) {
  gettimeofday(&last_time, NULL);
}


double timer_time () {
  struct timeval this_time;
  gettimeofday(&this_time, NULL);
  return (this_time.tv_sec - last_time.tv_sec
	  + (this_time.tv_usec - last_time.tv_usec)/1.0e6);
}


void if_debug_set(int l) {
  if_debug = l;
}


void if_debug_set_off() {
  if_debug = IF_DEBUG_OFF;
}


void if_debug_set_low() {
  if_debug = IF_DEBUG_LOW;
}


void if_debug_set_med() {
  if_debug = IF_DEBUG_MED;
}


void if_debug_set_high() {
  if_debug = IF_DEBUG_HIGH;
}


void if_debug_set_omg() {
  if_debug = IF_DEBUG_OMG;
}


uint8_t debug_off() {
  return (if_debug == IF_DEBUG_OFF);
}


uint8_t debug_low() {
  return (if_debug <= IF_DEBUG_LOW);
}


uint8_t debug_med() {
  return (if_debug <= IF_DEBUG_MED);
}


uint8_t debug_high() {
  return (if_debug <= IF_DEBUG_HIGH);
}


uint8_t debug_omg() {
  return (if_debug <= IF_DEBUG_OMG);
}


uint8_t debug_at_least_low() {
  return (if_debug >= IF_DEBUG_LOW);
}


uint8_t debug_at_least_med() {
  return (if_debug >= IF_DEBUG_MED);
}


uint8_t debug_at_least_high() {
  return (if_debug >= IF_DEBUG_HIGH);
}


uint8_t debug_at_least_omg() {
  return (if_debug >= IF_DEBUG_OMG);
}





// p is an address that came out of the log.  
// or it might be a fake address, like a register.
// decide which and print something approriate. 
void render_address(unsigned long long p) {
  if (p == IFRBA(IFRN_EAX)) 
    printf ("&eax");
  else if (p == IFRBA(IFRN_ECX)) 
    printf ("&ecx");
  else if (p == IFRBA(IFRN_EDX)) 
    printf ("&edx");
  else if (p == IFRBA(IFRN_EBX)) 
    printf ("&ebx");
  else if (p == IFRBA(IFRN_ESP)) 
    printf ("&esp");
  else if (p == IFRBA(IFRN_EBP))
    printf ("&ebp");
  else if (p == IFRBA(IFRN_ESI)) 
    printf ("&esi");
  else if (p == IFRBA(IFRN_EDI)) 
    printf ("&edi");
  else if (p == IFRBA(IFRN_T0)) 
    printf ("&t0");
  else if (p == IFRBA(IFRN_T1)) 
    printf ("&t1");
  else if (p == IFRBA(IFRN_A0)) 
    printf ("&a0");
  else if (p == IFRBA(IFRN_Q0)) 
    printf ("&q0");
  else if (p == IFRBA(IFRN_Q1)) 
    printf ("&q1");
  else if (p == IFRBA(IFRN_Q2)) 
    printf ("&q2");
  else if (p == IFRBA(IFRN_Q3)) 
    printf ("&q3");
  else if (p == IFRBA(IFRN_Q4)) 
    printf ("&q4");
  else 
    printf ("0x%Lux",p);
}


// returns TRUE iff address is *not* to a QEMU temporary register
uint8_t addr_is_real(unsigned long long p) {
  if ((p == IFRBA(IFRN_T0))
      || (p == IFRBA(IFRN_T1)) 
      || (p == IFRBA(IFRN_A0)) 
      || (p == IFRBA(IFRN_Q0)) 
      || (p == IFRBA(IFRN_Q1)) 
      || (p == IFRBA(IFRN_Q2)) 
      || (p == IFRBA(IFRN_Q3)) 
      || (p == IFRBA(IFRN_Q4)))
    return FALSE;
  else 
    return TRUE;
}


// mark this register as possibly tainted
inline void info_flow_mark_as_possibly_tainted (uint8_t if_regnum) {
  if_reg_taint[if_regnum] = TRUE;
}


// mark this register and not possibly tainted
inline void info_flow_mark_as_not_possibly_tainted (uint8_t if_regnum) {
  if_reg_taint[if_regnum] = FALSE;
}


// should return TRUE if this reg might be tainted. 
//inline uint8_t info_flow_possibly_tainted (uint8_t if_regnum) {
//  return (if_reg_taint[if_regnum] == TRUE);
//}

inline uint8_t info_flow_possibly_tainted (uint8_t if_regnum) {
  return (TRUE);
}




// should return TRUE only if this register cannot be tainted. 
inline uint8_t info_flow_not_possibly_tainted (uint8_t if_regnum) {
  return (if_reg_taint[if_regnum] == FALSE);
}



inline uint64_t ctull(unsigned long long p) {
  return (uint64_t) p;
}


inline uint64_t wctull(unsigned long long p) {
  uint64_t lp;
  //  char key[9];

  lp = ctull(p);

  /*
  memset (key,0,16);
  key[0] = (lp & 0xFF00000000000000) >> 56;
  key[1] = (lp & 0x00FF000000000000) >> 48;
  key[2] = (lp & 0x0000FF0000000000) >> 40;
  key[3] = (lp & 0x000000FF00000000) >> 32;
  key[4] = (lp & 0x00000000FF000000) >> 24;
  key[5] = (lp & 0x0000000000FF0000) >> 16;
  key[6] = (lp & 0x000000000000FF00) >> 8;
  key[7] = (lp & 0x00000000000000FF);
  *((uint64_t *) key) = lp;  
  
  if (tmh == NULL) 
    tmh = vslht_new();
  vslht_add(tmh, key, 1);
  */

  return (lp);
}


// delete info-flow for (p,p+n-1)
inline void info_flow_delete(unsigned long long p, size_t n) {
  delete_taint(wctull(p),n,"NONE",-1);
}


// copy of (p2,p2+n-1) to (p1,p1+n-1)
inline void info_flow_copy(unsigned long long p1,unsigned long long p2, size_t n) {
  transfer_taint(wctull(p1),n,wctull(p2),n,1,1,"NONE","NONE",-1);
}


// a non-obliting compute transfer.
// from (p2,p2+n2-1) to (p1,p1+n1-1)
inline void info_flow_compute(unsigned long long p1, size_t n1, unsigned long long p2, size_t n2) {
  transfer_taint(wctull(p1),n1,wctull(p2),n2,0,0,"NONE","NONE",-1);
}


inline void info_flow_label(unsigned long long p, size_t n, char *label) {
  label_taint(wctull(p),n,label,"NONE",-1);
}


// we want to *add* this label to the current set. 
inline void info_flow_add_label(unsigned long long p, size_t n, char *label) {
  // label an extent somewhere in never-never land.
  label_taint(safe_address_for_arbitrary_tainting, n, label, "NONE", -1);
  // execute a compute transfer from that to the extent we got as an arg.
  info_flow_compute(safe_address_for_arbitrary_tainting, n, p, n);
}


///////////////////////////////////////////////////////////


/* 
   p1 is presumed to be the address of some info-flow 
   register (fake address).
   load of bytes (p2,p2+n-1) from memory into bottom n bytes of 
   fake register. 
   if u is True then this is an unsigned load, so if
   n<4 we zero out higher-order bytes of the register at p1.  
   otherwise we sign-extend
*/
inline void info_flow_ld(unsigned long long p1, unsigned long long p2, size_t n, uint8_t u) {

  // NB: you are IGNORING u which is wrong!  
  //  if (if_debug == TRUE) 
  //    printf ("info_flow_ld %p %p %d %d\n", p1, p2, n, u);
  // um, we shouldn't have 64-bit lds yet...
  assert (n<=4);
  // copy the lower n bytes from p2 into reg at p1
  info_flow_copy(p1,p2,n);
  // delete info for upper bytes.
  if (n<4) {
    info_flow_delete(p1+n,4-n);
  }
}


// p1 is address of some info-flow register (fake address).
// store the low n bytes of that register at location p2.  
inline void info_flow_st(unsigned long long p1, unsigned long long p2, size_t n) {
  //  if (if_debug == TRUE) 
  //    printf ("info_flow_st %p %p %d\n", p1, p2, n);
  info_flow_ld(p2,p1,n,TRUE);
}


/////////////////////////////////////////


// delete info-flow for n bytes of register number rn, starting at offset o.
// if we delete for all 4 byts, then we can set the big falg.
inline void if_delete_reg_aux (uint32_t rn, uint32_t o, uint32_t n) {
  // only bother if reg *may* be tainted 
  if (info_flow_possibly_tainted(rn)) {		
    if (debug_at_least_med()) 
      printf ("%s possibly tainted\n", info_flow_reg_str[rn]);
    // untaint 
    info_flow_delete(IFRBA(rn)+o,n);		 
    if (o==0 && n==4) {
      // if we untainted the whole thing.  take note. 
      info_flow_mark_as_not_possibly_tainted(rn); 
    }
  }   
  else {
    if (debug_at_least_med()) {
      printf ("%s NOT possibly tainted\n", info_flow_reg_str[rn]);
    }
  }
  if (debug_at_least_med()) {
    check_reg_taint(rn,__FILE__,__LINE__);
  }
}


// most common case.  delete info-flow for all of 4-byte register.
inline void if_delete_r4(uint32_t rn) {
  if_delete_reg_aux(rn,0,4);
}


// just delete bottom 2 bytes 
inline void if_delete_r2(uint32_t rn) {
  if_delete_reg_aux(rn,0,2);
}


// just delete bottom byte
inline void if_delete_r1(uint32_t rn) {
  if_delete_reg_aux(rn,0,1);
}


/*
  Update info-flow graph to represent that 
  n bytes of register number rn1 starting from offset o1,
  is a copy of the n bytes of register number rn2 starting from offset o2.
  note: if rn2 is NOT tainted, we just untaint the 
  n bytes of rn1 starting with offset o1
*/
// rn1 is dest
// rn2 is source
inline void if_copy_regs_aux (uint32_t rn1, uint32_t o1, uint32_t rn2, uint32_t o2, uint32_t n) {
  //  assert (rn1 != UNINITIALIZED && rn2 != UNINITIALIZED);	
  if (debug_at_least_med()) {
    check_reg_taint(rn1,__FILE__,__LINE__);
    check_reg_taint(rn2,__FILE__,__LINE__);
    printf ("  if_copy_regs_aux (r1=%s o1=%d) (r2=%s o2=%d) n=%d\n",
	    info_flow_reg_str[rn1], o1, 
	    info_flow_reg_str[rn2], o2, n);
  }
  if (info_flow_possibly_tainted(rn2)) {  
    if (debug_at_least_med()) {
      printf ("%s possibly tainted\n", info_flow_reg_str[rn2]);
    }
    info_flow_copy(IFRBA(rn1)+o1,IFRBA(rn2)+o2,n);   
    info_flow_mark_as_possibly_tainted(rn1); 
  } 
  else { 
    if (debug_at_least_med()) {
      printf ("%s NOT possibly tainted\n", info_flow_reg_str[rn2]);
    }
    if (o1==0 && o2==0 && n==4 && info_flow_possibly_tainted(rn1)) { 
      // untainted rn1 copied to possibly tainted rn2.  
      // Thus, rn2 not possibly tainted  
      info_flow_delete(IFRBA(rn1)+o1,n);	
      info_flow_mark_as_not_possibly_tainted(rn1);	    
    } 
  }   
  if (debug_at_least_med()) {
    check_reg_taint(rn1,__FILE__,__LINE__);
    check_reg_taint(rn2,__FILE__,__LINE__);
  }
}


// most common case.  copy 4-byte reg to another 4-byte reg.
inline void if_copy_r4(uint32_t rn1, uint32_t rn2) {
  if_copy_regs_aux(rn1,0,rn2,0,4);
}


// copy 2-byte reg to another 2-byte.  leave higher-order bytes alone
// NB: x86 is little-endian.
// So, the 1st byte is least significant, 2nd is next-least significant. 
inline void if_copy_r2(uint32_t rn1, uint32_t rn2) {
  if_copy_regs_aux(rn1,0,rn2,0,2);
}


// copy 1-byte reg to another 1-byte. leave higher-order bytes alone
inline void if_copy_r1(uint32_t rn1, uint32_t rn2) {
  if_copy_regs_aux(rn1,0,rn2,0,1);
}


/*
  Update info-flow graph to represent that
  n1 bytes of rn1 starting from offset o1 are
  computed from n2 bytes of rn2 starting from 
  offset o2.
  NOTE: a "non-oblitting compute transfer"
*/
// rn1 is dest
// rn2 is src
inline void if_compute_regs_aux (uint32_t rn1, uint32_t o1, uint32_t n1, 
				 uint32_t rn2, uint32_t o2, uint32_t n2) {
  //  assert (rn1 != UNINITIALIZED && rn2 != UNINITIALIZED);	
  if (debug_at_least_med()) {
    check_reg_taint(rn1,__FILE__,__LINE__);
    check_reg_taint(rn2,__FILE__,__LINE__);
  }
  if (debug_at_least_med()) {
    printf ("  if_compute_regs_aux (r1=%s o1=%d n1=%d) (r2=%s o2=%d n2=%d)\n",
	    info_flow_reg_str[rn1], o1, n1,
	    info_flow_reg_str[rn2], o2, n2);
  }
  if (info_flow_possibly_tainted(rn2)) { 
    if (debug_at_least_med()) {
      printf ("%s possibly tainted\n", info_flow_reg_str[rn2]);
    }
    //  printf("IF_COMPUTE_REGS_AUX(%d,%d,%d,%d,%d,%d)\n", rn1,o1,n1,rn2,o2,n2); 
    info_flow_compute(IFRBA(rn1)+o1,n1,IFRBA(rn2)+o2,n2); 
    info_flow_mark_as_possibly_tainted(rn1); 
  } 
  else { 
    if (debug_at_least_med()) {
      printf ("%s NOT possibly tainted\n", info_flow_reg_str[rn2]);
    }
    /*
      // TRL 0805 whoops this is just bogus, right? 
      // This is a "non-oblitting" compute transfer.  
      // if rn2 isn't possibly tainted, it should have no effect upon rn1.
    if (info_flow_possibly_tainted(rn1)) { 
      info_flow_delete(IFRBA(rn1)+o1,n1);	
      info_flow_mark_as_not_possibly_tainted(rn1); 
    } 
    */
  }
  if (debug_at_least_med()) {
    check_reg_taint(rn1,__FILE__,__LINE__);
    check_reg_taint(rn2,__FILE__,__LINE__);
  }
}


inline void if_compute_r4(uint32_t rn1, uint32_t rn2) {
  if_compute_regs_aux(rn1,0,4,rn2,0,4);
}


inline void if_compute_r2(uint32_t rn1, uint32_t rn2) {
  if_compute_regs_aux(rn1,0,2,rn2,0,2);
}


inline void if_compute_r1(uint32_t rn1, uint32_t rn2) {
  if_compute_regs_aux(rn1,0,1,rn2,0,1);
}


/*
  This is for all of r1 += r2, r1 -= r2, etc.
  That is, r1 computed from itself and another register.
  We do this with a temporary as it streamlines the operations
  on the taint graph.
  n is to make the self-compute based on fewer than 4 bytes.  
*/
inline void if_self_compute_regs_aux(uint32_t rn1, uint32_t rn2, uint32_t n) {
  uint8_t r1pt, r2pt;

  //  assert (rn1 != UNINITIALIZED && rn2 != UNINITIALIZED);		
  if (debug_at_least_med()) {
    printf ("  if_self_compute_regs_aux r1=%s r2=%s n=%d\n",
	    info_flow_reg_str[rn1], info_flow_reg_str[rn2],n);
  }  
  r1pt = info_flow_possibly_tainted(rn1);
  r2pt = info_flow_possibly_tainted(rn2);  
  if (debug_at_least_med()) {
    if (r1pt == TRUE) 
      printf ("%s possibly tainted\n", info_flow_reg_str[rn1]);
    else
      printf ("%s NOT possibly tainted\n", info_flow_reg_str[rn1]);
    if (r2pt == TRUE) 
      printf ("%s possibly tainted\n", info_flow_reg_str[rn2]);
    else
      printf ("%s NOT possibly tainted\n", info_flow_reg_str[rn2]);
  }  
  if (r1pt || r2pt) {
    // we split up r1 += r2 into two parts
    // part 1: q0 = r1 + r2
    if_delete_reg_aux(IFRN_Q0,0,n); 
    if_compute_regs_aux(IFRN_Q0,0,n,rn1,0,n); 
    if_compute_regs_aux(IFRN_Q0,0,n,rn2,0,n); 
    // part 2: r1 = q0
    if_copy_regs_aux(rn1,0,IFRN_Q0,0,n); 
  } 
}


// common case -- r1+=r2 for all four bytes
inline void if_self_compute_r4(uint32_t rn1, uint32_t rn2) {
  if_self_compute_regs_aux(rn1,rn2,4);
}


// for bottom 2 bytes
inline void if_self_compute_r2(uint32_t rn1, uint32_t rn2) {
  if_self_compute_regs_aux(rn1,rn2,2);
}


// for bottom byte only
inline void if_self_compute_r1(uint32_t rn1, uint32_t rn2) {
  if_self_compute_regs_aux(rn1,rn2,1);
}


/*
  This is for r++, r-- etc.  
  Again, we make use of the temporary Q0.
  i.e. Q0 = R + 1;  R = Q0;
*/
inline void if_inc_r4(uint32_t rn) {
  if (debug_at_least_med()) {
    printf ("  if_inc_r4 r=%s\n",info_flow_reg_str[rn]);
  }
  if (info_flow_possibly_tainted(rn)) {		
    if (debug_at_least_med()) 
      printf ("%s possibly tainted\n", info_flow_reg_str[rn]);
    if_delete_r4(IFRN_Q0); 
    if_compute_r4(IFRN_Q0,rn); 
    if_copy_r4(rn,IFRN_Q0); 
  }
  else {
    if (debug_at_least_med())     
      printf ("%s NOT possibly tainted\n", info_flow_reg_str[rn]);
  }
}


/*
  load n bytes into register r from location p.
  u is a uint8_t: True means it's an unsigned ld, False requires sign-extension
  note, location, p, is really if_addr, which we've already extracted from log.
  msn is memory suffix number.  To my peril, currently ignoring it... 
  raincheck?
  0 _raw
  1 _kernel
  2 _user
*/
inline void if_ld(uint32_t msn, uint32_t rn, uint32_t n, uint32_t u, unsigned long long p) { 
  //  assert (msn != UNINITIALIZED); 
  //  assert (rn != UNINITIALIZED); 
  //  assert (p != (char *) UNINITIALIZED);	
  if (debug_at_least_med()) {
    check_reg_taint(rn,__FILE__,__LINE__);
  }
  if (debug_at_least_med()) {
    printf ("if_ld msn=%d rn=%d n=%d u=%d p=%p\n", msn,rn,n,u,p);
  }
  info_flow_ld( IFRBA(rn), p, n, u);	
  // assume rn might now be tainted.  
  info_flow_mark_as_possibly_tainted(rn);
  // check for tainted pointer.
  if (exists_taint(p,4,"NONE",-1)) {
    // tainted pointer.  Any labels on the 4 bytes of p need to get transferred as well.  
    info_flow_compute(p,4,IFRBA(rn),n);
  }
  if (debug_at_least_med()) {
    check_reg_taint(rn,__FILE__,__LINE__);
  }
}


inline void if_ldu(uint32_t msn, uint32_t rn, uint32_t n, unsigned long long p) {
  if_ld(msn,rn,n,TRUE,p);
}


inline void if_lds(uint32_t msn, uint32_t rn, uint32_t n, unsigned long long p) {
  if_ld(msn,rn,n,FALSE,p);
}


// store low n bytes of register r at location p. 
// msn is memory suffix number. 
inline void if_st(uint32_t msn, uint32_t rn, uint32_t n, unsigned long long p) {
  if (debug_at_least_med()) {
    check_reg_taint(rn,__FILE__,__LINE__);
  }
  if (info_flow_possibly_tainted(rn)) {		
    if (debug_at_least_med()) {
      printf ("if_st msn=%d r=%s n=%d p=%p\n", msn,info_flow_reg_str[rn],n,p);
      printf ("%s possibly tainted\n", info_flow_reg_str[rn]);
    }
    // note: no way to *mark* an address as possibly tainted.  
    info_flow_st(IFRBA(rn), p, n);	
  } 
  else {
    if (debug_at_least_med()) 
      printf ("%s NOT possibly tainted\n", info_flow_reg_str[rn]);
  }
  if (debug_at_least_med()) {
    check_reg_taint(rn,__FILE__,__LINE__);
  }
}











void iferret_info_flow_process_op(iferret_t *iferret,  iferret_op_t *op) {  
  iferret_op_arg_t *arg;
  uint8_t a0_8, a1_8, a2_8, a3_8, a4_8;
  uint16_t a0_16, a1_16, a2_16, a3_16, a4_16;
  uint32_t a0_32, a1_32, a2_32, a3_32, a4_32;
  uint64_t a0_64, a1_64, a2_64, a3_64, a4_64;
  assert (op->num < IFLO_SYS_CALLS_START);

  arg = op->arg;

  a0_8 = arg[0].val.u8;
  a1_8 = arg[1].val.u8;
  a2_8 = arg[2].val.u8;
  a3_8 = arg[3].val.u8;
  a4_8 = arg[4].val.u8;

  a0_16 = arg[0].val.u16;
  a1_16 = arg[1].val.u16;
  a2_16 = arg[2].val.u16;
  a3_16 = arg[3].val.u16;
  a4_16 = arg[4].val.u16;
  
  a0_32 = arg[0].val.u32;
  a1_32 = arg[1].val.u32;
  a2_32 = arg[2].val.u32;
  a3_32 = arg[3].val.u32;
  a4_32 = arg[4].val.u32;

  a0_64 = arg[0].val.u64;
  a1_64 = arg[1].val.u64;
  a2_64 = arg[2].val.u64;
  a3_64 = arg[3].val.u64;
  a4_64 = arg[4].val.u64;

  switch (op->num) {

    /* 
       Start of opreg_template.h
    */

    /* 
       This stuff (through IFLO_MOVH_REG_T1) all parallels
       that which is in opreg_template.h in target-i386.
       we keep REG opaque and code the register directly in the 
       info flow log element. Thus, we don't need to play same
       trick of including that file over and over.  
    */

    // iferret_log_info_flow_op_write_1(IFLO_OPREG_TEMPL_MOVL_A0_R,REGNUM);
    // A0 = (uint32_t)REG; 
  case IFLO_OPREG_TEMPL_MOVL_A0_R:  
    // dest,src
    if_copy_r4(IFRN_A0,a0_8);
    break;

    // iferret_log_info_flow_op_write_1(IFLO_OPREG_TEMPL_ADDL_A0_R,REGNUM);
    // A0 = (uint32_t)(A0 + REG);
  case IFLO_OPREG_TEMPL_ADDL_A0_R:
    // iferret_log_info_flow_op_write_1(IFLO_OPREG_TEMPL_ADDL_A0_R_S1,REGNUM);
    // A0 = (uint32_t)(A0 + (REG << 1)); 
  case IFLO_OPREG_TEMPL_ADDL_A0_R_S1:
    // iferret_log_info_flow_op_write_1(IFLO_OPREG_TEMPL_ADDL_A0_R_S2,REGNUM);
    // A0 = (uint32_t)(A0 + (REG << 2));
  case IFLO_OPREG_TEMPL_ADDL_A0_R_S2:
    // iferret_log_info_flow_op_write_1(IFLO_OPREG_TEMPL_ADDL_A0_R_S3,REGNUM);
    // A0 = (uint32_t)(A0 + (REG << 3));
  case IFLO_OPREG_TEMPL_ADDL_A0_R_S3:
    if_self_compute_r4(IFRN_A0,a0_8);
    break;

    /*
      NB: skipping bloody 64-bit for now
      void OPPROTO glue(op_movq_A0,REGNAME)(void)
      void OPPROTO glue(op_addq_A0,REGNAME)(void)
      void OPPROTO glue(glue(op_addq_A0,REGNAME),_s1)(void)
      void OPPROTO glue(glue(op_addq_A0,REGNAME),_s2)(void)
      void OPPROTO glue(glue(op_addq_A0,REGNAME),_s3)(void)
    */

    // iferret_log_info_flow_op_write_1(IFLO_OPREG_TEMPL_MOVL_T0_R,REGNUM);
    // T0 = REG;
  case IFLO_OPREG_TEMPL_MOVL_T0_R:
    if_copy_r4(IFRN_T0,a0_8);
    break;
    
    // iferret_log_info_flow_op_write_1(IFLO_OPREG_TEMPL_MOVL_T1_R,REGNUM);
    // T1 = REG;
  case IFLO_OPREG_TEMPL_MOVL_T1_R:
    if_copy_r4(IFRN_T1,a0_8);
    break;

    // iferret_log_info_flow_op_write_1(IFLO_OPREG_TEMPL_MOVH_T0_R,REGNUM);
    // T0 = REG >> 8;
  case IFLO_OPREG_TEMPL_MOVH_T0_R:
    if_delete_r4(IFRN_T0);
    if_compute_r4(IFRN_T0,a0_8);
    break;

    // iferret_log_info_flow_op_write_1(IFLO_OPREG_TEMPL_MOVH_T1_R,REGNUM);
    // T1 = REG >> 8;
  case IFLO_OPREG_TEMPL_MOVH_T1_R:
    if_delete_r4(IFRN_T1);
    if_compute_r4(IFRN_T1,a0_8);
    break;

    // iferret_log_info_flow_op_write_1(IFLO_OPREG_TEMPL_MOVL_R_T0,REGNUM);
    // REG = (uint32_t)T0;
  case IFLO_OPREG_TEMPL_MOVL_R_T0:
    if_copy_r4(a0_8,IFRN_T0);
    break;
    
    // iferret_log_info_flow_op_write_1(IFLO_OPREG_TEMPL_MOVL_R_T1,REGNUM);
    // REG = (uint32_t)T1;
  case IFLO_OPREG_TEMPL_MOVL_R_T1:
    if_copy_r4(a0_8,IFRN_T1);
    break;
    
    // iferret_log_info_flow_op_write_1(IFLO_OPREG_TEMPL_MOVL_R_A0,REGNUM);
    // REG = (uint32_t)A0;
  case IFLO_OPREG_TEMPL_MOVL_R_A0:
    if_copy_r4(a0_8,IFRN_A0);
    break;

    /*
      Skipping 64-bit again 
      void OPPROTO glue(glue(op_movq,REGNAME),_T0)(void)
      void OPPROTO glue(glue(op_movq,REGNAME),_T1)(void)
      void OPPROTO glue(glue(op_movq,REGNAME),_A0)(void)
    */

    //    iferret_log_info_flow_op_write_1(IFLO_OPREG_TEMPL_CMOVW_R_T1_T0,REGNUM);
    // mov T1 to REG.  (NB: we are inside the conditional.  So really this isa MOVW.)
    // REG = (REG & ~0xffff) | (T1 & 0xffff);
    // here, copy low 2 bytes from T1 to REG and leave top 24 bytes of REG alone.
  case IFLO_OPREG_TEMPL_CMOVW_R_T1_T0:
    if_copy_r2(a0_8,IFRN_T1);
    break;

    // iferret_log_info_flow_op_write_1(IFLO_OPREG_TEMPL_CMOVL_R_T1_T0,REGNUM);
    // mov T1 to REG if T0
    // REG = (uint32_t)T1;
    // here, different from previous.  
    // copy low 4 bytes from T1 to REG and zero anything in REG above those 4 bytes.  
  case IFLO_OPREG_TEMPL_CMOVL_R_T1_T0:
    if_copy_r4(a0_8,IFRN_T1);
    break; 

    /*
      Skipping 64 bit.
      void OPPROTO glue(glue(op_cmovq,REGNAME),_T1_T0)(void)
    */

    // iferret_log_info_flow_op_write_1(IFLO_OPREG_TEMPL_MOVW_R_T0,REGNUM);
    // REG = (REG & ~0xffff) | (T0 & 0xffff);
  case IFLO_OPREG_TEMPL_MOVW_R_T0:
    if_copy_r2(a0_8,IFRN_T0);
    break;
    
    // iferret_log_info_flow_op_write_1(IFLO_OPREG_TEMPL_MOVW_R_T1,REGNUM);
    // REG = (REG & ~0xffff) | (T1 & 0xffff);
  case IFLO_OPREG_TEMPL_MOVW_R_T1:
    if_copy_r2(a0_8,IFRN_T1);
    break;
    
    // iferret_log_info_flow_op_write_1(IFLO_OPREG_TEMPL_MOVW_R_A0,REGNUM);
    // REG = (REG & ~0xffff) | (A0 & 0xffff);
  case IFLO_OPREG_TEMPL_MOVW_R_A0:
    if_copy_r2(a0_8,IFRN_A0);
    break;  

    // iferret_log_info_flow_op_write_1(IFLO_OPREG_TEMPL_MOVB_R_T0,REGNUM);
    // REG = (REG & ~0xff) | (T0 & 0xff);
  case IFLO_OPREG_TEMPL_MOVB_R_T0:
    if_copy_r1(a0_8,IFRN_T0);
    break;
    
    // iferret_log_info_flow_op_write_1(IFLO_OPREG_TEMPL_MOVH_R_T0,REGNUM);
    // REG = (REG & ~0xff00) | ((T0 & 0xff) << 8);
  case IFLO_OPREG_TEMPL_MOVH_R_T0:
    if_copy_regs_aux(a0_8,1,IFRN_T0,1,1);
    break;

    // iferret_log_info_flow_op_write_1(IFLO_OPREG_TEMPL_MOVB_R_T1,REGNUM);
    // REG = (REG & ~0xff) | (T1 & 0xff);
  case IFLO_OPREG_TEMPL_MOVB_R_T1:
    if_copy_r1(a0_8,IFRN_T1);
    break;

    // iferret_log_info_flow_op_write_1(IFLO_OPREG_TEMPL_MOVH_R_T1,REGNUM);
    // REG = (REG & ~0xff00) | ((T1 & 0xff) << 8);
  case IFLO_OPREG_TEMPL_MOVH_R_T1:
    if_copy_regs_aux(a0_8,1,IFRN_T1,1,1);
    break;

    /*
      TRL 0802 Skipping 64-bit versions
      i.e. env->regs[9..15]
    */

    /* 
       End of opreg_template.h 
    */

    // T0 += T1;      
  case IFLO_ADDL_T0_T1:
    // T0 |= T1;
  case IFLO_ORL_T0_T1:
    // T0 &= T1;
  case IFLO_ANDL_T0_T1:
    // T0 -= T1;
  case IFLO_SUBL_T0_T1:
    // T0 ^= T1;
  case IFLO_XORL_T0_T1:
    if_self_compute_r4(IFRN_T0,IFRN_T1);
    break;

    // T0 = -T0;
  case IFLO_NEGL_T0:
    // T0++;
  case IFLO_INCL_T0:
    // T0--;
  case IFLO_DECL_T0:
    // T0 = ~T0;
  case IFLO_NOTL_T0:
    // T0 = bswap32(T0);
  case IFLO_BSWAPL_T0:
    // helper_bswapq_T0();
    if_inc_r4(IFRN_T0);
    break;

    /*
      TRL 0802 Not bothering with 64-bit.
      Thus, skipping op_bswapq_T0
    */

    // MULB:
    // res = (uint8_t)EAX * (uint8_t)T0;
    // EAX = (EAX & ~0xffff) | res;
    // IMULB:
    // res = (int8_t)EAX * (int8_t)T0;
    // EAX = (EAX & ~0xffff) | (res & 0xffff);
  case IFLO_MULB_AL_T0:
  case IFLO_IMULB_AL_T0:
    // q1(0..1) der (eax(0),t0(0))
    if_delete_r4(IFRN_Q1);
    if_compute_regs_aux(IFRN_Q1,0,2,IFRN_EAX,0,1);
    if_compute_regs_aux(IFRN_Q1,0,2,IFRN_T0,0,1);
    // eax(0..1) = q1(0..1)
    if_copy_r2(IFRN_EAX,IFRN_Q1);    
    break;

    // MULW:
    // res = (uint16_t)EAX * (uint16_t)T0;
    // EAX = (EAX & ~0xffff) | (res & 0xffff);
    // EDX = (EDX & ~0xffff) | ((res >> 16) & 0xffff);
  case IFLO_MULW_AX_T0:
  case IFLO_IMULW_AX_T0:
    // q1(0..3) der (eax(0..1),t0(0..1))
    if_delete_r4(IFRN_Q1);
    if_compute_regs_aux(IFRN_Q1,0,4,IFRN_EAX,0,2);
    if_compute_regs_aux(IFRN_Q1,0,4,IFRN_T0,0,2);
    // eax(0..1) = q1(0..1)
    if_copy_regs_aux(IFRN_EAX,0,IFRN_Q1,0,2);    
    // edx(0..1) = q1(2..3)
    if_copy_regs_aux(IFRN_EDX,0,IFRN_Q1,2,2);
    break;

    // MULL:
    // res = (uint64_t)((uint32_t)EAX) * (uint64_t)((uint32_t)T0);
    // EAX = (uint32_t)res;
    // EDX = (uint32_t)(res >> 32);
    // IMULL:
    // res = (int64_t)((int32_t)EAX) * (int64_t)((int32_t)T0);
    // EAX = (uint32_t)(res);
    // EDX = (uint32_t)(res >> 32);
  case IFLO_MULL_EAX_T0:
  case IFLO_IMULL_EAX_T0:
    // q1(0..3) der (eax(0..3),t0(0..3))
    if_delete_r4(IFRN_Q1);
    if_compute_r4(IFRN_Q1,IFRN_EAX);
    if_compute_r4(IFRN_Q1,IFRN_T0);
    // q2(0..3) der (eax(0..3),t0(0..3))
    if_delete_r4(IFRN_Q2);
    if_compute_r4(IFRN_Q2,IFRN_EAX);
    if_compute_r4(IFRN_Q2,IFRN_T0);
    // eax(0..3) = q1(0..3)
    if_copy_r4(IFRN_EAX,IFRN_Q1);
    // edx(0..3) = q2(0..3)
    if_copy_r4(IFRN_EDX,IFRN_Q2);
    break;

    // res = (int16_t)T0 * (int16_t)T1;
    // T0 = res;
  case IFLO_IMULW_T0_T1:
    // q1(0..2) der (t0(0..1),t1(0..1))
    if_delete_r4(IFRN_Q1);
    if_compute_regs_aux(IFRN_Q1,0,4,IFRN_T0,0,2);
    if_compute_regs_aux(IFRN_Q1,0,4,IFRN_T1,0,2);
    // t0(0..3) = q1(0..3)
    if_copy_r4(IFRN_EAX,IFRN_Q1);    

    break;

    // res = (int64_t)((int32_t)T0) * (int64_t)((int32_t)T1);
    // T0 = res;
  case IFLO_IMULL_T0_T1:
    // okay, we're screwed here.  looks like T0 *has* to be 64-bit?
    if_delete_r4(IFRN_T0);
    break;

    /*
      TRL 0802 
      Ignoring 64-bit 
      op_mulq_EAX_T0
      op_imulq_EAX_T0
      op_imulq_T0_T1
    */

    /*
    num = (EAX & 0xffff);
    den = (T0 & 0xff);
    if (den == 0) {
        raise_exception(EXCP00_DIVZ);
    }
    q = (num / den);
    if (q > 0xff)
        raise_exception(EXCP00_DIVZ);
    q &= 0xff;
    r = (num % den) & 0xff;
    EAX = (EAX & ~0xffff) | (r << 8) | q;
    */

  case IFLO_DIVB_AL_T0:
  case IFLO_IDIVB_AL_T0:
    // q1(0..1) = eax(0..1)         num
    if_copy_r2(IFRN_Q1,IFRN_EAX);
    // q2(0) = t0(0)                den
    if_copy_r1(IFRN_Q2,IFRN_T0);
    // q(0) der (q1(0..1),q2(0))   q
    if_delete_r4(IFRN_Q3);
    if_compute_regs_aux(IFRN_Q3,0,1,IFRN_Q1,0,2);
    if_compute_regs_aux(IFRN_Q3,0,1,IFRN_Q2,0,1);
    // q4(0) der (q1(0..1),q2(0))   r
    if_delete_r4(IFRN_Q4);
    if_compute_regs_aux(IFRN_Q4,0,1,IFRN_Q1,0,2);
    if_compute_regs_aux(IFRN_Q4,0,1,IFRN_Q2,0,1);
    // eax(0) = q3(0)
    if_copy_r1(IFRN_EAX,IFRN_Q3);
    // eax(1) = q4(0)
    if_copy_regs_aux(IFRN_EAX,1,IFRN_Q4,0,1);
    break;

    /*
    num = (EAX & 0xffff) | ((EDX & 0xffff) << 16);
    den = (T0 & 0xffff);
    if (den == 0) {
        raise_exception(EXCP00_DIVZ);
    }
    q = (num / den);
    if (q > 0xffff)
        raise_exception(EXCP00_DIVZ);
    q &= 0xffff;
    r = (num % den) & 0xffff;
    EAX = (EAX & ~0xffff) | q;
    EDX = (EDX & ~0xffff) | r;
    */
  case IFLO_DIVW_AX_T0:
  case IFLO_IDIVW_AX_T0:
    // q1(0..1) = eax(0..1)               num
    if_copy_r2(IFRN_Q1,IFRN_EAX);         
    // q1(2..3) = edx(0..1)               
    if_copy_regs_aux(IFRN_Q1,2,IFRN_EDX,0,2);
    // q2(0..1) = t0(0..1)                den
    if_copy_r2(IFRN_Q2,IFRN_T0);
    // q3(0..1) der (q1(0..3),q2(0..1))   q
    if_delete_r4(IFRN_Q3);
    if_compute_regs_aux(IFRN_Q3,0,2,IFRN_Q1,0,4);
    if_compute_regs_aux(IFRN_Q3,0,2,IFRN_Q2,0,2);
    // q4(0..1) der (q1(0..3),q2(0..1))   r
    if_delete_r4(IFRN_Q4);
    if_compute_regs_aux(IFRN_Q4,0,2,IFRN_Q1,0,4);
    if_compute_regs_aux(IFRN_Q4,0,2,IFRN_Q2,0,2);
    // eax(0..1) = q3(0..1)
    if_copy_r2(IFRN_EAX,IFRN_Q3);
    // edx(0..1) = q4(0..1)
    if_copy_r2(IFRN_EAX,IFRN_Q4);
    break;

    /*
    num = ((uint32_t)EAX) | ((uint64_t)((uint32_t)EDX) << 32);
    den = T0;
    if (den == 0) {
        raise_exception(EXCP00_DIVZ);
    }
#ifdef BUGGY_GCC_DIV64
    r = div32(&q, num, den);
#else
    q = (num / den);
    r = (num % den);
#endif
    if (q > 0xffffffff)
        raise_exception(EXCP00_DIVZ);
    EAX = (uint32_t)q;
    EDX = (uint32_t)r;

     */
  case IFLO_DIVL_EAX_T0:
  case IFLO_IDIVL_EAX_T0:
    // Once again, we are in some trouble.
    // num & den & T0 need to be 64-bit...
    if_delete_r4(IFRN_EAX);
    if_delete_r4(IFRN_EDX);
    break;

    /*
      TRL 0802
      Ignoring 64 bit again.
      op_divq_EAX_T0
      op_idivq_EAX_T0
    */

    // T0 = (uint32_t)PARAM1;
  case IFLO_MOVL_T0_IMU:
    // T0 = (int32_t)PARAM1;
  case IFLO_MOVL_T0_IM:
    if_delete_r4(IFRN_T0);
    break;

    // T0 += PARAM1;
  case IFLO_ADDL_T0_IM:
    // T0 = T0 & 0xffff;
  case IFLO_ANDL_T0_FFFF:
    //  T0 = T0 & PARAM1;
  case IFLO_ANDL_T0_IM:
    if_inc_r4(IFRN_T0);
    break;

    // T0 = T1;
  case IFLO_MOVL_T0_T1:
    if_copy_r4(IFRN_T0,IFRN_T1);
    break;

    // T1 = (uint32_t)PARAM1;    
  case IFLO_MOVL_T1_IMU:
    // T1 = (int32_t)PARAM1;
  case IFLO_MOVL_T1_IM:
    if_delete_r4(IFRN_T1);
    break;

    // T1 += PARAM1;
  case IFLO_ADDL_T1_IM:
    if_inc_r4(IFRN_T1);
    break;

    // T1 = A0;
  case IFLO_MOVL_T1_A0:
    if_copy_r4(IFRN_T1,IFRN_A0);
    break; 

    // A0 = (uint32_t)PARAM1;
  case IFLO_MOVL_A0_IM:
    if_delete_r4(IFRN_A0);
    break;

    // A0 = (uint32_t)(A0 + PARAM1);
  case IFLO_ADDL_A0_IM:
    if_inc_r4(IFRN_A0);
    break;

    // A0 = (uint32_t)*(target_ulong *)((char *)env + PARAM1);
    // NB: if_addr is env+PARAM1
  case IFLO_MOVL_A0_SEG:    
    // memsuffix is 0 for _raw?  A flying leap
    if_ldu(0, IFRN_A0, 4, fake_base_addr_for_env + a0_64);
    break;
 
    // A0 = (uint32_t)(A0 + *(target_ulong *)((char *)env + PARAM1));
    // NB: if_addr is env+PARAM1
  case IFLO_ADDL_A0_SEG:
    // q1 = *(if_addr)
    if_ldu(0,IFRN_Q1,4,a0_64);
    // a0 += q1
    if_self_compute_r4(IFRN_A0, fake_base_addr_for_env + IFRN_Q1);
    break;

    // A0 = (uint32_t)(A0 + (EAX & 0xff));
  case IFLO_ADDL_A0_AL:
    if_self_compute_r4(IFRN_A0,IFRN_EAX);
    break;

    // A0 = A0 & 0xffff;
  case IFLO_ANDL_A0_FFFF:
    // x86 is little endian so bytes 2 and 3 are higher order ones. 
    if_delete_reg_aux(IFRN_A0,2,2);
    break;

    /*
      TRL0802 skipping 64-bit
      op_movq_T0_im64
      op_movq_T1_im64
      op_movq_A0_im
      op_movq_A0_im64
      op_addq_A0_im
      op_addq_A0_im64
      op_movq_A0_seg
      op_addq_A0_seg
      op_addq_A0_AL
    */

    /*
      Start handling includes of ops_mem.h in op.c.

      The following, up to IFLO_STL_MEMSUFFIX_T1,
      are analogous to ops_mem.h.  That file is included
      several times by op.c with different settings for MEMSUFFIX.
      Here, MEMSUFFIX is if_mem, which we squirreled away in the log
      entry and extracted above.  
    */
    
    //  iferret_log_info_flow_op_write_18(IFLO_OPS_MEM_LDUB_T0_A0,MEMSUFFIXNUM,phys_a0(A0));
    // T0 = *A0, just one byte. A0 is next element in log.
  case IFLO_OPS_MEM_LDUB_T0_A0:
    // first, a copy transfer from address to t0
    if_ldu(a0_8,IFRN_T0,1,a1_64);
    break;
    
    // iferret_log_info_flow_op_write_18(IFLO_OPS_MEM_LDSB_T0_A0,MEMSUFFIXNUM,phys_a0(A0));
    // ditto, but signed.
  case IFLO_OPS_MEM_LDSB_T0_A0:
    if_lds(a0_8,IFRN_T0,1,a1_64);
    break; 

    // iferret_log_info_flow_op_write_18(IFLO_OPS_MEM_LDUW_T0_A0,MEMSUFFIXNUM,phys_a0(A0));
    // T0 = *A0, 2 bytes
  case IFLO_OPS_MEM_LDUW_T0_A0:
    if_ldu(a0_8,IFRN_T0,2,a1_64);
    break;

    // iferret_log_info_flow_op_write_18(IFLO_OPS_MEM_LDSW_T0_A0,MEMSUFFIXNUM,phys_a0(A0));
    // ditto, but signed.
  case IFLO_OPS_MEM_LDSW_T0_A0:
    if_lds(a0_8,IFRN_T0,2,a1_64);
    break;

    // iferret_log_info_flow_op_write_18(IFLO_OPS_MEM_LDL_T0_A0,MEMSUFFIXNUM,phys_a0(A0));
    // T0 = *A0, 4 bytes
  case IFLO_OPS_MEM_LDL_T0_A0:
    if_ldu(a0_8,IFRN_T0,4,a1_64);
    break;

    // iferret_log_info_flow_op_write_18(IFLO_OPS_MEM_LDUB_T1_A0,MEMSUFFIXNUM,phys_a0(A0));
    // T1 = *A0, just one byte. A0 is next element in log.
  case IFLO_OPS_MEM_LDUB_T1_A0:
    if_ldu(a0_8,IFRN_T1,1,a1_64);
    break;

    // iferret_log_info_flow_op_write_18(IFLO_OPS_MEM_LDSB_T1_A0,MEMSUFFIXNUM,phys_a0(A0));  
    // ditto, but signed.
  case IFLO_OPS_MEM_LDSB_T1_A0:
    if_lds(a0_8,IFRN_T1,1,a1_64);
    break; 
      
    // iferret_log_info_flow_op_write_18(IFLO_OPS_MEM_LDUW_T1_A0,MEMSUFFIXNUM,phys_a0(A0));
    // T1 = *A0, 2 bytes
  case IFLO_OPS_MEM_LDUW_T1_A0:
    if_ldu(a0_8,IFRN_T1,2,a1_64);
    break;

    // ditto, but signed.
  case IFLO_OPS_MEM_LDSW_T1_A0:
    if_lds(a0_8,IFRN_T1,2,a1_64);
    break;

    // T1 = *A0, 4 bytes
  case IFLO_OPS_MEM_LDL_T1_A0:
    if_ldu(a0_8,IFRN_T1,4,a1_64);
    break;

    // *A0 = T0, one byte
  case IFLO_OPS_MEM_STB_T0_A0:
    if_st(a0_8,IFRN_T0,1,a1_64);
    break;

    // two bytes.
  case IFLO_OPS_MEM_STW_T0_A0:
    if_st(a0_8,IFRN_T0,2,a1_64);
    break;

    // all four bytes
  case IFLO_OPS_MEM_STL_T0_A0:
    if_st(a0_8,IFRN_T0,4,a1_64);
    break;
    
  case IFLO_OPS_MEM_STW_T1_A0:
    if_st(a0_8,IFRN_T1,2,a1_64);
    break;

  case IFLO_OPS_MEM_STL_T1_A0:
    if_st(a0_8,IFRN_T1,4,a1_64);
    break;

    /*
      Done handling ops_mem.h stuff.
    */

    // raincheck
    // EIP = T0;
  case IFLO_JMP_T0:
    // check here that EIP isn't tainted?  
    if_copy_r4(IFRN_EIP,IFRN_T0);
    break;

    // EIP = (uint32_t)PARAM1;
  case IFLO_MOVL_EIP_IM:
    if_delete_r4(IFRN_EIP);
    break;

    /*
      Skipping 64-bit
      op_movq_eip_im
      op_movq_eip_im64
    */

    /*
      No info-flow for
      op_hlt;
      op_monitor;
      op_mwait;
      op_debug;
      op_raise_interrupt;
      op_raise_exception;
      op_into;
      op_cli;
      op_sti;
      op_set_inhibit_irq;
      op_reset_inhibit_irq;
      op_rsm;
      op_boundw;  (Really?)
      op_boundl;  (Really?)

    */

    /*
    eflags = cc_table[CC_OP].compute_all();
    d = ldq(A0);
    if (d == (((uint64_t)EDX << 32) | EAX)) {
        // part_1 corresponds to this branch
        stq(A0, ((uint64_t)ECX << 32) | EBX);
        eflags |= CC_Z;
	...
    } else 
    */

HERE I AM!

  case IFLO_CMPXCHG8B_PART1:
    //if_st(0,IFRN_EAX,4,if_addr);
    //if_st(0,IFRN_ECX,4,if_addr+4);
    break;

    /*
    eflags = cc_table[CC_OP].compute_all();
    d = ldq(A0);
    if (d == (((uint64_t)EDX << 32) | EAX)) {
      ...
    } else {
      // part_2 corresponds to this branch
        EDX = d >> 32;
        EAX = d;
        eflags &= ~CC_Z;
    }
    */
  case IFLO_CMPXCHG8B_PART2:
    //if_ldu(0,IFRN_EAX,4,if_addr);
    // little endian -- higher order word comes 2nd
    //if_ldu(0,IFRN_EDX,4,if_addr+4);
    break;

    // T0 = 0;
  case IFLO_MOVL_T0_0:
    if_delete_r4(IFRN_T0);
    break;

    /*
      Start handling includes of ops_template.h into op.c

      Time for all those multiple size ops 
      i.e. the 4 different includes of ops_template.h in op.c
      for SHIFT = 0,1,2,3
      which turns into q,l,w,b in ops_template.h
    */

    /*
      These are all about computing cc (flags).
      Taking a rain check for now. 
      compute_all_add
      compute_c_add
      compute_all_adc
      compute_c_adc
      compute_all_sub
      compute_c_sub
      compute_all_sbb
      compute_c_sbb
      compute_all_logic
      compute_all_inc
      compute_c_inc
      compute_all_dec
      compute_all_shl
      compute_c_shl
      compute_c_sar
      compute_all_sar
      compute_c_mul
      compute_all_mul
      
      These are jumps.  Also taking a raincheck.
      op_jb_sub
      op_jz_sub
      op_jnz_sub
      op_jbe_sub
      op_js_sub
      op_jl_sub
      op_jle_sub
      
      Loops get a raincheck, too.
      op_loopnz
      op_loopz
      op_jz_ecx
      op_jnz_ecx
    */

    // raincheck
    // these are all about setting T0 to true/false
    // based upon the relationships between condition codes.
    // T0 = ((DATA_TYPE)src1 < (DATA_TYPE)src2);
  case IFLO_OPS_TEMPLATE_SETB_T0_SUB:
    // T0 = ((DATA_TYPE)CC_DST == 0);
  case IFLO_OPS_TEMPLATE_SETZ_T0_SUB:
    // T0 = ((DATA_TYPE)src1 <= (DATA_TYPE)src2);
  case IFLO_OPS_TEMPLATE_SETBE_T0_SUB:
    // T0 = lshift(CC_DST, -(DATA_BITS - 1)) & 1;
  case IFLO_OPS_TEMPLATE_SETS_T0_SUB:
    // T0 = ((DATA_STYPE)src1 < (DATA_STYPE)src2);
  case IFLO_OPS_TEMPLATE_SETL_T0_SUB:
    // T0 = ((DATA_STYPE)src1 <= (DATA_STYPE)src2);
  case IFLO_OPS_TEMPLATE_SETLE_T0_SUB:
    if_delete_r4(IFRN_T0);
    break;

    // count = T1 & SHIFT1_MASK;
    // T0 = T0 << count;
  case IFLO_OPS_TEMPLATE_SHL_T0_T1:
    // count = T1 & SHIFT1_MASK;
    // T0 &= DATA_MASK;
    // T0 = T0 >> count;
  case IFLO_OPS_TEMPLATE_SHR_T0_T1:
    // count = T1 & SHIFT1_MASK;
    // src = (DATA_STYPE)T0;
    // T0 = src >> count;    
  case IFLO_OPS_TEMPLATE_SAR_T0_T1:
    // wrong.  prolly should use SHIFT to limit spread
    if_self_compute_r4(IFRN_T0,IFRN_T1);
    break;

    /*
      Start handling includes of ops_template_mem.h into ops_template.h, 
      and thus into op.c
      MEM_WRITE is the parameterization for this include, taking on values 0,1,2 
      and undefined.
      Note that this means each of these needs to be parameterized by *both*
      if_shift and if_mem_write
    */

    // blimey. raincheck.  These fiddle with value for T0 and then (if mem_write == 1)
    // store that value at A0.  Ugh. 
  case IFLO_OPS_TEMPLATE_ROL_T0_T1_CC_MEMWRITE:
  case IFLO_OPS_TEMPLATE_ROR_T0_T1_CC_MEMWRITE:
  case IFLO_OPS_TEMPLATE_ROL_T0_T1_MEMWRITE:
  case IFLO_OPS_TEMPLATE_ROR_T0_T1_MEMWRITE:
  case IFLO_OPS_TEMPLATE_RCL_T0_T1_CC_MEMWRITE:
  case IFLO_OPS_TEMPLATE_RCR_T0_T1_CC_MEMWRITE:
  case IFLO_OPS_TEMPLATE_SHL_T0_T1_CC_MEMWRITE:
  case IFLO_OPS_TEMPLATE_SHR_T0_T1_CC_MEMWRITE:
  case IFLO_OPS_TEMPLATE_SAR_T0_T1_CC_MEMWRITE:
    //    if_delete_r4(IFRN_T0);
    if (arg[0].val.u8 == 1) {
      if_st(a0_64,T0_BASE);
    break;

  case IFLO_OPS_TEMPLATE_SHLD_T0_T1_IM_CC_MEMWRITE:
  case IFLO_OPS_TEMPLATE_SHLD_T0_T1_ECX_CC_MEMWRITE:
  case IFLO_OPS_TEMPLATE_SHRD_T0_T1_IM_CC_MEMWRITE:
  case IFLO_OPS_TEMPLATE_SHRD_T0_T1_ECX_CC_MEMWRITE:
    {
      uint8_t okay = FALSE;
      if (if_shift == 1) { // DATA_BITS == 16
	if_delete_r4(IFRN_T0);
	okay = TRUE;
      }
      else {
	if (if_shift >= 2) { // DATA_BITS >= 32
	  if_delete_r4(IFRN_T0);
	  if_delete_r4(IFRN_T1);
	  okay = TRUE;
	}
      }
      if (okay == FALSE) {
	printf ("DATA_BITS < 16 ?\n");
	assert (1==0);
      }    
      break;
    }

    // raincheck. 
  case IFLO_OPS_TEMPLATE_ADC_T0_T1_CC_MEMWRITE:
  case IFLO_OPS_TEMPLATE_SBB_T0_T1_CC_MEMWRITE:
  case IFLO_OPS_TEMPLATE_CMPXCHG_T0_T1_EAX_CC_MEMWRITE:
    if_delete_r4(IFRN_T0);
    break;
    
    /*
      Done handling includes of ops_template_mem.h into ops_template.h
    */

    /*
      first, all of these have
      (use Q1 for count)
      Q1 = T1 & SHIFT_MAX
      T1 = T0 >> Q1;
      
      next, they differentiate 
      BTS: T0 |= (((target_long)1) << Q1);
      BTR: T0 &= ~(((target_long)1) << Q1);
      BTC: T0 ^= (((target_long)1) << Q1);
     */
  case IFLO_SHIFT_BTS_T0_T1_CC:
  case IFLO_SHIFT_BTR_T0_T1_CC:
  case IFLO_SHIFT_BTC_T0_T1_CC:
    // Q1 = T1 & SHIFT_MASK
    if_delete_r4(IFRN_Q1);
    if_compute_r4(IFRN_Q1,IFRN_T1);
    // T1 = T0 >> Q1;
    if_delete_r4(IFRN_T1);
    if_compute_r4(IFRN_T1,IFRN_T0);
    if_compute_r4(IFRN_T1,IFRN_Q1);
    // T0 computed from itself and Q1.
    if_self_compute_r4(IFRN_T0,IFRN_Q1);
    break;

    // A0 += ((DATA_STYPE)T1 >> (3 + SHIFT)) << SHIFT;
  case IFLO_SHIFT_ADD_BIT_A0_T1:
    if_self_compute_r4(IFRN_A0,IFRN_T1);
    break;

    // weird shit.
  case IFLO_SHIFT_BSF_T0_CC:
  case IFLO_SHIFT_BSR_T0_CC:
    if_delete_r4(IFRN_T1);
    if_compute_r4(IFRN_T1,IFRN_T0);
    break;

    // raincheck. 
    // DF is "direction flag" 
    // T0 = DF << SHIFT;    
  case IFLO_SHIFT_MOVL_T0_DSHIFT:
    if_delete_r4(IFRN_T0);
    break;
    
  case IFLO_SHIFT_IN_T0_T1:
    // port i/o 
    // I think this is handled elsewhere...
    //    if_delete_r4(IFRN_T1);
    break;

  case IFLO_SHIFT_IN_DX_T0:
    // again, port i/o
    //    if_delete_r4(IFRN_T0);
    break;

    /*
      Done handling includes of ops_template.h into op.c
    */

    // T0 = (int8_t)T0;
  case IFLO_MOVSBL_T0_T0:
    // sign extension?
    if_inc_r4(IFRN_T0);
    break;

    // T0 = (uint8_t)T0;
  case IFLO_MOVZBL_T0_T0:
    // clear top 3 bytes
    if_delete_reg_aux(IFRN_T0,1,3);
    break;

    // T0 = (int16_t)T0;
  case IFLO_MOVSWL_T0_T0:
    if_inc_r4(IFRN_T0);
    break;

    // T0 = (uint16_t)T0;    
  case IFLO_MOVZWL_T0_T0:
    if_delete_reg_aux(IFRN_T0,2,2);
    break;

    // EAX = (uint32_t)((int16_t)EAX);
  case IFLO_MOVSWL_EAX_AX:
    // sign extend and then mask?
    if_inc_r4(IFRN_EAX);
    break;

    /*
      Skipping 64-bit stuff.
      op_movslq_T0_T0
      op_moslq_RAX_EAX
    */

    // wtf, mate?
    // EAX = (EAX & ~0xffff) | ((int8_t)EAX & 0xffff);
  case IFLO_MOVSBW_AX_AL:
    if_inc_r4(IFRN_EAX);
    break;

    // wtf isnt this a 64-bit op? 
    // EDX = (uint32_t)((int32_t)EAX >> 31);
  case IFLO_MOVSLQ_EDX_EAX:
    if_self_compute_r4(IFRN_EDX,IFRN_EAX);
    break;

    // EDX = (EDX & ~0xffff) | (((int16_t)EAX >> 15) & 0xffff);
  case IFLO_MOVSWL_DX_AX:
    if_self_compute_r4(IFRN_EDX,IFRN_EAX);
    break;    

    /*
      Skipping 64-bit stuff.
      op_movsqo_RDX_RAX
    */

    // ESI = (uint32_t)(ESI + T0);
  case IFLO_ADDL_ESI_T0:
    if_self_compute_r4(IFRN_ESI,IFRN_T0);
    break;

    // ESI = (ESI & ~0xffff) | ((ESI + T0) & 0xffff);
  case IFLO_ADDW_ESI_T0:
    if_self_compute_r2(IFRN_ESI,IFRN_T0);
    break;

    // EDI = (uint32_t)(EDI + T0);
  case IFLO_ADDL_EDI_T0:
    if_self_compute_r4(IFRN_EDI,IFRN_T0);
    break;

    // EDI = (EDI & ~0xffff) | ((EDI + T0) & 0xffff);
  case IFLO_ADDW_EDI_T0:
    if_self_compute_r2(IFRN_EDI,IFRN_T0);
    break;

    // ECX = (uint32_t)(ECX - 1);
  case IFLO_DECL_ECX:
    if_inc_r4(IFRN_ECX);
    break;

    // ECX = (ECX & ~0xffff) | ((ECX - 1) & 0xffff);
  case IFLO_DECW_ECX:
    if_inc_r4(IFRN_ECX);
    break;

    /*
      Skipping 64-bit stuff.
      op_addq_ESI_T0
      op_addq_EDI_T0
      op_decq_ECX
    */

    // A0 = (uint32_t)(A0 + env->segs[R_SS].base);
  case IFLO_ADDL_A0_SS:
    // A0 = (uint32_t)(A0 - 2);
  case IFLO_SUBL_A0_2:
    // A0 = (uint32_t)(A0 - 4);
  case IFLO_SUBL_A0_4:
    if_inc_r4(IFRN_A0);
    break;

    // ESP = (uint32_t)(ESP + 4)
  case IFLO_ADDL_ESP_4:
    // ESP = (uint32_t)(ESP + 2);
  case IFLO_ADDL_ESP_2:
    // ESP = (ESP & ~0xffff) | ((ESP + 4) & 0xffff);
  case IFLO_ADDW_ESP_4:
    // ESP = (ESP & ~0xffff) | ((ESP + 2) & 0xffff);
  case IFLO_ADDW_ESP_2:
    // ESP = (uint32_t)(ESP + PARAM1);
  case IFLO_ADDL_ESP_IM:
    // ESP = (ESP & ~0xffff) | ((ESP + PARAM1) & 0xffff);
  case IFLO_ADDW_ESP_IM:
    if_inc_r4(IFRN_ESP);
    break;

    /*
      Skipping 64-bit stuff.
      op_subq_A0_2
      op_subq_A0_8
      op_addq_ESP_8
      op_addq_ESP_im
    */

    // helper_rdtsc();
  case IFLO_RDTSC:
    if_delete_r4(IFRN_EAX);
    if_delete_r4(IFRN_EDX);
    break;

    // helper_cpuid();    
  case IFLO_CPUID:
    if_delete_r4(IFRN_EAX);
    if_delete_r4(IFRN_EBX);
    if_delete_r4(IFRN_ECX);
    if_delete_r4(IFRN_EDX);
    break;

    // rainchek    
    // helper_enter_level(PARAM1, PARAM2);
  case IFLO_ENTER_LEVEL:
    // helper_sysenter();
  case IFLO_SYSENTER:
    // helper_sysexit();
  case IFLO_SYSEXIT:
    break;

    /*
      Skipping 64-bit stuff.
      op_enter64_level;
      op_syscall;
      op_sysret;
    */

    // raincheck
    // helper_rdmsr();
  case IFLO_RDMSR:
    // helper_wrmsr();
  case IFLO_WRMSR:
    if_delete_r4(IFRN_EAX);
    if_delete_r4(IFRN_EDX);
    break;

    // raincheck
    // bcd shite.  please.
  case IFLO_AAM:
  case IFLO_AAD:
  case IFLO_AAA:
  case IFLO_AAS:
  case IFLO_DAA:
  case IFLO_DAS:
    if_delete_r4(IFRN_EAX);
    break;

    // raincheck
    // segment handling stuff
  case IFLO_MOVL_SEG_T0:
  case IFLO_MOVL_SEG_T0_VM:
  case IFLO_MOVL_T0_SEG:
    if_delete_r4(IFRN_T0);
    break;

    // rainchek
  case IFLO_LSL:
    // load segment limit
  case IFLO_LAR:
    // load access rights byte
    if_delete_r4(IFRN_T1);
    break;

    /*
      no info flow for?
      op_verr
      op_verw
    */

    // raincheck
  case IFLO_ARPL:
    if_delete_r4(IFRN_T0);
    if_delete_r4(IFRN_T1);
    break;

    // raincheck
  case IFLO_LJMP_PROTECTED_T0_T1:
    // protected mode jump
  case IFLO_LCALL_REAL_T0_T1:
    // real mode call
  case IFLO_LCALL_PROTECTED_T0_T1:
    // protected mode call
  case IFLO_IRET_REAL:
    // real & vm86 mode iret
  case IFLO_IRET_PROTECTED:
    // protected mode iret
  case IFLO_LRET_PROTECTED:
    // punt.
    break;

    // raincheck
  case IFLO_LLDT_T0:
    // load local descriptor table
  case IFLO_LTR_T0:
    // load task descriptor table
  case IFLO_MOVL_CRN_T0:
    // punting again. 
    break;

    // raincheck
  case IFLO_MOVTL_T0_CR8:
    if_delete_r4(IFRN_T0);
    break;

    // raincheck
  case IFLO_MOVL_DRN_T0:
    // punt 
    break;

    // raincheck
  case IFLO_LMSW_T0:
    if_delete_r4(IFRN_T0);
    break;

    // raincheck
  case IFLO_INVLPG_A0:
    // punt
    break;

    /*
      NB:using memsufix=_raw here?
    */

    // TRL 0806.  I *think* this all has fairly esoteric use.
    // these aren't run-of-the-mill ld/st.  Like ld/st global descriptor table addr. 
    // raincheck.
    
    // T0 = *(uint32_t *)((char *)env + PARAM1);
  case IFLO_MOVL_T0_ENV:
    // *(uint32_t *)((char *)env + PARAM1) = T0;
  case IFLO_MOVL_ENV_T0:
    // *(uint32_t *)((char *)env + PARAM1) = T1;
  case IFLO_MOVL_ENV_T1:
    // T0 = *(target_ulong *)((char *)env + PARAM1);
  case IFLO_MOVTL_T0_ENV:
    // *(target_ulong *)((char *)env + PARAM1) = T0;
  case IFLO_MOVTL_ENV_T0:
    // T1 = *(target_ulong *)((char *)env + PARAM1);
  case IFLO_MOVTL_T1_ENV:
    // *(target_ulong *)((char *)env + PARAM1) = T1;
  case IFLO_MOVTL_ENV_T1:
    break;

    // raincheck
  case IFLO_CLTS:
    break; 

    /* 
       No info flow for
       op_goto_tb0
       op_goto_tb1
       op_jmp_label
       op_jnz_T0_label  IMPLICIT INFO-FLOW???
       op_jz_T0_label   DITTO!
    */

    // raincheck
  case IFLO_SETO_T0_CC:    
  case IFLO_SETB_T0_CC:
  case IFLO_SETZ_T0_CC:
  case IFLO_SETBE_T0_CC:
  case IFLO_SETS_T0_CC:
  case IFLO_SETP_T0_CC:
  case IFLO_SETL_T0_CC:
  case IFLO_SETLE_T0_CC:
    if_delete_r4(IFRN_T0);
    break;
    
    // T0 ^= 1
  case IFLO_XOR_T0_1:
    if_inc_r4(IFRN_T0);
    break;

    /* 
       No info flow for
       op_set_cc_op
    */

  case IFLO_MOV_T0_CC:
    if_delete_r4(IFRN_T0);
    break;

    // raincheck
    // these are all about eflags.
    // no effect upon regular regs.  
    // variants of eflags = t0
  case IFLO_MOVL_EFLAGS_T0:
  case IFLO_MOVW_EFLAGS_T0:
  case IFLO_MOVL_EFLAGS_T0_IO:
  case IFLO_MOVW_EFLAGS_T0_IO:
  case IFLO_MOVL_EFLAGS_T0_CPL0:
  case IFLO_MOVW_EFLAGS_T0_CPL0:
  case IFLO_MOVB_EFLAGS_T0:
    // punt.
    break;

    // raincheck
    // t0 = eflags 
  case IFLO_MOVL_T0_EFLAGS:
    if_delete_r4(IFRN_T0);
    break;

    /*
      No info flow for the following
      op_cld
      op_std
      op_clc
      op_stc
      op_cmc
    */

  case IFLO_SALC:
    if_delete_r4(IFRN_EAX);
    break;

    /*
      Skipping boat load of FPU ops
    */

    // well, not quite. 
    // hmm this one diddles with EAX.  
  case IFLO_FNSTSW_EAX:
    if_delete_r4(IFRN_EAX);
    break;

    /*
      Skipping threading support, 
      op_lock
      op_unlock

      Skipping SSE support,
      op_movo
      op_movl
      etc etc
    */
    
  case IFLO_KEYBOARD_INPUT:
    {
      char alabel[1024];						        
      // construct a new keyboard label for each keycode.
            
     
      snprintf (alabel, 1024, "key-%s-%x-%x", 
		if_keyboard_label, if_key_num,if_key_val);
      //      push_key_label(alabel);
      //      if_key_num ++;      
      //      if (debug_at_least_low()) {
	printf ("IFLO_KEYBOARD_INPUT: if_p_orig=%p val=%x label=%s\n", 
		if_p_orig, if_key_val, alabel);
	//      }
      info_flow_label(T1_BASE, 1, alabel);
      info_flow_mark_as_possibly_tainted(IFRN_T1);      
      
      //      if (if_key_num == 3 && if_key_val == 0x2e) {
      //      	foo2 = TRUE;
//	if_debug_set_med();
      }
    }
    break;

  case IFLO_TLB_FILL:
	if(debug_at_least_low())
	printf("We hit a tlb fill!\r\n");
	break;
  case  IFLO_SAVE_ENV:
	if(debug_at_least_low())
         printf("SAVE_ENV called outside loop\r\n");
	 if_save_env_called = TRUE;
	 break;
   case IFLO_RESTORE_ENV:
	if(debug_at_least_low())
        printf("RESTORE_ENV called outside loop\r\n");
	if(!if_save_env_called_previous){
		if(debug_at_least_low())
		printf("YIPES, a restore with no save\r\n");
	}
	break;

  case IFLO_NEW_KEYBOARD_LABEL:
    {
      // no need to do anything; it already happened
      break;
    }
  
  case IFLO_NEW_NETWORK_LABEL:
    {
      // no need to do anything; it already happened
      break;
    }

 case IFLO_NETWORK_INPUT_BYTE_T0:
      if (debug_at_least_low()) {
	printf ("IFLO_NETWORK_INPUT_BYTE_T0: if_p_orig=%p val=%x",  
		if_p_orig, if_byte_val);
        }
	info_flow_label(T0_BASE, 1, if_network_label);
        info_flow_mark_as_possibly_tainted(IFRN_T0);
  	break;
  case IFLO_NETWORK_INPUT_WORD_T0:
      if (debug_at_least_low()) {
	printf ("IFLO_NETWORK_INPUT_WORD_T0: if_p_orig=%p val=%x",  
		if_p_orig, if_word_val);
        }
	info_flow_label(T0_BASE, 2, if_network_label);
        info_flow_mark_as_possibly_tainted(IFRN_T0);
  	break;
  case IFLO_NETWORK_INPUT_LONG_T0:
      if (debug_at_least_low()) {
	printf ("IFLO_NETWORK_INPUT_LONG_T0: if_p_orig=%p val=%x",  
		if_p_orig, if_long_val);
        }
	info_flow_label(T0_BASE, 4, if_network_label);
        info_flow_mark_as_possibly_tainted(IFRN_T0);
  	break;
  case IFLO_NETWORK_INPUT_BYTE_T1:
      if (debug_at_least_low()) {
	printf ("IFLO_NETWORK_INPUT_BYTE_T1: if_p_orig=%p val=%x",  
		if_p_orig, if_byte_val);
        }
	info_flow_label(T1_BASE, 1, if_network_label);
        info_flow_mark_as_possibly_tainted(IFRN_T1);
  	break;
  case IFLO_NETWORK_INPUT_WORD_T1:
      if (debug_at_least_low()) {
	printf ("IFLO_NETWORK_INPUT_WORD_T1: if_p_orig=%p val=%x",  
		if_p_orig, if_word_val);
        }
	info_flow_label(T1_BASE, 2, if_network_label);
        info_flow_mark_as_possibly_tainted(IFRN_T1);
  	break;
  case IFLO_NETWORK_INPUT_LONG_T1:
      if (debug_at_least_low()) {
	printf ("IFLO_NETWORK_INPUT_LONG_T1: if_p_orig=%p val=%x",  
		if_p_orig, if_long_val);
        }
	info_flow_label(T1_BASE, 4, if_network_label);
        info_flow_mark_as_possibly_tainted(IFRN_T1);
  	break;
 case IFLO_NETWORK_OUTPUT_BYTE_T0:
    if(exists_taint(T0_BASE,1,__FILE__,__LINE__))
	printf("DANGER! Exfiltrating taint!\r\n");
    break;
 case IFLO_NETWORK_OUTPUT_WORD_T0:
    if(exists_taint(T0_BASE,2,__FILE__,__LINE__))
	printf("DANGER! Exfiltrating taint!\r\n");
    break;
 case IFLO_NETWORK_OUTPUT_LONG_T0:
    if(exists_taint(T0_BASE,4,__FILE__,__LINE__))
	printf("DANGER! Exfiltrating taint!\r\n");
    break;
 case IFLO_NETWORK_OUTPUT_BYTE_T1:
    if(exists_taint(T1_BASE,1,__FILE__,__LINE__))
	printf("DANGER! Exfiltrating taint!\r\n");
    break;
 case IFLO_NETWORK_OUTPUT_WORD_T1:
    if(exists_taint(T1_BASE,2,__FILE__,__LINE__))
	printf("DANGER! Exfiltrating taint!\r\n");
    break;
 case IFLO_NETWORK_OUTPUT_LONG_T1:
    if(exists_taint(T1_BASE,4,__FILE__,__LINE__))
	printf("DANGER! Exfiltrating taint!\r\n");
    break;
  case IFLO_HD_TRANSFER:
      if (debug_at_least_low()) {
	printf ("IFLO_HD_TRANSFER: if_p_orig=%p to=%lld from=%lld size=%d\r\n",  
		if_p_orig, if_to_val, if_from_val,if_size_val);
        }
        if(exists_taint(if_from_val,if_size_val,__FILE__,__LINE__))
		printf("WOO HOO, we're tainted!!!\r\n");
    	info_flow_copy(if_to_val, if_from_val, if_size_val);
   	break;
  case IFLO_HD_TRANSFER_PART1:
      if (debug_at_least_low()) {
	printf ("IFLO_HD_TRANSFER_PART1: if_from_val=%lld\r\n",if_from_val);
        }
   	break;
  case IFLO_HD_TRANSFER_PART2:
      if (debug_at_least_low()) {
	printf ("IFLO_HD_TRANSFER_PART2: if_p_orig=%p to=%lld from=%lld size=%d\r\n",  
		if_p_orig, if_to_val, if_from_val,if_size_val);
        }
        if(exists_taint(if_from_val,if_size_val,__FILE__,__LINE__))
		printf("WOO HOO, we're tainted!!!\r\n");
    	info_flow_copy(if_to_val, if_from_val, if_size_val);
   	break;
    case IFLO_X86_INSN:
	printf("Yar, we hit the mark #%d...\r\n",if_long_val); 
	spit_taint();
	break;
    case IFLO_CMPXCHG_T0_T1_EAX_CC_CASE1:
//	printf("we saw CMPXCHG_T0_T1_EAX_CASE1\r\n");
//	info_flow_copy(T0_BASE, T1_BASE, 4);
	break;
    case IFLO_CMPXCHG_T0_T1_EAX_CC_CASE2:
//	printf("we saw CMPXCHG_T0_T1_EAX_CASE2\r\n");
//	info_flow_copy(if_addr, T0_BASE, 4);
	break;
    case IFLO_CMPXCHG_T0_T1_EAX_CC_CASE3:
//	printf("we saw CMPXCHG_T0_T1_EAX_CASE3\r\n");
//	info_flow_compute(IFRBA(IFRN_EAX), 4, T0_BASE, 4);
	break;

    case IFLO_SAVE_REG:
	printf("We're saving regnum#%d to addr 0x%08x\r\n",a0_32,if_addr);
        break;
  }
  return (if_p); 
}
