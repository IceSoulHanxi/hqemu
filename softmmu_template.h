/*
 *  Software MMU support
 *
 * Generate helpers used by TCG for qemu_ld/st ops and code load
 * functions.
 *
 * Included from target op helpers and exec.c.
 *
 *  Copyright (c) 2003 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#include "qemu/timer.h"
#include "exec/address-spaces.h"
#include "exec/memory.h"
#include "hqemu-config.h"

#define DATA_SIZE (1 << SHIFT)

#if DATA_SIZE == 8
#define SUFFIX q
#define LSUFFIX q
#define SDATA_TYPE  int64_t
#define DATA_TYPE  uint64_t
#elif DATA_SIZE == 4
#define SUFFIX l
#define LSUFFIX l
#define SDATA_TYPE  int32_t
#define DATA_TYPE  uint32_t
#elif DATA_SIZE == 2
#define SUFFIX w
#define LSUFFIX uw
#define SDATA_TYPE  int16_t
#define DATA_TYPE  uint16_t
#elif DATA_SIZE == 1
#define SUFFIX b
#define LSUFFIX ub
#define SDATA_TYPE  int8_t
#define DATA_TYPE  uint8_t
#else
#error unsupported data size
#endif


/* For the benefit of TCG generated code, we want to avoid the complication
   of ABI-specific return type promotion and always return a value extended
   to the register size of the host.  This is tcg_target_long, except in the
   case of a 32-bit host and 64-bit data, and for that we always have
   uint64_t.  Don't bother with this widened value for SOFTMMU_CODE_ACCESS.  */
#if defined(SOFTMMU_CODE_ACCESS) || DATA_SIZE == 8
# define WORD_TYPE  DATA_TYPE
# define USUFFIX    SUFFIX
#else
# define WORD_TYPE  tcg_target_ulong
# define USUFFIX    glue(u, SUFFIX)
# define SSUFFIX    glue(s, SUFFIX)
#endif

#ifdef SOFTMMU_CODE_ACCESS
#define READ_ACCESS_TYPE MMU_INST_FETCH
#define ADDR_READ addr_code
#else
#define READ_ACCESS_TYPE MMU_DATA_LOAD
#define ADDR_READ addr_read
#endif

#if DATA_SIZE == 8
# define BSWAP(X)  bswap64(X)
#elif DATA_SIZE == 4
# define BSWAP(X)  bswap32(X)
#elif DATA_SIZE == 2
# define BSWAP(X)  bswap16(X)
#else
# define BSWAP(X)  (X)
#endif

#ifdef TARGET_WORDS_BIGENDIAN
# define TGT_BE(X)  (X)
# define TGT_LE(X)  BSWAP(X)
#else
# define TGT_BE(X)  BSWAP(X)
# define TGT_LE(X)  (X)
#endif

#if DATA_SIZE == 1
# define helper_le_ld_name  glue(glue(helper_ret_ld, USUFFIX), MMUSUFFIX)
# define helper_be_ld_name  helper_le_ld_name
# define helper_le_lds_name glue(glue(helper_ret_ld, SSUFFIX), MMUSUFFIX)
# define helper_be_lds_name helper_le_lds_name
# define helper_le_st_name  glue(glue(helper_ret_st, SUFFIX), MMUSUFFIX)
# define helper_be_st_name  helper_le_st_name
#else
# define helper_le_ld_name  glue(glue(helper_le_ld, USUFFIX), MMUSUFFIX)
# define helper_be_ld_name  glue(glue(helper_be_ld, USUFFIX), MMUSUFFIX)
# define helper_le_lds_name glue(glue(helper_le_ld, SSUFFIX), MMUSUFFIX)
# define helper_be_lds_name glue(glue(helper_be_ld, SSUFFIX), MMUSUFFIX)
# define helper_le_st_name  glue(glue(helper_le_st, SUFFIX), MMUSUFFIX)
# define helper_be_st_name  glue(glue(helper_be_st, SUFFIX), MMUSUFFIX)
#endif

#ifdef TARGET_WORDS_BIGENDIAN
# define helper_te_ld_name  helper_be_ld_name
# define helper_te_st_name  helper_be_st_name
#else
# define helper_te_ld_name  helper_le_ld_name
# define helper_te_st_name  helper_le_st_name
#endif

