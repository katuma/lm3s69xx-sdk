#include "lwip/opt.h"
#include "lwip/sys.h"

#include "inc/hw_ints.h"
#include "inc/hw_ethernet.h"
#include "inc/hw_memmap.h"
#include "inc/hw_nvic.h"
#include "inc/hw_sysctl.h"
#include "inc/hw_types.h"

#include "driverlib/rom_map.h"
#include "driverlib/interrupt.h"

u32_t now;

//////////////////// low level arch stuff //////////////////////////
u32_t sys_now(void)
{
    return now;
}

// CLI
sys_prot_t sys_arch_protect(void)
{
    return((sys_prot_t)MAP_IntMasterDisable());
}

// STI
void sys_arch_unprotect(sys_prot_t lev)
{
    if((lev&1)==0) MAP_IntMasterEnable();
}


