#include "libctru/types.h"

typedef u32(*backdoor_fn)(u32 arg0, u32 arg1);
u32 svc_7b(backdoor_fn entry_fn, ...);
u32 get_thread_page(void);
void k_enable_all_svcs(u32 isNew3DS);
u32 read_kaddr(u32 kaddr);
void write_kaddr(u32 kaddr, u32 val);
void do_memchunkhax1(void);
void unlock_services(bool isNew3DS);