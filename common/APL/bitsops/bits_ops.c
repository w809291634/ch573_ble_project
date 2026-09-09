/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2011-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file bits_ops.c — bits_ops.h 的非 builtin 路径实现
 *
 * @brief 设计原则:**不使用临界区**
 *
 * 这里的 `//__disable_irq();` / `//__enable_irq();` 全部是**注释**,
 * 故意不启用,目的:
 *   1. 不依赖临界区(避免与 `common/UTILS/atomic/` 耦合)
 *   2. 假设调用方在单线程 / 无中断竞争场景使用
 *   3. 真正的原子性由调用方负责(常见做法:在状态机主循环 / 任务里调用)
 *
 * **如需真正跨上下文原子性**,应改用 `common/UTILS/atomic/atomic_c.c`
 * 中基于临界区的实现,或配置工具链让 builtin 路径生效。
 */

/**
 * @file Atomic ops in pure C
 *
 * This module provides the atomic operators for processors
 * which do not support native atomic operations.
 *
 * The atomic operations are guaranteed to be atomic with respect
 * to interrupt service routines, and to operations performed by peer
 * processors.
 *
 * (originally from x86's atomic.c)
 */

#include "bits_ops.h"

/**
 *
 * @brief Atomic compare-and-set primitive
 *
 * This routine provides the compare-and-set operator. If the original value at
 * <target> equals <oldValue>, then <newValue> is stored at <target> and the
 * function returns 1.
 *
 * If the original value at <target> does not equal <oldValue>, then the store
 * is not done and the function returns 0.
 *
 * The reading of the original value at <target>, the comparison,
 * and the write of the new value (if it occurs) all happen atomically with
 * respect to both interrupts and accesses of other processors to <target>.
 *
 * @param target address to be tested
 * @param old_value value to compare against
 * @param new_value value to compare against
 * @return Returns 1 if <new_value> is written, 0 otherwise.
 */
int bitsop_cas(bitsop_t *target, bitsop_val_t old_value,
               bitsop_val_t new_value)
{
    int ret = 0;

    //__disable_irq();

    if (*target == old_value) {
        *target = new_value;
        ret = 1;
    }

    //__enable_irq();

    return ret;
}

/**
 *
 * @brief Atomic addition primitive
 *
 * This routine provides the atomic addition operator. The <value> is
 * atomically added to the value at <target>, placing the result at <target>,
 * and the old value from <target> is returned.
 *
 * @param target memory location to add to
 * @param value the value to add
 *
 * @return The previous value from <target>
 */
bitsop_val_t bitsop_add(bitsop_t *target, bitsop_val_t value)
{
    bitsop_val_t ret;

    //__disable_irq();

    ret = *target;
    *target += value;

    //__enable_irq();

    return ret;
}

/**
 *
 * @brief Atomic subtraction primitive
 *
 * This routine provides the atomic subtraction operator. The <value> is
 * atomically subtracted from the value at <target>, placing the result at
 * <target>, and the old value from <target> is returned.
 *
 * @param target the memory location to subtract from
 * @param value the value to subtract
 *
 * @return The previous value from <target>
 */
bitsop_val_t bitsop_sub(bitsop_t *target, bitsop_val_t value)
{
    bitsop_val_t ret;

    //__disable_irq();

    ret = *target;
    *target -= value;

    //__enable_irq();

    return ret;
}

/**
 *
 * @brief Atomic increment primitive
 *
 * @param target memory location to increment
 *
 * This routine provides the atomic increment operator. The value at <target>
 * is atomically incremented by 1, and the old value from <target> is returned.
 *
 * @return The value from <target> before the increment
 */
bitsop_val_t bitsop_inc(bitsop_t *target)
{
    bitsop_val_t ret;

    //__disable_irq();

    ret = *target;
    (*target)++;

    //__enable_irq();

    return ret;
}

/**
 *
 * @brief Atomic decrement primitive
 *
 * @param target memory location to decrement
 *
 * This routine provides the atomic decrement operator. The value at <target>
 * is atomically decremented by 1, and the old value from <target> is returned.
 *
 * @return The value from <target> prior to the decrement
 */
