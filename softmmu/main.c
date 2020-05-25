/*
 * QEMU System Emulator
 *
 * Copyright (c) 2003-2020 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "sysemu/sysemu.h"

#ifdef CONFIG_SDL
#if defined(__APPLE__) || defined(main)
#include <SDL.h>
int main(int argc, char **argv)
{
    return qemu_main(argc, argv, NULL);
}
#undef main
#define main qemu_main
#endif
#endif /* CONFIG_SDL */

#ifdef CONFIG_COCOA
#undef main
#define main qemu_main
#endif /* CONFIG_COCOA */

#include <stdio.h>


#include "../target/riscv/cpu-param.h"
#include "../include/qemu/osdep.h"
#include "../include/exec/cpu-defs.h"
#include "../target/riscv/cpu.h"
#include "../target/riscv/pmp.h"
#include "assert.h"


int main(int argc, char **argv, char **envp)
{
    
//    qemu_init(argc, argv, envp);
//    qemu_main_loop();
//    qemu_cleanup();
    printf("hello world\n");

// pmp_hart_has_privs(CPURISCVState *env, target_ulong addr,
    // target_ulong size, pmp_priv_t priv, target_ulong mode)

    RISCVCPU *cpu = g_new0(RISCVCPU, 1);
    CPURISCVState *env = &cpu->env;
    
    assert(env->pmp_state.num_rules == 0);

    int32_t pmp_idx = 0;


    pmpaddr_csr_write(env, 0, 0x80200000 >> 2);
    pmpcfg_csr_write(env, 0, PMP_READ | PMP_WRITE | (PMP_AMATCH_TOR << 3));    
    
    uint64_t val8 = PMP_READ | PMP_WRITE | (PMP_AMATCH_TOR << 3);
    uint64_t val16 = val8 << 8 | val8;
    uint64_t val32 = val16 << 16 | val16;
    uint64_t val64 = val32 << 32 | val32;
    for (int i = 0; i < 16; i++){
        pmpaddr_csr_write(env, pmp_idx++, (0x80200000 | (0x00200000 + 0x00100000 * i)) >> 2);    
    }
        // pmpcfg_csr_write(env, 0, val64 & ((a << 56) - 1));
    pmpcfg_csr_write(env, 0, val64);

    pmpcfg_csr_write(env, 2, val64);

    printf("%d\n", env->pmp_state.num_rules);



    printf("pmp_hart_has_privs(env, 0x80200000, 0, PMP_READ, PRV_M) = %d\n", pmp_hart_has_privs(env, 0x80200000, 0, PMP_READ, PRV_M));
    printf("pmp_hart_has_privs(env, 0x80200000, 0, PMP_WRITE, PRV_M) = %d\n", pmp_hart_has_privs(env, 0x80200000, 0, PMP_WRITE, PRV_M));
    printf("pmp_hart_has_privs(env, 0x80200000, 0, PMP_EXEC, PRV_M) = %d\n", pmp_hart_has_privs(env, 0x80200000, 0, PMP_EXEC, PRV_M));
    printf("pmp_hart_has_privs(env, 0x80200000, 0, PMP_READ, PRV_U) = %d\n", pmp_hart_has_privs(env, 0x80200000, 0, PMP_READ, PRV_U));
    printf("pmp_hart_has_privs(env, 0x80200000, 0, PMP_WRITE, PRV_U) = %d\n", pmp_hart_has_privs(env, 0x80200000, 0, PMP_WRITE, PRV_U));
    printf("pmp_hart_has_privs(env, 0x80200000, 0, PMP_EXEC, PRV_U) = %d\n", pmp_hart_has_privs(env, 0x80200000, 0, PMP_EXEC, PRV_U));

    printf("pmp_hart_has_privs(env, 0x80100000, 0, PMP_READ, PRV_M) = %d\n", pmp_hart_has_privs(env, 0x80100000, 0, PMP_READ, PRV_M));
    printf("pmp_hart_has_privs(env, 0x80100000, 0, PMP_WRITE, PRV_M) = %d\n", pmp_hart_has_privs(env, 0x80100000, 0, PMP_WRITE, PRV_M));
    printf("pmp_hart_has_privs(env, 0x80100000, 0, PMP_EXEC, PRV_M) = %d\n", pmp_hart_has_privs(env, 0x80100000, 0, PMP_EXEC, PRV_M));
    printf("pmp_hart_has_privs(env, 0x80100000, 0, PMP_READ, PRV_U) = %d\n", pmp_hart_has_privs(env, 0x80100000, 0, PMP_READ, PRV_U));
    printf("pmp_hart_has_privs(env, 0x80100000, 0, PMP_WRITE, PRV_U) = %d\n", pmp_hart_has_privs(env, 0x80100000, 0, PMP_WRITE, PRV_U));
    printf("pmp_hart_has_privs(env, 0x80100000, 0, PMP_EXEC, PRV_U) = %d\n", pmp_hart_has_privs(env, 0x80100000, 0, PMP_EXEC, PRV_U));

    g_free(cpu);

    return 0;
}
