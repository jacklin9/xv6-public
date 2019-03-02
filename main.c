#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

static void startothers(void);
static void mpmain(void)  __attribute__((noreturn));
extern pde_t *kpgdir;
extern char end[]; // first address after kernel loaded from ELF file

// Bootstrap processor starts running C code here.
// Allocate a real stack and switch to it, first
// doing some setup required for memory allocator to work.
int
main(void)
{
  kinit1(end, P2V(4*1024*1024)); // phys page allocator   /// Set mem in [end, KERNBASE + 4M] as free so that init 
                                                          /// code can use dynamic mem
  kvmalloc();      // kernel page table
  mpinit();        // detect other processors
  lapicinit();     // interrupt controller
  seginit();       // segment descriptors
  picinit();       // disable pic     /// XV6 can only run on SMP machine
  ioapicinit();    // another interrupt controller  /// Disable all interrupt
  consoleinit();   // console hardware
  uartinit();      // serial port
  pinit();         // process table
  tvinit();        // trap vectors
  binit();         // buffer cache
  fileinit();      // file table
  ideinit();       // disk 
  startothers();   // start other processors
  /// The reason why separating the mem init into 2 phases (kinit1 and kinit2) is that we cannot init the mem in one phase: it is a
  /// bootstrap process. First we need setup a simple dynamic mem allocator that can dynamically allocate mem in virtual address
  /// [_end, KERNBASE + 4M] so that other init process can use dynamic mem. Then we make the final mem init at proper time. Here 
  /// the proper time. Here the proper time must be after calling startothers(). The reason is when starting an AP, startothers()
  /// calls the dynamic allocator to allocate mem as the kernal stack of the AP, whose physical addr must be in [0, 4M] because
  /// the AP can only access physical mem in [0, 4M] as it uses entrypgdir to set up the initial mem map. If we called kinit2 before
  /// startothers, the dynamic allocator might allocate the kernal stack above physical address 4M, which was not accessible by the
  /// AP with the initial mem map, so the initialization will fail
  /// The design here can be improved: we may start merge the two kinit into one and implement a separate mem allocator which is 
  /// dedicately used by codes who need mem below 4M
  kinit2(P2V(4*1024*1024), P2V(PHYSTOP)); // must come after startothers()
  userinit();      // first user process
  mpmain();        // finish this processor's setup
}

// Other CPUs jump here from entryother.S.
static void
mpenter(void)
{
  switchkvm();
  seginit();
  lapicinit();
  mpmain();
}

// Common CPU setup code.
static void
mpmain(void)
{
  cprintf("cpu%d: starting %d\n", cpuid(), cpuid());
  idtinit();       // load idt register
  xchg(&(mycpu()->started), 1); // tell startothers() we're up
  scheduler();     // start running processes
  /// Actually initialization doesn't finish yet. There are some initializations
  /// calling functions that need process context (for example, sleep). These initializations
  /// are acted in forkret which is first called when proc 1 (init) is first scheduled to run.
  /// These initializations include iinit, initlog. See proc.c:432
}

pde_t entrypgdir[];  // For entry.S

// Start the non-boot (AP) processors.
static void
startothers(void)
{
  extern uchar _binary_entryother_start[], _binary_entryother_size[];
  uchar *code;
  struct cpu *c;
  char *stack;

  // Write entry code to unused memory at 0x7000.
  // The linker has placed the image of entryother.S in
  // _binary_entryother_start.
  code = P2V(0x7000);
  memmove(code, _binary_entryother_start, (uint)_binary_entryother_size);

  for(c = cpus; c < cpus+ncpu; c++){
    if(c == mycpu())  // We've started already.
      continue;

    // Tell entryother.S what stack to use, where to enter, and what
    // pgdir to use. We cannot use kpgdir yet, because the AP processor
    // is running in low  memory, so we use entrypgdir for the APs too.
    stack = kalloc();
    /// Pass parameters to the mem that the CPU can access at startup
    /// Parameter 1: virtual address of kernal stack
    /// Parameter 2: virtual address of mpenter
    /// Parameter 3: the initial mem map pg dir which maps virtual addr [0, 4M] and [KERNBASE, KERNBASE + 4M] to physical addr [0, 4M]
    /// The code in _binary_entryother_start is generated according to Makefile:111
    *(void**)(code-4) = stack + KSTACKSIZE;
    *(void(**)(void))(code-8) = mpenter;
    *(int**)(code-12) = (void *) V2P(entrypgdir); 

    lapicstartap(c->apicid, V2P(code));

    // wait for cpu to finish mpmain()
    while(c->started == 0)
      ;
  }
}

// The boot page table used in entry.S and entryother.S.
// Page directories (and page tables) must start on page boundaries,
// hence the __aligned__ attribute.
// PTE_PS in a page directory entry enables 4Mbyte pages.

__attribute__((__aligned__(PGSIZE)))
pde_t entrypgdir[NPDENTRIES] = {
  // Map VA's [0, 4MB) to PA's [0, 4MB)
  [0] = (0) | PTE_P | PTE_W | PTE_PS,
  // Map VA's [KERNBASE, KERNBASE+4MB) to PA's [0, 4MB)
  [KERNBASE>>PDXSHIFT] = (0) | PTE_P | PTE_W | PTE_PS,
};

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

