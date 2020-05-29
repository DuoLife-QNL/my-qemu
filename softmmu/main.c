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

void pmpcfg_set_value(CPURISCVState *env, int idx, uint64_t val);
void assert_exact(CPURISCVState *env, int idx, uint8_t priv, uint64_t mode);

void pmpcfg_set_value(CPURISCVState *env, int idx, uint64_t val) {
    uint64_t raw_val = pmpcfg_csr_read(env, (idx >> 3) << 1);

    int xidx = idx & 7;
    raw_val &= ~(0xffULL << (xidx << 3));
    raw_val |= (val << (xidx << 3));
    pmpcfg_csr_write(env, (idx >> 3) << 1, raw_val);
}

uint64_t testing_addr[18];

void assert_exact(CPURISCVState *env, int idx, uint8_t priv, uint64_t mode) {
    printf("ok: ");
    int x = 0;
    for(int sub_priv = priv; sub_priv; sub_priv = (sub_priv-1) & priv)
    {
        printf("%3x ", sub_priv);
        fflush(stdout);
        assert(pmp_hart_has_privs(env, testing_addr[idx], 0, sub_priv, mode));
        x++;
    }
    priv = 0x7 ^ priv;

    for(;x < 7;x++)
    {
        printf("    ");
    }

    printf("vio: ");
    for(int sub_priv = priv; sub_priv; sub_priv = (sub_priv-1) & priv)
    {
        printf("%3x ", sub_priv);
        fflush(stdout);
        assert(!pmp_hart_has_privs(env, testing_addr[idx], 0, sub_priv, mode));
    }
    printf("\n");
}

#define assert_exact_test(idx, priv, mode) printf("calling assert_exact(%d, %d, %llu): ", idx, priv, (long long unsigned int)(mode));\
    assert_exact(env, idx, priv, mode)


