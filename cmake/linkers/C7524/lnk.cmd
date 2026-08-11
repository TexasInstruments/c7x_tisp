#if SOC == j722s
--ram_model
-heap  0x400000
-stack 0x400000 
--args 0x1000
--diag_suppress=10068 /* to suppress no matching section error */
--cinit_compression=off
-e _c_int00_secure

#define DDR0_ALLOCATED_START  0xA3000000

#define C7X_ALLOCATED_START DDR0_ALLOCATED_START

#define C7X_RESOURCE_TABLE_BASE (C7X_ALLOCATED_START + 0x00100000)
#define C7X_IPC_TRACE_BUFFER    (C7X_ALLOCATED_START + 0x00100400)
#define C7X_BOOT_BASE           (C7X_ALLOCATED_START + 0x00200000)
#define C7X_VECTOR_BASE         (C7X_ALLOCATED_START + 0x00400000)
#define C7X_DDR_SPACE_BASE      (C7X_ALLOCATED_START + 0x00410000)

MEMORY
{
    L2SRAM (RWX):  org = 0x7E000000,                len = 0x200000
    DDR0_RESERVED: org = 0x80000000,                len = 0x19800000         /*  Reserved for A53 OS */
    C7X_IPC_D:     org = C7X_ALLOCATED_START,       len = 0x00100000         /*  1MB DDR */
    C7X_BOOT_D:    org = C7X_BOOT_BASE,             len = 0x400              /*  1024B DDR */
    C7X_VECS_D:    org = C7X_VECTOR_BASE,           len = 0x4000             /*  16KB DDR */
    C7X_CIO_MEM:   org = C7X_DDR_SPACE_BASE,        len = 0x1000             /*  4KB */
    // C7X_DDR_SPACE: org = C7X_DDR_SPACE_BASE+0x1000, len = 0x00BF0000-0x1000  /*  11.9MB - 4KB DDR  */
    C7X_DDR_SPACE: org = C7X_DDR_SPACE_BASE+0x1000, len = 0x6000000-0x1000  /*  96MB - 4KB DDR  */
    LOG_SHM_MEM             : ORIGIN = 0xAD000000, LENGTH = 0x40000
}

SECTIONS
{
    boot:
    {
      boot.*<boot.oe71>(.text)
    } load > C7X_BOOT_D
    .vecs       >       C7X_VECS_D
    .secure_vecs    >   C7X_DDR_SPACE ALIGN(0x100000)
    .text:_c_int00_secure > C7X_DDR_SPACE ALIGN(0x200000)
    .text       >       C7X_DDR_SPACE ALIGN(0x100000)

    .bss        >       C7X_DDR_SPACE  /* Zero-initialized data */
    RUN_START(__BSS_START)
    RUN_END(__BSS_END)

    .data       >       C7X_DDR_SPACE  /* Initialized data */

    .cinit      >       C7X_DDR_SPACE  /* could be part of const */
    .init_array >       C7X_DDR_SPACE  /* C++ initializations */
    .stack      >       C7X_DDR_SPACE ALIGN(0x2000)
    .args       >       C7X_DDR_SPACE
    .cio        >       C7X_CIO_MEM
    .const      >       C7X_DDR_SPACE
    .switch     >       C7X_DDR_SPACE /* For exception handling. */
    .sysmem     >       C7X_DDR_SPACE /* heap */

    .ddrData    >       C7X_DDR_SPACE
    .l2sramData >       L2SRAM

    .tisp_node_arena   > L2SRAM ALIGN(0x80)   /* 128 bytes */
    .tisp_twiddle_arena > L2SRAM ALIGN(0x40)  /* 64 bytes  */
    .tisp_handle_arena  > L2SRAM ALIGN(0x20)  /* 32 bytes  */

    GROUP:              >  C7X_DDR_SPACE
    {
        .data.Mmu_tableArray          : type=NOINIT
        .data.Mmu_tableArraySlot      : type=NOINIT
        .data.Mmu_level1Table         : type=NOINIT
        .data.gMmu_tableArray_NS       : type=NOINIT
        .data.Mmu_tableArraySlot_NS   : type=NOINIT
        .data.Mmu_level1Table_NS      : type=NOINIT
    }

    .benchmark_buffer:     > C7X_DDR_SPACE ALIGN (32)

    /* this is used when Debug log's to shared memory is enabled, else this is not used */
    .bss.log_shared_mem  (NOLOAD) : {} > LOG_SHM_MEM
}
#elif SOC == AM275 