#if defined(ENABLE_TLBVERSION)
#define TLB_IO_MASK          (TLB_NOTDIRTY | TLB_MMIO)
#define TLB_NONIO_MASK       (TARGET_PAGE_MASK | TLB_INVALID_MASK | TLB_VERSION_MASK)
#define page_val(addr, env)  (((tlbaddr_t)addr & TARGET_PAGE_MASK) | tlb_version(env))
#else
#define TLB_IO_MASK          (~TARGET_PAGE_MASK)
#define TLB_NONIO_MASK       (TARGET_PAGE_MASK | TLB_INVALID_MASK)
#define page_val(addr, env)  ((addr & TARGET_PAGE_MASK))
#endif

/* macro to check the victim tlb */
#define VICTIM_TLB_HIT(ty)                                                    \
({                                                                            \
    /* we are about to do a page table walk. our last hope is the             \
     * victim tlb. try to refill from the victim tlb before walking the       \
     * page table. */                                                         \
    int vidx;                                                                 \
    CPUIOTLBEntry tmpiotlb;                                                   \
    CPUTLBEntry tmptlb;                                                       \
    for (vidx = CPU_VTLB_SIZE-1; vidx >= 0; --vidx) {                         \
        if (env->tlb_v_table[mmu_idx][vidx].ty == page_val(addr, env)) {      \
            /* found entry in victim tlb, swap tlb and iotlb */               \
            tmptlb = env->tlb_table[mmu_idx][index];                          \
            env->tlb_table[mmu_idx][index] = env->tlb_v_table[mmu_idx][vidx]; \
            env->tlb_v_table[mmu_idx][vidx] = tmptlb;                         \
            tmpiotlb = env->iotlb[mmu_idx][index];                            \
            env->iotlb[mmu_idx][index] = env->iotlb_v[mmu_idx][vidx];         \
            env->iotlb_v[mmu_idx][vidx] = tmpiotlb;                           \
            break;                                                            \
        }                                                                     \
    }                                                                         \
    /* return true when there is a vtlb hit, i.e. vidx >=0 */                 \
    vidx >= 0;                                                                \
})

#ifndef SOFTMMU_CODE_ACCESS
static inline DATA_TYPE glue(io_read, SUFFIX)(CPUArchState *env,
                                              CPUIOTLBEntry *iotlbentry,
                                              target_ulong addr,
                                              uintptr_t retaddr)
{
    uint64_t val;
    CPUState *cpu = ENV_GET_CPU(env);
    hwaddr physaddr = iotlbentry->addr;
    MemoryRegion *mr = iotlb_to_region(cpu, physaddr, iotlbentry->attrs);

    physaddr = (physaddr & TARGET_PAGE_MASK) + addr;
    cpu->mem_io_pc = retaddr;
    if (mr != &io_mem_rom && mr != &io_mem_notdirty && !cpu->can_do_io) {
        cpu_io_recompile(cpu, retaddr);
    }

    cpu->mem_io_vaddr = addr;
    memory_region_dispatch_read(mr, physaddr, &val, 1 << SHIFT,
                                iotlbentry->attrs);
    return val;
}
#endif