int main(int argc, char **argv, char **envp)
{

    RISCVCPU *cpu = g_new0(RISCVCPU, 1);
    CPURISCVState *env = &cpu->env;
    
    assert(env->pmp_state.num_rules == 0);

    int32_t pmp_idx = 0;


    pmpaddr_csr_write(env, 0, 0x80200000 >> 2);
    pmpcfg_csr_write(env, 0, PMP_READ | PMP_WRITE | (PMP_AMATCH_TOR << 3));    
    
    for (int i = 0; i < 16; i++){
        pmpaddr_csr_write(env, pmp_idx++, (0x80200000 + (0x00200000 + 0x00100000 * i)) >> 2);   
        testing_addr[i] = (0x80200000 + (0x00200000 + 0x00100000 * i)) - 1; 
    }
    pmp_idx = 0;

    // 0
    pmpcfg_set_value(env, pmp_idx++, PMP_READ | PMP_EXEC | PMP_WRITE | 0        | (PMP_AMATCH_TOR << 3));
    pmpcfg_set_value(env, pmp_idx++, PMP_READ | PMP_EXEC | PMP_WRITE | PMP_LOCK | (PMP_AMATCH_TOR << 3));
    pmpcfg_set_value(env, pmp_idx++, PMP_READ | 0        | PMP_WRITE | 0        | (PMP_AMATCH_TOR << 3));
    pmpcfg_set_value(env, pmp_idx++, 0        | PMP_EXEC | PMP_WRITE | 0        | (PMP_AMATCH_TOR << 3));
    
    // 4
    pmpcfg_set_value(env, pmp_idx++, PMP_READ | 0        | 0         | 0        | (PMP_AMATCH_TOR << 3));
    pmpcfg_set_value(env, pmp_idx++, 0        | PMP_EXEC | 0         | 0        | (PMP_AMATCH_TOR << 3));
    pmpcfg_set_value(env, pmp_idx++, 0        | 0        | PMP_WRITE | 0        | (PMP_AMATCH_TOR << 3));
    pmpcfg_set_value(env, pmp_idx++, 0        | 0        | 0         | 0        | (PMP_AMATCH_TOR << 3));
    
    // 8
    pmpcfg_set_value(env, pmp_idx++, PMP_READ | 0        | 0         | 0        | (PMP_AMATCH_TOR << 3));
    pmpcfg_set_value(env, pmp_idx++, PMP_READ | 0        | 0         | PMP_LOCK | (PMP_AMATCH_TOR << 3));
    
    printf("%d\n", env->pmp_state.num_rules);
    
    env->mseccfg = 0;
    printf("env->mseccfg = 0;\n");
    
    // no bits of mseccfg is set

    // 0
    // pmpcfg_set_value(env, pmp_idx++, PMP_READ | PMP_EXEC | PMP_WRITE | 0        | (PMP_AMATCH_TOR << 3));
    // pmpcfg_set_value(env, pmp_idx++, PMP_READ | PMP_EXEC | PMP_WRITE | PMP_LOCK | (PMP_AMATCH_TOR << 3));
    // pmpcfg_set_value(env, pmp_idx++, PMP_READ | 0        | PMP_WRITE | 0        | (PMP_AMATCH_TOR << 3));
    // pmpcfg_set_value(env, pmp_idx++, 0        | PMP_EXEC | PMP_WRITE | 0        | (PMP_AMATCH_TOR << 3));

    assert_exact_test(0, PMP_READ | PMP_EXEC | PMP_WRITE, PRV_M);
    assert_exact_test(0, PMP_READ | PMP_EXEC | PMP_WRITE, PRV_S);
    assert_exact_test(1, PMP_READ | PMP_EXEC | PMP_WRITE, PRV_M);
    assert_exact_test(1, PMP_READ | PMP_EXEC | PMP_WRITE, PRV_S);
    assert_exact_test(2, PMP_READ | PMP_EXEC | PMP_WRITE, PRV_M);
    assert_exact_test(2, PMP_READ | 0        | PMP_WRITE, PRV_S);
    assert_exact_test(3, PMP_READ | PMP_EXEC | PMP_WRITE, PRV_M);
    assert_exact_test(3, 0        | PMP_EXEC | PMP_WRITE, PRV_S);
    
    // 4
    // pmpcfg_set_value(env, pmp_idx++, PMP_READ | 0        | 0         | 0        | (PMP_AMATCH_TOR << 3));
    // pmpcfg_set_value(env, pmp_idx++, 0        | PMP_EXEC | 0         | 0        | (PMP_AMATCH_TOR << 3));
    // pmpcfg_set_value(env, pmp_idx++, 0        | 0        | PMP_WRITE | 0        | (PMP_AMATCH_TOR << 3));
    // pmpcfg_set_value(env, pmp_idx++, 0        | 0        | 0         | 0        | (PMP_AMATCH_TOR << 3));

    assert_exact_test(4, PMP_READ | PMP_EXEC | PMP_WRITE, PRV_M);
    assert_exact_test(4, PMP_READ | 0        | 0        , PRV_S);
    assert_exact_test(5, PMP_READ | PMP_EXEC | PMP_WRITE, PRV_M);
    assert_exact_test(5, 0        | PMP_EXEC | 0        , PRV_S);
    assert_exact_test(6, PMP_READ | PMP_EXEC | PMP_WRITE, PRV_M);
    assert_exact_test(6, 0        | 0        | PMP_WRITE, PRV_S);
    assert_exact_test(7, PMP_READ | PMP_EXEC | PMP_WRITE, PRV_M);
    assert_exact_test(7, 0        | 0        | 0        , PRV_S);

    // 8
    // pmpcfg_set_value(env, pmp_idx++, PMP_READ | 0        | 0         | 0        | (PMP_AMATCH_TOR << 3));
    // pmpcfg_set_value(env, pmp_idx++, PMP_READ | 0        | 0         | PMP_LOCK | (PMP_AMATCH_TOR << 3));

    assert_exact_test(9, PMP_READ | 0        | 0        , PRV_M);
    assert_exact_test(9, PMP_READ | 0        | 0        , PRV_S);

    env->mseccfg = MSECCFG_MML;
    printf("env->mseccfg = MSECCFG_MML;\n");

    // 0
    // pmpcfg_set_value(env, pmp_idx++, PMP_READ | PMP_EXEC | PMP_WRITE | 0        | (PMP_AMATCH_TOR << 3));
    // pmpcfg_set_value(env, pmp_idx++, PMP_READ | PMP_EXEC | PMP_WRITE | PMP_LOCK | (PMP_AMATCH_TOR << 3));
    // pmpcfg_set_value(env, pmp_idx++, PMP_READ | 0        | PMP_WRITE | 0        | (PMP_AMATCH_TOR << 3));
    // pmpcfg_set_value(env, pmp_idx++, 0        | PMP_EXEC | PMP_WRITE | 0        | (PMP_AMATCH_TOR << 3));

    assert_exact_test(0, 0        | 0        | 0        , PRV_M);
    assert_exact_test(0, PMP_READ | PMP_EXEC | PMP_WRITE, PRV_S);
    assert_exact_test(1, PMP_READ | PMP_EXEC | PMP_WRITE, PRV_M);
    assert_exact_test(1, 0        | 0        | 0        , PRV_S);
    assert_exact_test(2, 0        | 0        | 0        , PRV_M);
    assert_exact_test(2, PMP_READ | 0        | PMP_WRITE, PRV_S);
    assert_exact_test(3, PMP_READ | 0        | PMP_WRITE, PRV_M);
    assert_exact_test(3, PMP_READ | 0        | PMP_WRITE, PRV_S);
    
    // 4
    // pmpcfg_set_value(env, pmp_idx++, PMP_READ | 0        | 0         | 0        | (PMP_AMATCH_TOR << 3));
    // pmpcfg_set_value(env, pmp_idx++, 0        | PMP_EXEC | 0         | 0        | (PMP_AMATCH_TOR << 3));
    // pmpcfg_set_value(env, pmp_idx++, 0        | 0        | PMP_WRITE | 0        | (PMP_AMATCH_TOR << 3));
    // pmpcfg_set_value(env, pmp_idx++, 0        | 0        | 0         | 0        | (PMP_AMATCH_TOR << 3));

    assert_exact_test(4, 0        | 0        | 0        , PRV_M);
    assert_exact_test(4, PMP_READ | 0        | 0        , PRV_S);
    assert_exact_test(5, 0        | 0        | 0        , PRV_M);
    assert_exact_test(5, 0        | PMP_EXEC | 0        , PRV_S);
    assert_exact_test(6, PMP_READ | 0        | PMP_WRITE, PRV_M);
    assert_exact_test(6, PMP_READ | 0        | 0        , PRV_S);
    assert_exact_test(7, 0        | 0        | 0        , PRV_M);
    assert_exact_test(7, 0        | 0        | 0        , PRV_S);

    // 8
    // pmpcfg_set_value(env, pmp_idx++, PMP_READ | 0        | 0         | 0        | (PMP_AMATCH_TOR << 3));
    // pmpcfg_set_value(env, pmp_idx++, PMP_READ | 0        | 0         | PMP_LOCK | (PMP_AMATCH_TOR << 3));

    assert_exact_test(9, PMP_READ | 0        | 0        , PRV_M);
    assert_exact_test(9, 0        | 0        | 0        , PRV_S);

    printf("all test cases pass\n");

    g_free(cpu);

    return 0;
}