--ram_model
-heap  0x20000
-stack 0x20000
--args 0x1000
--diag_suppress=10068 /* to suppress no matching section error */
--cinit_compression=off
-e _c_int00_secure

/******************************************************************************/
/*   The SRAM address is aliased as follows for C7x for enabling CacheWT      */
/*   SRAM Address       |       Aliased address         |           Size      */
/*   0x72000000         |        0x80000000             |         0x600000    */
/******************************************************************************/

MEMORY
{
    C75_0_L2SRAM (RWX):  org = 0x7E000000,                len = 0x240000


    C75_0_OCRAM_ENTRY   (RWIX)   : ORIGIN = 0x80200000 LENGTH = 0x00000040
    C75_0_OCRAM   (RWIX)         : ORIGIN = 0x80200040 LENGTH = 0x00400000 - 0x40 - 0x1000
    C75_0_CIO_MEM                : ORIGIN = 0x80200040 + 0x00400000 - 0x40 - 0x1000 LENGTH = 0x1000

    HYPERRAM (RW)                : ORIGIN = 0x68100000 LENGTH = 0x04000000  /* 64 MB HyperRAM via FSS1 */

}

SECTIONS
{
    .vecs       >       C75_0_L2SRAM ALIGN(0x200000)
    .text:_c_int00_secure > C75_0_OCRAM_ENTRY ALIGN(0x200000)
    .text       >       C75_0_OCRAM

    .bss        >       C75_0_L2SRAM  /* Zero-initialized data */
    RUN_START(__BSS_START)
    RUN_END(__BSS_END)

    .data       >       C75_0_L2SRAM  /* Initialized data */

    .cinit      >       C75_0_OCRAM  /* could be part of const */
    .init_array >       C75_0_OCRAM  /* C++ initializations */
    .stack      >       C75_0_L2SRAM ALIGN(0x2000)
    .args       >       C75_0_L2SRAM
    .cio        >       C75_0_CIO_MEM
    .const      >       C75_0_OCRAM
    .switch     >       C75_0_L2SRAM /* For exception handling. */
    // .sysmem     >       C75_0_OCRAM /* heap */
    .sysmem     >       C75_0_L2SRAM/* heap */
    
    .ddrData          >       C75_0_OCRAM
    .l2sramData       >       C75_0_L2SRAM
    .staticData       >       C75_0_OCRAM
    .hyperRamData     >       HYPERRAM ALIGN(128)

    .tisp_node_arena   > C75_0_L2SRAM ALIGN(0x80)   /* 128 bytes */
    .tisp_twiddle_arena > C75_0_L2SRAM ALIGN(0x40)  /* 64 bytes  */
    .tisp_handle_arena  > C75_0_L2SRAM ALIGN(0x20)  /* 32 bytes  */

    .coeffMemory      > C75_0_OCRAM
    

    GROUP:              >  C75_0_OCRAM
    {
        .data.Mmu_tableArray          : type=NOINIT
        .data.Mmu_tableArraySlot      : type=NOINIT
        .data.Mmu_level1Table         : type=NOINIT
        .data.gMmu_tableArray_NS       : type=NOINIT
        .data.Mmu_tableArraySlot_NS   : type=NOINIT
        .data.Mmu_level1Table_NS      : type=NOINIT
    }

    .benchmark_buffer:     > C75_0_OCRAM ALIGN (32)

    /* this is used when Debug log's to shared memory is enabled, else this is not used */
    .bss.log_shared_mem  (NOLOAD) : {} > C75_0_OCRAM


}
#endif