WORD_TYPE helper_le_ld_name(CPUArchState *env, target_ulong addr,
                            TCGMemOpIdx oi, uintptr_t retaddr)
{
    unsigned mmu_idx = get_mmuidx(oi);
    int index = (addr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);
    tlbaddr_t tlb_addr = env->tlb_table[mmu_idx][index].ADDR_READ;
    uintptr_t haddr;
    DATA_TYPE res;

    /* Adjust the given return address.  */
    retaddr -= GETPC_ADJ;

    /* If the TLB entry is for a different page, reload and try again.  */
    if (page_val(addr, env) != (tlb_addr & TLB_NONIO_MASK)) {
        if ((addr & (DATA_SIZE - 1)) != 0
            && (get_memop(oi) & MO_AMASK) == MO_ALIGN) {
            cpu_unaligned_access(ENV_GET_CPU(env), addr, READ_ACCESS_TYPE,
                                 mmu_idx, retaddr);
        }
        if (!VICTIM_TLB_HIT(ADDR_READ)) {
            tlb_fill(ENV_GET_CPU(env), addr, READ_ACCESS_TYPE,
                     mmu_idx, retaddr);
        }
        tlb_addr = env->tlb_table[mmu_idx][index].ADDR_READ;
    }

    /* Handle an IO access.  */
    if (unlikely(tlb_addr & TLB_IO_MASK)) {
        CPUIOTLBEntry *iotlbentry;
        if ((addr & (DATA_SIZE - 1)) != 0) {
            goto do_unaligned_access;
        }
        iotlbentry = &env->iotlb[mmu_idx][index];

        /* ??? Note that the io helpers always read data in the target
           byte ordering.  We should push the LE/BE request down into io.  */
        res = glue(io_read, SUFFIX)(env, iotlbentry, addr, retaddr);
        res = TGT_LE(res);
        return res;
    }

    /* Handle slow unaligned access (it spans two pages or IO).  */
    if (DATA_SIZE > 1
        && unlikely((addr & ~TARGET_PAGE_MASK) + DATA_SIZE - 1
                    >= TARGET_PAGE_SIZE)) {
        target_ulong addr1, addr2;
        DATA_TYPE res1, res2;
        unsigned shift;
    do_unaligned_access:
        if ((get_memop(oi) & MO_AMASK) == MO_ALIGN) {
            cpu_unaligned_access(ENV_GET_CPU(env), addr, READ_ACCESS_TYPE,
                                 mmu_idx, retaddr);
        }
        addr1 = addr & ~(DATA_SIZE - 1);
        addr2 = addr1 + DATA_SIZE;
        /* Note the adjustment at the beginning of the function.
           Undo that for the recursion.  */
        res1 = helper_le_ld_name(env, addr1, oi, retaddr + GETPC_ADJ);
        res2 = helper_le_ld_name(env, addr2, oi, retaddr + GETPC_ADJ);
        shift = (addr & (DATA_SIZE - 1)) * 8;

        /* Little-endian combine.  */
        res = (res1 >> shift) | (res2 << ((DATA_SIZE * 8) - shift));
        return res;
    }

    /* Handle aligned access or unaligned access in the same page.  */
    if ((addr & (DATA_SIZE - 1)) != 0
        && (get_memop(oi) & MO_AMASK) == MO_ALIGN) {
        cpu_unaligned_access(ENV_GET_CPU(env), addr, READ_ACCESS_TYPE,
                             mmu_idx, retaddr);
    }

    haddr = addr + env->tlb_table[mmu_idx][index].addend;
#if DATA_SIZE == 1
    res = glue(glue(ld, LSUFFIX), _p)((uint8_t *)haddr);
#else
    res = glue(glue(ld, LSUFFIX), _le_p)((uint8_t *)haddr);
#endif
    return res;
}

