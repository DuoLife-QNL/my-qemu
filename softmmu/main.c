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

// epmp documentation https://docs.google.com/document/d/1kmHBwR8soAP3hXRXVBLrWS85bl0rnEBNXpm7HcxIPS8/edit#

// test of behavior of cpu about mseccfg.RLB
// 1)         mseccfg = 0, it meets the requirements of pmp
// 2/epmp.2a) When mseccfg.RLB is set PMP rules with pmpcfg.L bit set can be removed and/or edited.
// 3/epmp.2b) When mseccfg.RLB is unset and at least one rule with pmpcfg.L is set, mseccfg.RLB is
//            locked and any further modifications to mseccfg.RLB are ignored (WARL).
// 4/epmp.4b) Adding a new M-mode-only or a Shared-Region rule with executable privileges is not
//            possible and such pmpcfg writes are ignored, leaving pmpcfg unchanged. This
//            restriction can be temporarily lifted e.g. during the boot process, by setting mseccfg.RLB.
int32_t pmp_rlb_test(CPURISCVState *env);

// test of behavior of cpu about mseccfg.MMWP
// 1/epmp.3)  This is a sticky bit, meaning that once set it cannot be unset until a hard reset.
int32_t pmp_mmwp_test(CPURISCVState *env);

// test of behavior of cpu about mseccfg.MML
// 1)         mseccfg = 0, it meets the requirements of pmp
// 2/epmp.4)  This is a sticky bit, meaning that once set it cannot be unset until a hard reset.
// 3/epmp.4b) Adding a new M-mode-only or a Shared-Region rule with executable privileges is not
//            possible and such pmpcfg writes are ignored, leaving pmpcfg unchanged. This
//            restriction can be temporarily lifted e.g. during the boot process, by setting mseccfg.RLB.
int32_t pmp_mml_test(CPURISCVState *env);

// test of pmp_hart_has_privs
// 1)         mseccfg = 0, it meets the requirements of pmp
// 2)         mseccfg = MML, it meets the requirements of truth table when mseccfg.MML is set
//            mentioned in epmp doc.
// 3/epmp.4c) mseccfg = MML, Executing code with Machine mode privileges is only possible from
//            memory regions with a matching M-mode-only rule or a Shared-Region rule with
//            executable privileges. Executing ((with Machine mode)) code from a region without a
//            matching rule or with a matching S/U-mode-only rule is denied.
// 4/epmp.3)  mseccfg = MMWP, it changes the default PMP policy for M-mode when accessing
//            memory regions that don’t have a matching PMP rule, to denied instead of ignored.
int32_t pmp_hart_has_privs_test(CPURISCVState *env);

int32_t aux_run_pmp_test(const char * test_prefix, int32_t *code, int32_t (*test_func)(CPURISCVState *env));

#define run_pmp_test(prefix) aux_run_pmp_test("pmp_" #prefix, &code, pmp_ ## prefix ## _test);

int main(int argc, char **argv, char **envp)
{
    int code = 0;

    run_pmp_test(rlb);
    run_pmp_test(mmwp);
    run_pmp_test(mml);
    run_pmp_test(hart_has_privs);

    if (!code) printf("all test cases pass\n");

    return code;
}

#undef run_pmp_test

int32_t aux_run_pmp_test(const char * test_prefix, int32_t *code, int32_t (*test_func)(CPURISCVState *env)) {
    if (*code) return *code;
    printf("================================================================================\n");
    printf("begin test %s\n", test_prefix);
    fflush(stdout);
    
    // the cpu is not realized, but we can use fields in initialized cpu
    RISCVCPU *cpu = g_new0(RISCVCPU, 1);
    CPURISCVState *env = &cpu->env;
    
    *code = test_func(env);

    g_free(cpu);

    printf("test %s %s\n", test_prefix, *code ? "failed" : "passed");
    printf("================================================================================\n");
    fflush(stdout);
        
    return *code;
}