bitsop_val_t bitsop_dec(bitsop_t *target)
{
    bitsop_val_t ret;

    //__disable_irq();

    ret = *target;
    (*target)--;

    //__enable_irq();

    return ret;
}

/**
 *
 * @brief Atomic get primitive
 *
 * @param target memory location to read from
 *
 * This routine provides the atomic get primitive to atomically read
 * a value from <target>. It simply does an ordinary load.  Note that <target>
 * is expected to be aligned to a 4-byte boundary.
 *
 * @return The value read from <target>
 */
bitsop_val_t bitsop_get(const bitsop_t *target)
{
    return *target;
}

/**
 *
 * @brief Atomic get-and-set primitive
 *
 * This routine provides the atomic set operator. The <value> is atomically
 * written at <target> and the previous value at <target> is returned.
 *
 * @param target the memory location to write to
 * @param value the value to write
 *
 * @return The previous value from <target>
 */
bitsop_val_t bitsop_set(bitsop_t *target, bitsop_val_t value)
{
    bitsop_val_t ret;

    //__disable_irq();

    ret = *target;
    *target = value;

    //__enable_irq();

    return ret;
}

/**
 *
 * @brief Atomic clear primitive
 *
 * This routine provides the atomic clear operator. The value of 0 is atomically
 * written at <target> and the previous value at <target> is returned. (Hence,
 * bitsop_clear(pAtomicVar) is equivalent to bitsop_set(pAtomicVar, 0).)
 *
 * @param target the memory location to write
 *
 * @return The previous value from <target>
 */
bitsop_val_t bitsop_clear(bitsop_t *target)
{
    bitsop_val_t ret;

    //__disable_irq();

    ret = *target;
    *target = 0;

    //__enable_irq();

    return ret;
}

/**
 *
 * @brief Atomic bitwise inclusive OR primitive
 *
 * This routine provides the atomic bitwise inclusive OR operator. The <value>
 * is atomically bitwise OR'ed with the value at <target>, placing the result
 * at <target>, and the previous value at <target> is returned.
 *
 * @param target the memory location to be modified
 * @param value the value to OR
 *
 * @return The previous value from <target>
 */
bitsop_val_t bitsop_or(bitsop_t *target, bitsop_val_t value)
{
    bitsop_val_t ret;

    //__disable_irq();

    ret = *target;
    *target |= value;

    //__enable_irq();

    return ret;
}

/**
 *
 * @brief Atomic bitwise exclusive OR (XOR) primitive
 *
 * This routine provides the atomic bitwise exclusive OR operator. The <value>
 * is atomically bitwise XOR'ed with the value at <target>, placing the result
 * at <target>, and the previous value at <target> is returned.
 *
 * @param target the memory location to be modified
 * @param value the value to XOR
 *
 * @return The previous value from <target>
 */
bitsop_val_t bitsop_xor(bitsop_t *target, bitsop_val_t value)
{
    bitsop_val_t ret;

    //__disable_irq();

    ret = *target;
    *target ^= value;

    //__enable_irq();

    return ret;
}

/**
 *
 * @brief Atomic bitwise AND primitive
 *
 * This routine provides the atomic bitwise AND operator. The <value> is
 * atomically bitwise AND'ed with the value at <target>, placing the result
 * at <target>, and the previous value at <target> is returned.
 *
 * @param target the memory location to be modified
 * @param value the value to AND
 *
 * @return The previous value from <target>
 */
bitsop_val_t bitsop_and(bitsop_t *target, bitsop_val_t value)
{
    bitsop_val_t ret;

    //__disable_irq();

    ret = *target;
    *target &= value;

    //__enable_irq();

    return ret;
}

/**
 *
 * @brief Atomic bitwise NAND primitive
 *
 * This routine provides the atomic bitwise NAND operator. The <value> is
 * atomically bitwise NAND'ed with the value at <target>, placing the result
 * at <target>, and the previous value at <target> is returned.
 *
 * @param target the memory location to be modified
 * @param value the value to NAND
 *
 * @return The previous value from <target>
 */
bitsop_val_t bitsop_nand(bitsop_t *target, bitsop_val_t value)
{
    bitsop_val_t ret;

    //__disable_irq();

    ret = *target;
    *target = ~(*target & value);

    //__enable_irq();

    return ret;
}