#if DATA_SIZE > 1
WORD_TYPE helper_be_ld_name(CPUArchState *env, target_ulong addr,
                            TCGMemOpIdx oi, uintptr_t retaddr)
{
    unsigned mmu_idx = get_mmuidx(oi);
    int index = (addr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);
    tlbaddr_t tlb_addr = env->tlb_table[mmu_idx][index].ADDR_READ;
    uintptr_t haddr;
    DATA_TYPE res;

    /* Adjust the given return address.  */
    retaddr -= GETPC_ADJ;

    /* If the TLB entry is for a different page, reload and try again.  */
    if (page_val(addr, env) != (tlb_addr & TLB_NONIO_MASK)) {
        if ((addr & (DATA_SIZE - 1)) != 0
            && (get_memop(oi) & MO_AMASK) == MO_ALIGN) {
            cpu_unaligned_access(ENV_GET_CPU(env), addr, READ_ACCESS_TYPE,
                                 mmu_idx, retaddr);
        }
        if (!VICTIM_TLB_HIT(ADDR_READ)) {
            tlb_fill(ENV_GET_CPU(env), addr, READ_ACCESS_TYPE,
                     mmu_idx, retaddr);
        }
        tlb_addr = env->tlb_table[mmu_idx][index].ADDR_READ;
    }

    /* Handle an IO access.  */
    if (unlikely(tlb_addr & TLB_IO_MASK)) {
        CPUIOTLBEntry *iotlbentry;
        if ((addr & (DATA_SIZE - 1)) != 0) {
            goto do_unaligned_access;
        }
        iotlbentry = &env->iotlb[mmu_idx][index];

        /* ??? Note that the io helpers always read data in the target
           byte ordering.  We should push the LE/BE request down into io.  */
        res = glue(io_read, SUFFIX)(env, iotlbentry, addr, retaddr);
        res = TGT_BE(res);
        return res;
    }

    /* Handle slow unaligned access (it spans two pages or IO).  */
    if (DATA_SIZE > 1
        && unlikely((addr & ~TARGET_PAGE_MASK) + DATA_SIZE - 1
                    >= TARGET_PAGE_SIZE)) {
        target_ulong addr1, addr2;
        DATA_TYPE res1, res2;
        unsigned shift;
    do_unaligned_access:
        if ((get_memop(oi) & MO_AMASK) == MO_ALIGN) {
            cpu_unaligned_access(ENV_GET_CPU(env), addr, READ_ACCESS_TYPE,
                                 mmu_idx, retaddr);
        }
        addr1 = addr & ~(DATA_SIZE - 1);
        addr2 = addr1 + DATA_SIZE;
        /* Note the adjustment at the beginning of the function.
           Undo that for the recursion.  */
        res1 = helper_be_ld_name(env, addr1, oi, retaddr + GETPC_ADJ);
        res2 = helper_be_ld_name(env, addr2, oi, retaddr + GETPC_ADJ);
        shift = (addr & (DATA_SIZE - 1)) * 8;

        /* Big-endian combine.  */
        res = (res1 << shift) | (res2 >> ((DATA_SIZE * 8) - shift));
        return res;
    }

    /* Handle aligned access or unaligned access in the same page.  */
    if ((addr & (DATA_SIZE - 1)) != 0
        && (get_memop(oi) & MO_AMASK) == MO_ALIGN) {
        cpu_unaligned_access(ENV_GET_CPU(env), addr, READ_ACCESS_TYPE,
                             mmu_idx, retaddr);
    }

    haddr = addr + env->tlb_table[mmu_idx][index].addend;
    res = glue(glue(ld, LSUFFIX), _be_p)((uint8_t *)haddr);
    return res;
}
#endif /* DATA_SIZE > 1 */

#ifndef SOFTMMU_CODE_ACCESS

/* Provide signed versions of the load routines as well.  We can of course
   avoid this for 64-bit data, or for 32-bit data on 32-bit host.  */
#if DATA_SIZE * 8 < TCG_TARGET_REG_BITS
WORD_TYPE helper_le_lds_name(CPUArchState *env, target_ulong addr,
                             TCGMemOpIdx oi, uintptr_t retaddr)
{
    return (SDATA_TYPE)helper_le_ld_name(env, addr, oi, retaddr);
}

# if DATA_SIZE > 1
WORD_TYPE helper_be_lds_name(CPUArchState *env, target_ulong addr,
                             TCGMemOpIdx oi, uintptr_t retaddr)
{
    return (SDATA_TYPE)helper_be_ld_name(env, addr, oi, retaddr);
}
# endif
#endif

static inline void glue(io_write, SUFFIX)(CPUArchState *env,
                                          CPUIOTLBEntry *iotlbentry,
                                          DATA_TYPE val,
                                          target_ulong addr,
                                          uintptr_t retaddr)
{
    CPUState *cpu = ENV_GET_CPU(env);
    hwaddr physaddr = iotlbentry->addr;
    MemoryRegion *mr = iotlb_to_region(cpu, physaddr, iotlbentry->attrs);

    physaddr = (physaddr & TARGET_PAGE_MASK) + addr;
    if (mr != &io_mem_rom && mr != &io_mem_notdirty && !cpu->can_do_io) {
        cpu_io_recompile(cpu, retaddr);
    }

    cpu->mem_io_vaddr = addr;
    cpu->mem_io_pc = retaddr;
    memory_region_dispatch_write(mr, physaddr, val, 1 << SHIFT,
                                 iotlbentry->attrs);
}