int32_t pmp_rlb_test(CPURISCVState *env) {

    // if no rule is set, RLB bit can be set or unset
    assert(mseccfg_csr_read(env) == 0);
    mseccfg_csr_write(env, MSECCFG_RLB);
    assert(mseccfg_csr_read(env) == MSECCFG_RLB);
    mseccfg_csr_write(env, 0);
    assert(mseccfg_csr_read(env) == 0);
    
// 1)         mseccfg = 0, it meets the requirements of pmp

    assert(pmpcfg_csr_read(env, 0) == 0);
    pmpcfg_csr_write(env, 0, PMP_LOCK | (PMP_AMATCH_TOR << 3));
    assert(pmpcfg_csr_read(env, 0) == (PMP_LOCK | (PMP_AMATCH_TOR << 3)));
    pmpcfg_csr_write(env, 0, 0);
    // pmpcfg.L will be locked
    assert(pmpcfg_csr_read(env, 0) == (PMP_LOCK | (PMP_AMATCH_TOR << 3)));

// 3/epmp.2b) When mseccfg.RLB is unset and at least one rule with pmpcfg.L is set, mseccfg.RLB is
//            locked and any further modifications to mseccfg.RLB are ignored (WARL).
    
    assert(env->pmp_state.num_rules == 1);
    env->mseccfg = 0;
    assert(mseccfg_csr_read(env) == 0);
    mseccfg_csr_write(env, MSECCFG_RLB);
    assert(mseccfg_csr_read(env) == 0);
    
    assert(env->pmp_state.num_rules == 1);
    env->mseccfg = MSECCFG_RLB;
    assert(mseccfg_csr_read(env) == MSECCFG_RLB);
    mseccfg_csr_write(env, 0);
    assert(mseccfg_csr_read(env) == 0);

// 2/epmp.2a) When mseccfg.RLB is set PMP rules with pmpcfg.L bit set can be removed and/or edited.

    env->mseccfg = MSECCFG_RLB;

    pmpcfg_csr_write(env, 0, 0);
    assert(pmpcfg_csr_read(env, 0) == 0);
    pmpcfg_csr_write(env, 0, PMP_LOCK);
    assert(pmpcfg_csr_read(env, 0) == PMP_LOCK);
    pmpcfg_csr_write(env, 0, 0);
    assert(pmpcfg_csr_read(env, 0) == 0);

// 4/epmp.4b) Adding a new M-mode-only or a Shared-Region rule with executable privileges is not
//            possible and such pmpcfg writes are ignored, leaving pmpcfg unchanged. This
//            restriction can be temporarily lifted e.g. during the boot process, by setting mseccfg.RLB.

    // shared region with executable privileges
    pmpcfg_csr_write(env, 0, PMP_WRITE | PMP_EXEC);
    assert(pmpcfg_csr_read(env, 0) == (PMP_WRITE | PMP_EXEC));
    
    // m-mode-only with executable privileges
    pmpcfg_csr_write(env, 0, PMP_LOCK | PMP_EXEC);
    assert(pmpcfg_csr_read(env, 0) == (PMP_LOCK | PMP_EXEC));
    
    // m-mode-only with executable privileges
    pmpcfg_csr_write(env, 0, (PMP_LOCK | PMP_EXEC | PMP_WRITE));
    assert(pmpcfg_csr_read(env, 0) == (PMP_LOCK | PMP_EXEC | PMP_WRITE));
    
    return 0;
}

int32_t pmp_mmwp_test(CPURISCVState *env) {

// 1/epmp.3)  This is a sticky bit, meaning that once set it cannot be unset until a hard reset.
    assert(mseccfg_csr_read(env) == 0);
    mseccfg_csr_write(env, MSECCFG_MMWP);
    assert(mseccfg_csr_read(env) == MSECCFG_MMWP);
    mseccfg_csr_write(env, 0);
    assert(mseccfg_csr_read(env) == MSECCFG_MMWP);
    
    return 0;
}

void reset_pmpcfg(CPURISCVState *env, int idx);
void reset_pmpcfg(CPURISCVState *env, int idx) {
    int tmp = env->mseccfg;
    env->mseccfg = MSECCFG_RLB;
    assert(mseccfg_csr_read(env) == MSECCFG_RLB);
    pmpcfg_csr_write(env, idx, 0);
    assert(pmpcfg_csr_read(env, idx) == 0);
    env->mseccfg = tmp;
}

