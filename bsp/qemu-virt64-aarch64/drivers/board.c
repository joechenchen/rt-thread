/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2012-11-20     Bernard    the first version
 * 2018-11-22     Jesven     add rt_hw_spin_lock
 *                           add rt_hw_spin_unlock
 *                           add smp ipi init
 */

#include <rthw.h>
#include <rtthread.h>
#include <mmu.h>

#ifdef RT_USING_SMART
#include <page.h>
#include <lwp_arch.h>
#endif

#include "board.h"
#include <interrupt.h>

#ifdef RT_USING_FDT
#include "interrupt.h"
#include "dtb_node.h"
#include <psci_api.h>
#include <cpu.h>
#endif

#ifdef RT_USING_SMART
struct mem_desc platform_mem_desc[] = {
    {KERNEL_VADDR_START, KERNEL_VADDR_START + 0x0fffffff, KERNEL_VADDR_START + PV_OFFSET, NORMAL_MEM}
};
#else
struct mem_desc platform_mem_desc[] =
{
    {0x40000000, 0x80000000 - 1, 0x40000000, NORMAL_MEM},
    // {PL031_RTC_BASE, PL031_RTC_BASE + 0x1000 - 1, PL031_RTC_BASE, DEVICE_MEM},
    // {PL061_GPIO_BASE, PL061_GPIO_BASE + 0x1000 - 1, PL061_GPIO_BASE, DEVICE_MEM},
    {PL011_UART0_BASE, PL011_UART0_BASE + ARCH_SECTION_SIZE - 1, PL011_UART0_BASE, DEVICE_MEM},
    {VIRTIO_MMIO_BASE, RT_ALIGN(VIRTIO_MMIO_BASE + VIRTIO_MAX_NR * VIRTIO_MMIO_SIZE, ARCH_SECTION_SIZE) - 1, VIRTIO_MMIO_BASE, DEVICE_MEM},
#ifdef BSP_USING_GICV2
    {GIC_PL390_DISTRIBUTOR_PPTR, GIC_PL390_DISTRIBUTOR_PPTR + ARCH_SECTION_SIZE - 1, GIC_PL390_DISTRIBUTOR_PPTR, DEVICE_MEM},
#endif
#ifdef BSP_USING_GICV3
    {GIC_PL500_DISTRIBUTOR_PPTR, GIC_PL500_DISTRIBUTOR_PPTR + 0x1000 - 1, GIC_PL500_DISTRIBUTOR_PPTR, DEVICE_MEM},
    {GIC_PL500_REDISTRIBUTOR_PPTR, GIC_PL500_REDISTRIBUTOR_PPTR + 0xf60000 - 1, GIC_PL500_REDISTRIBUTOR_PPTR, DEVICE_MEM},
#endif
};
#endif

const rt_uint32_t platform_mem_desc_size = sizeof(platform_mem_desc)/sizeof(platform_mem_desc[0]);

void idle_wfi(void)
{
    asm volatile ("wfi");
}

/**
 * This function will initialize board
 */

rt_mmu_info mmu_info;

extern size_t MMUTable[];

#ifdef RT_USING_SMART
rt_region_t init_page_region = {
    PAGE_START,
    PAGE_END,
};
#endif

void rt_hw_board_init(void)
{
#ifdef RT_USING_SMART
    rt_page_init(init_page_region);

    rt_hw_mmu_setup(platform_mem_desc, platform_mem_desc_size);

    rt_hw_mmu_map_init(&mmu_info, (void*)0xfffffffff0000000, 0x10000000, MMUTable, PV_OFFSET);

    arch_kuser_init(&mmu_info, (void*)0xffffffffffff0000);
#else
    rt_hw_mmu_setup(platform_mem_desc, platform_mem_desc_size);
    rt_hw_mmu_map_init(&mmu_info, (void*)0x80000000, 0x10000000, MMUTable, 0);
    rt_hw_mmu_ioremap_init(&mmu_info, (void*)0x80000000, 0x10000000);
#endif

    /* initialize hardware interrupt */
    rt_hw_interrupt_init();

    /* initialize system heap */
    rt_system_heap_init((void *)HEAP_BEGIN, (void *)HEAP_END);

    /* support debug feature before components init */
    rt_hw_uart_init();
    rt_console_set_device(RT_CONSOLE_DEVICE_NAME);

#ifdef RT_USING_FDT
    // TODO 0x44000000 should be replace by a variable
    void * fdt_start = (void *)0x44000000 - PV_OFFSET;
    device_tree_setup(fdt_start);

#ifdef RT_USING_SMP
    rt_hw_cpu_init();
#else
    psci_init();
#endif /* RT_USING_SMP */
#endif

    rt_components_board_init();

    rt_thread_idle_sethook(idle_wfi);

#ifdef RT_USING_SMP
    /* install IPI handle */
    rt_hw_ipi_handler_install(RT_SCHEDULE_IPI, rt_scheduler_ipi_handler);
#endif
}