void helper_le_st_name(CPUArchState *env, target_ulong addr, DATA_TYPE val,
                       TCGMemOpIdx oi, uintptr_t retaddr)
{
    unsigned mmu_idx = get_mmuidx(oi);
    int index = (addr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);
    tlbaddr_t tlb_addr = env->tlb_table[mmu_idx][index].addr_write;
    uintptr_t haddr;

    /* Adjust the given return address.  */
    retaddr -= GETPC_ADJ;

    /* If the TLB entry is for a different page, reload and try again.  */
    if (page_val(addr, env) != (tlb_addr & TLB_NONIO_MASK)) {
        if ((addr & (DATA_SIZE - 1)) != 0
            && (get_memop(oi) & MO_AMASK) == MO_ALIGN) {
            cpu_unaligned_access(ENV_GET_CPU(env), addr, MMU_DATA_STORE,
                                 mmu_idx, retaddr);
        }
        if (!VICTIM_TLB_HIT(addr_write)) {
            tlb_fill(ENV_GET_CPU(env), addr, MMU_DATA_STORE, mmu_idx, retaddr);
        }
        tlb_addr = env->tlb_table[mmu_idx][index].addr_write;
    }

    /* Handle an IO access.  */
    if (unlikely(tlb_addr & TLB_IO_MASK)) {
        CPUIOTLBEntry *iotlbentry;
        if ((addr & (DATA_SIZE - 1)) != 0) {
            goto do_unaligned_access;
        }
        iotlbentry = &env->iotlb[mmu_idx][index];

        /* ??? Note that the io helpers always read data in the target
           byte ordering.  We should push the LE/BE request down into io.  */
        val = TGT_LE(val);
        glue(io_write, SUFFIX)(env, iotlbentry, val, addr, retaddr);
        return;
    }

    /* Handle slow unaligned access (it spans two pages or IO).  */
    if (DATA_SIZE > 1
        && unlikely((addr & ~TARGET_PAGE_MASK) + DATA_SIZE - 1
                     >= TARGET_PAGE_SIZE)) {
        int i;
    do_unaligned_access:
        if ((get_memop(oi) & MO_AMASK) == MO_ALIGN) {
            cpu_unaligned_access(ENV_GET_CPU(env), addr, MMU_DATA_STORE,
                                 mmu_idx, retaddr);
        }
        /* XXX: not efficient, but simple */
        /* Note: relies on the fact that tlb_fill() does not remove the
         * previous page from the TLB cache.  */
        for (i = DATA_SIZE - 1; i >= 0; i--) {
            /* Little-endian extract.  */
            uint8_t val8 = val >> (i * 8);
            /* Note the adjustment at the beginning of the function.
               Undo that for the recursion.  */
            glue(helper_ret_stb, MMUSUFFIX)(env, addr + i, val8,
                                            oi, retaddr + GETPC_ADJ);
        }
        return;
    }

    /* Handle aligned access or unaligned access in the same page.  */
    if ((addr & (DATA_SIZE - 1)) != 0
        && (get_memop(oi) & MO_AMASK) == MO_ALIGN) {
        cpu_unaligned_access(ENV_GET_CPU(env), addr, MMU_DATA_STORE,
                             mmu_idx, retaddr);
    }

    haddr = addr + env->tlb_table[mmu_idx][index].addend;
#if DATA_SIZE == 1
    glue(glue(st, SUFFIX), _p)((uint8_t *)haddr, val);
#else
    glue(glue(st, SUFFIX), _le_p)((uint8_t *)haddr, val);
#endif
}