int32_t pmp_mml_test(CPURISCVState *env) {

// 1)         mseccfg = 0, it meets the requirements of pmp

    assert(pmpcfg_csr_read(env, 0) == 0);
    pmpcfg_csr_write(env, 0, PMP_LOCK | (PMP_AMATCH_TOR << 3));
    assert(pmpcfg_csr_read(env, 0) == (PMP_LOCK | (PMP_AMATCH_TOR << 3)));
    pmpcfg_csr_write(env, 0, 0);
    // pmpcfg.L will be locked
    assert(pmpcfg_csr_read(env, 0) == (PMP_LOCK | (PMP_AMATCH_TOR << 3)));

    reset_pmpcfg(env, 0);

// 2/epmp.4)  This is a sticky bit, meaning that once set it cannot be unset until a hard reset.
    env->mseccfg = 0;
    assert(mseccfg_csr_read(env) == 0);
    mseccfg_csr_write(env, MSECCFG_MML);
    assert(mseccfg_csr_read(env) == MSECCFG_MML);
    mseccfg_csr_write(env, 0);
    assert(mseccfg_csr_read(env) == MSECCFG_MML);

// 3/epmp.4b) Adding a new M-mode-only or a Shared-Region rule with executable privileges is not
//            possible and such pmpcfg writes are ignored, leaving pmpcfg unchanged. This
//            restriction can be temporarily lifted e.g. during the boot process, by setting mseccfg.RLB.

    reset_pmpcfg(env, 0);
    assert(pmpcfg_csr_read(env, 0) == 0);

    // shared region with executable privileges
    assert(mseccfg_csr_read(env) == MSECCFG_MML);
    pmpcfg_csr_write(env, 0, PMP_WRITE | PMP_EXEC);
    assert(pmpcfg_csr_read(env, 0) == 0);

    reset_pmpcfg(env, 0);
    assert(pmpcfg_csr_read(env, 0) == 0);
    
    // m-mode-only with executable privileges
    assert(mseccfg_csr_read(env) == MSECCFG_MML);
    pmpcfg_csr_write(env, 0, PMP_LOCK | PMP_EXEC);
    assert(pmpcfg_csr_read(env, 0) == 0);

    reset_pmpcfg(env, 0);
    assert(pmpcfg_csr_read(env, 0) == 0);
    
    // m-mode-only with executable privileges
    assert(mseccfg_csr_read(env) == MSECCFG_MML);
    pmpcfg_csr_write(env, 0, (PMP_LOCK | PMP_EXEC | PMP_WRITE));
    assert(pmpcfg_csr_read(env, 0) == 0);

    return 0;
}

// set pmpcfg[idx] value to val
void pmpcfg_set_value(CPURISCVState *env, int idx, uint64_t val);

// assert when accessing address between range [pmpaddr[idx-1], pmpaddr[idx]],
// pmp_hart_has_privs should exact allow a subset privilege of priv under mode (m/s/u mode)
void assert_exact(CPURISCVState *env, int idx, uint8_t priv, uint64_t mode);

// assertion helper
void pmp_hart_has_privs_assert_rules_without_mml(CPURISCVState *env);
void pmp_hart_has_privs_assert_rules_with_mml(CPURISCVState *env);

// the value of testing_addr[idx] should be in the range [pmpaddr[idx-1], pmpaddr[idx])
uint64_t testing_addr[18];

// print help info to console and invoke function assert_exact
#define assert_exact_test(idx, priv, mode) printf("calling assert_exact(%2d, %d, %llu): ", idx, priv, (long long unsigned int)(mode));\
    assert_exact(env, idx, priv, mode)

