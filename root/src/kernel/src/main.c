#include <stdint.h>
#include <stddef.h>
#include <cio.h>
#include <klib.h>
#include <serial.h>
#include <tty.h>
#include <vga_textmode.h>
#include <vbe_tty.h>
#include <vbe.h>
#include <e820.h>
#include <mm.h>
#include <idt.h>
#include <pic.h>
#include <acpi.h>
#include <cmdline.h>
#include <pit.h>
#include <smp.h>
#include <task.h>
#include <ata.h>
#include <dev.h>
#include <fs.h>
#include <elf.h>
#include <pci.h>
#include <ahci.h>
#include <time.h>
#include <kbd.h>
#include <irq.h>

void kmain_thread(void *arg) {
    (void)arg;

    /* Execute a test process */
    kexec("/bin/hello", 0, 0,
          "/dev/tty", "/dev/tty", "/dev/tty");

    kprint(KPRN_INFO, "kmain: End of init.");

    for (;;) asm volatile ("hlt;");
}

/* Main kernel entry point, all the things should be initialised */
void kmain(void) {
    init_idt();

    init_com1();
    init_vga_textmode();

    init_tty();

    kprint(KPRN_INFO, "Kernel booted");
    kprint(KPRN_INFO, "Build time: %s", BUILD_TIME);
    kprint(KPRN_INFO, "Command line: %s", cmdline);

    struct s_time_t s_time;

    bios_get_time(&s_time);
    kprint(KPRN_INFO, "Current date & time: %u/%u/%u %u:%u:%u",
           s_time.years, s_time.months, s_time.days,
           s_time.hours, s_time.minutes, s_time.seconds);

    /* Memory-related stuff */
    init_e820();
    init_pmm();
    init_vmm();

    /* Early inits */
    init_vbe();
    init_vbe_tty();

    /*** NO MORE REAL MODE CALLS AFTER THIS POINT ***/
    flush_irqs();
    init_acpi();
    init_pic();

    /* Enable interrupts on BSP */
    asm volatile ("sti");

    init_pit();
    init_smp();

    /* Initialise device drivers */
    init_pci();
    init_ahci();
    init_ata();
    init_kbd();

    /* Initialise Virtual Filesystem */
    init_vfs();

    /* Initialise filesystem drivers */
    init_devfs();
    init_echfs();
    init_iso9660();

    /* Mount /dev */
    mount("devfs", "/dev", "devfs", 0, 0);

    /* Mount /dev/hda on / */
    if (mount("/dev/hda", "/", "echfs", 0, 0))
        mount("/dev/hda", "/", "iso9660", 0, 0);

    /* Initialise scheduler */
    init_sched();

    /* Unlock the scheduler for the first time */
    spinlock_release(&scheduler_lock);

    /* Start a main kernel thread which will take over when the scheduler is running */
    task_tcreate(0, tcreate_fn_call, TCREATE_FN_CALL_DATA(kmain_thread, 0));

    /*** DO NOT ADD ANYTHING TO THIS FUNCTION AFTER THIS POINT, ADD TO kmain_thread
         INSTEAD! ***/

    /* Pre-scheduler init done. Wait for the main kernel thread to be scheduled. */
    for (;;) asm volatile ("hlt");
}