#if DATA_SIZE > 1
void helper_be_st_name(CPUArchState *env, target_ulong addr, DATA_TYPE val,
                       TCGMemOpIdx oi, uintptr_t retaddr)
{
    unsigned mmu_idx = get_mmuidx(oi);
    int index = (addr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);
    tlbaddr_t tlb_addr = env->tlb_table[mmu_idx][index].addr_write;
    uintptr_t haddr;

    /* Adjust the given return address.  */
    retaddr -= GETPC_ADJ;

    /* If the TLB entry is for a different page, reload and try again.  */
    if (page_val(addr, env) != (tlb_addr & TLB_NONIO_MASK)) {
        if ((addr & (DATA_SIZE - 1)) != 0
            && (get_memop(oi) & MO_AMASK) == MO_ALIGN) {
            cpu_unaligned_access(ENV_GET_CPU(env), addr, MMU_DATA_STORE,
                                 mmu_idx, retaddr);
        }
        if (!VICTIM_TLB_HIT(addr_write)) {
            tlb_fill(ENV_GET_CPU(env), addr, MMU_DATA_STORE, mmu_idx, retaddr);
        }
        tlb_addr = env->tlb_table[mmu_idx][index].addr_write;
    }

    /* Handle an IO access.  */
    if (unlikely(tlb_addr & TLB_IO_MASK)) {
        CPUIOTLBEntry *iotlbentry;
        if ((addr & (DATA_SIZE - 1)) != 0) {
            goto do_unaligned_access;
        }
        iotlbentry = &env->iotlb[mmu_idx][index];

        /* ??? Note that the io helpers always read data in the target
           byte ordering.  We should push the LE/BE request down into io.  */
        val = TGT_BE(val);
        glue(io_write, SUFFIX)(env, iotlbentry, val, addr, retaddr);
        return;
    }

    /* Handle slow unaligned access (it spans two pages or IO).  */
    if (DATA_SIZE > 1
        && unlikely((addr & ~TARGET_PAGE_MASK) + DATA_SIZE - 1
                     >= TARGET_PAGE_SIZE)) {
        int i;
    do_unaligned_access:
        if ((get_memop(oi) & MO_AMASK) == MO_ALIGN) {
            cpu_unaligned_access(ENV_GET_CPU(env), addr, MMU_DATA_STORE,
                                 mmu_idx, retaddr);
        }
        /* XXX: not efficient, but simple */
        /* Note: relies on the fact that tlb_fill() does not remove the
         * previous page from the TLB cache.  */
        for (i = DATA_SIZE - 1; i >= 0; i--) {
            /* Big-endian extract.  */
            uint8_t val8 = val >> (((DATA_SIZE - 1) * 8) - (i * 8));
            /* Note the adjustment at the beginning of the function.
               Undo that for the recursion.  */
            glue(helper_ret_stb, MMUSUFFIX)(env, addr + i, val8,
                                            oi, retaddr + GETPC_ADJ);
        }
        return;
    }

    /* Handle aligned access or unaligned access in the same page.  */
    if ((addr & (DATA_SIZE - 1)) != 0
        && (get_memop(oi) & MO_AMASK) == MO_ALIGN) {
        cpu_unaligned_access(ENV_GET_CPU(env), addr, MMU_DATA_STORE,
                             mmu_idx, retaddr);
    }

    haddr = addr + env->tlb_table[mmu_idx][index].addend;
    glue(glue(st, SUFFIX), _be_p)((uint8_t *)haddr, val);
}
#endif /* DATA_SIZE > 1 */

#if DATA_SIZE == 1
/* Probe for whether the specified guest write access is permitted.
 * If it is not permitted then an exception will be taken in the same
 * way as if this were a real write access (and we will not return).
 * Otherwise the function will return, and there will be a valid
 * entry in the TLB for this access.
 */
void probe_write(CPUArchState *env, target_ulong addr, int mmu_idx,
                 uintptr_t retaddr)
{
    int index = (addr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);
    tlbaddr_t tlb_addr = env->tlb_table[mmu_idx][index].addr_write;

    if (page_val(addr, env) != (tlb_addr & TLB_NONIO_MASK)) {
        /* TLB entry is for a different page */
        if (!VICTIM_TLB_HIT(addr_write)) {
            tlb_fill(ENV_GET_CPU(env), addr, MMU_DATA_STORE, mmu_idx, retaddr);
        }
    }
}
#endif
#endif /* !defined(SOFTMMU_CODE_ACCESS) */

#include "softmmu_template_llvm.h"

#undef TLB_IO_MASK
#undef TLB_NONIO_MASK
#undef page_val
#undef READ_ACCESS_TYPE
#undef SHIFT
#undef DATA_TYPE
#undef SUFFIX
#undef LSUFFIX
#undef DATA_SIZE
#undef ADDR_READ
#undef WORD_TYPE
#undef SDATA_TYPE
#undef USUFFIX
#undef SSUFFIX
#undef BSWAP
#undef TGT_BE
#undef TGT_LE
#undef CPU_BE
#undef CPU_LE
#undef helper_le_ld_name
#undef helper_be_ld_name
#undef helper_le_lds_name
#undef helper_be_lds_name
#undef helper_le_st_name
#undef helper_be_st_name
#undef helper_te_ld_name
#undef helper_te_st_name