int32_t pmp_hart_has_privs_test(CPURISCVState *env) {
    
    assert(env->pmp_state.num_rules == 0);

    int32_t pmp_idx = 0;

    for (int i = 0; i < 16; i++){
        pmpaddr_csr_write(env, pmp_idx++, (0x80200000 + (0x00200000 + 0x00100000 * i)) >> 2);   
        testing_addr[i] = (0x80200000 + (0x00200000 + 0x00100000 * i)) - 1; 
    }
    pmp_idx = 0;
    
    env->mseccfg = 0;
    printf("env->mseccfg = 0; no rule set\n");
    assert_exact_test(11, PMP_READ | PMP_EXEC | PMP_WRITE, PRV_M);
    assert_exact_test(11, PMP_READ | PMP_EXEC | PMP_WRITE, PRV_S);

    env->mseccfg = MSECCFG_MML;
    printf("env->mseccfg = MSECCFG_MML; no rule set\n");
    assert_exact_test(11, PMP_READ | 0        | PMP_WRITE, PRV_M);
    assert_exact_test(11, 0        | 0        | 0        , PRV_S);


    env->mseccfg = MSECCFG_MMWP;
    printf("env->mseccfg = MSECCFG_MMWP; no rule set\n");
    assert_exact_test(11, 0        | 0        | 0        , PRV_M);
    assert_exact_test(11, 0        | 0        | 0        , PRV_S);


    env->mseccfg = MSECCFG_MML | MSECCFG_MMWP;
    printf("env->mseccfg = MSECCFG_MML | MSECCFG_MMWP; no rule set\n");
    assert_exact_test(11, 0        | 0        | 0        , PRV_M);
    assert_exact_test(11, 0        | 0        | 0        , PRV_S);

    // reset mseccfg to ensure all rules can be set
    env->mseccfg = 0;

    printf("rules setting, please check code between line %d", __LINE__);
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

    printf(" and %d\n", __LINE__);
    
    assert(env->pmp_state.num_rules == 10);

    env->mseccfg = 0;
    printf("env->mseccfg = 0; 10 rule set\n");
    pmp_hart_has_privs_assert_rules_without_mml(env);
    // not rule matched but there are rules set
    assert_exact_test(11, PMP_READ | PMP_EXEC | PMP_WRITE, PRV_M);
    assert_exact_test(11, 0        | 0        | 0        , PRV_S);

    env->mseccfg = MSECCFG_MML;
    printf("env->mseccfg = MSECCFG_MML; 10 rule set\n");
    pmp_hart_has_privs_assert_rules_with_mml(env);
    // not rule matched but there are rules set
    assert_exact_test(11, PMP_READ | 0        | PMP_WRITE, PRV_M);
    assert_exact_test(11, 0        | 0        | 0        , PRV_S);

    env->mseccfg = MSECCFG_MMWP;
    printf("env->mseccfg = MSECCFG_MMWP; 10 rule set\n");
    pmp_hart_has_privs_assert_rules_without_mml(env);
    // not rule matched but there are rules set
    assert_exact_test(11, 0        | 0        | 0        , PRV_M);
    assert_exact_test(11, 0        | 0        | 0        , PRV_S);

    env->mseccfg = MSECCFG_MML | MSECCFG_MMWP;
    printf("env->mseccfg = MSECCFG_MML | MSECCFG_MMWP; 10 rule set\n");
    pmp_hart_has_privs_assert_rules_with_mml(env);
    // not rule matched but there are rules set
    assert_exact_test(11, 0        | 0        | 0        , PRV_M);
    assert_exact_test(11, 0        | 0        | 0        , PRV_S);

    return 0;
}

void pmp_hart_has_privs_assert_rules_without_mml(CPURISCVState *env) {
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
}

void pmp_hart_has_privs_assert_rules_with_mml(CPURISCVState *env) {
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
}

void pmpcfg_set_value(CPURISCVState *env, int idx, uint64_t val) {
    uint64_t raw_val = pmpcfg_csr_read(env, (idx >> 3) << 1);

    int xidx = idx & 7;
    raw_val &= ~(0xffULL << (xidx << 3));
    raw_val |= (val << (xidx << 3));
    pmpcfg_csr_write(env, (idx >> 3) << 1, raw_val);
}

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
