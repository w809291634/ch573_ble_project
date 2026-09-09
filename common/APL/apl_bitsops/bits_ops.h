/* atomic operations */

/*
 * Copyright (c) 1997-2015, Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file bits_ops.h — 无临界区的位掩码原子操作
 *
 * @brief 设计原则:**不使用临界区(critical section)**
 *
 * 本模块的原子性保证**不依赖临界区**。三种场景的原子性来源:
 *   1. **builtin 路径**(默认,GCC/Clang):由 `__atomic_*` 编译器内建提供,完全无锁
 *   2. **fallback 路径**(无 builtin 工具链):靠 `//__disable_irq();` 注释位置提示使用者自己加锁
 *   3. **应用层职责**:调用方必须保证单线程 / 非中断上下文
 *
 * **如需真正跨中断/任务的临界区保护**,应使用 `common/UTILS/atomic/` 提供的 `atomic_*` 函数
 * (其回退路径依赖工程自带的临界区接口,GCC/Clang 构建用 `__atomic` 内建)。
 *
 * 当前用例:`apl_inputex` 内部用 `bitsop_*` 维护组 mask 与对象 status,
 * 调用上下文是单一线程的 `Input_Scan`,无中断竞争,因此不需临界区。
 */

#ifndef __BITSOP_H__
#define __BITSOP_H__
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t bitsop_t;
typedef bitsop_t bitsop_val_t;

/**
 * @defgroup bitsop_apis Atomic Services APIs
 * @ingroup kernel_apis
 * @{
 */

/**
 * @brief Atomic compare-and-set.
 *
 * This routine performs an atomic compare-and-set on @a target. If the current
 * value of @a target equals @a old_value, @a target is set to @a new_value.
 * If the current value of @a target does not equal @a old_value, @a target
 * is left unchanged.
 *
 * @param target Address of atomic variable.
 * @param old_value Original value to compare against.
 * @param new_value New value to store.
 * @return 1 if @a new_value is written, 0 otherwise.
 */
extern int bitsop_cas(bitsop_t *target, bitsop_val_t old_value,
		      bitsop_val_t new_value);

/**
 *
 * @brief Atomic addition.
 *
 * This routine performs an atomic addition on @a target.
 *
 * @param target Address of atomic variable.
 * @param value Value to add.
 *
 * @return Previous value of @a target.
 */
extern bitsop_val_t bitsop_add(bitsop_t *target, bitsop_val_t value);

/**
 *
 * @brief Atomic subtraction.
 *
 * This routine performs an atomic subtraction on @a target.
 *
 * @param target Address of atomic variable.
 * @param value Value to subtract.
 *
 * @return Previous value of @a target.
 */
extern bitsop_val_t bitsop_sub(bitsop_t *target, bitsop_val_t value);

/**
 *
 * @brief Atomic increment.
 *
 * This routine performs an atomic increment by 1 on @a target.
 *
 * @param target Address of atomic variable.
 *
 * @return Previous value of @a target.
 */
extern bitsop_val_t bitsop_inc(bitsop_t *target);

/**
 *
 * @brief Atomic decrement.
 *
 * This routine performs an atomic decrement by 1 on @a target.
 *
 * @param target Address of atomic variable.
 *
 * @return Previous value of @a target.
 */
extern bitsop_val_t bitsop_dec(bitsop_t *target);

/**
 *
 * @brief Atomic get.
 *
 * This routine performs an atomic read on @a target.
 *
 * @param target Address of atomic variable.
 *
 * @return Value of @a target.
 */
extern bitsop_val_t bitsop_get(const bitsop_t *target);

/**
 *
 * @brief Atomic get-and-set.
 *
 * This routine atomically sets @a target to @a value and returns
 * the previous value of @a target.
 *
 * @param target Address of atomic variable.
 * @param value Value to write to @a target.
 *
 * @return Previous value of @a target.
 */
extern bitsop_val_t bitsop_set(bitsop_t *target, bitsop_val_t value);

/**
 *
 * @brief Atomic clear.
 *
 * This routine atomically sets @a target to zero and returns its previous
 * value. (Hence, it is equivalent to bitsop_set(target, 0).)
 *
 * @param target Address of atomic variable.
 *
 * @return Previous value of @a target.
 */
extern bitsop_val_t bitsop_clear(bitsop_t *target);

/**
 *
 * @brief Atomic bitwise inclusive OR.
 *
 * This routine atomically sets @a target to the bitwise inclusive OR of
 * @a target and @a value.
 *
 * @param target Address of atomic variable.
 * @param value Value to OR.
 *
 * @return Previous value of @a target.
 */
extern bitsop_val_t bitsop_or(bitsop_t *target, bitsop_val_t value);

/**
 *
 * @brief Atomic bitwise exclusive OR (XOR).
 *
 * This routine atomically sets @a target to the bitwise exclusive OR (XOR) of
 * @a target and @a value.
 *
 * @param target Address of atomic variable.
 * @param value Value to XOR
 *
 * @return Previous value of @a target.
 */
extern bitsop_val_t bitsop_xor(bitsop_t *target, bitsop_val_t value);

/**
 *
 * @brief Atomic bitwise AND.
 *
 * This routine atomically sets @a target to the bitwise AND of @a target
 * and @a value.
 *
 * @param target Address of atomic variable.
 * @param value Value to AND.
 *
 * @return Previous value of @a target.
 */
extern bitsop_val_t bitsop_and(bitsop_t *target, bitsop_val_t value);

/**
 *
 * @brief Atomic bitwise NAND.
 *
 * This routine atomically sets @a target to the bitwise NAND of @a target
 * and @a value. (This operation is equivalent to target = ~(target & value).)
 *
 * @param target Address of atomic variable.
 * @param value Value to NAND.
 *
 * @return Previous value of @a target.
 */
extern bitsop_val_t bitsop_nand(bitsop_t *target, bitsop_val_t value);


/**
 * @brief Initialize an atomic variable.
 *
 * This macro can be used to initialize an atomic variable. For example,
 * @code bitsop_t my_var = BITSOP_INIT(75); @endcode
 *
 * @param i Value to assign to atomic variable.
 */
#define BITSOP_INIT(i) (i)

/**
 * @cond INTERNAL_HIDDEN
 */

#define BITSOP_BITS (sizeof(bitsop_val_t) * 8)
#define BITSOP_MASK(bit) (1 << ((bit) & (BITSOP_BITS - 1)))
#define BITSOP_ELEM(addr, bit) ((addr) + ((bit) / BITSOP_BITS))

/**
 * INTERNAL_HIDDEN @endcond
 */

/**
 * @brief Define an array of atomic variables.
 *
 * This macro defines an array of atomic variables containing at least
 * @a num_bits bits.
 *
 * @note
 * If used from file scope, the bits of the array are initialized to zero;
 * if used from within a function, the bits are left uninitialized.
 *
 * @param name Name of array of atomic variables.
 * @param num_bits Number of bits needed.
 */
#define BITSOP_DEFINE(name, num_bits) \
	bitsop_t name[1 + ((num_bits) - 1) / BITSOP_BITS]

/**
 * @brief Atomically test a bit.
 *
 * This routine tests whether bit number @a bit of @a target is set or not.
 * The target may be a single atomic variable or an array of them.
 *
 * @param target Address of atomic variable or array.
 * @param bit Bit number (starting from 0).
 *
 * @return 1 if the bit was set, 0 if it wasn't.
 */
static inline int bitsop_test_bit(const bitsop_t *target, int bit)
{
	bitsop_val_t val = bitsop_get(BITSOP_ELEM(target, bit));

	return (1 & (val >> (bit & (BITSOP_BITS - 1))));
}

/**
 * @brief Atomically test and clear a bit.
 *
 * Atomically clear bit number @a bit of @a target and return its old value.
 * The target may be a single atomic variable or an array of them.
 *
 * @param target Address of atomic variable or array.
 * @param bit Bit number (starting from 0).
 *
 * @return 1 if the bit was set, 0 if it wasn't.
 */
static inline int bitsop_test_and_clear_bit(bitsop_t *target, int bit)
{
	bitsop_val_t mask = BITSOP_MASK(bit);
	bitsop_val_t old;

	old = bitsop_and(BITSOP_ELEM(target, bit), ~mask);

	return (old & mask) != 0;
}

/**
 * @brief Atomically set a bit.
 *
 * Atomically set bit number @a bit of @a target and return its old value.
 * The target may be a single atomic variable or an array of them.
 *
 * @param target Address of atomic variable or array.
 * @param bit Bit number (starting from 0).
 *
 * @return 1 if the bit was set, 0 if it wasn't.
 */
static inline int bitsop_test_and_set_bit(bitsop_t *target, int bit)
{
	bitsop_val_t mask = BITSOP_MASK(bit);
	bitsop_val_t old;

	old = bitsop_or(BITSOP_ELEM(target, bit), mask);

	return (old & mask) != 0;
}

/**
 * @brief Atomically clear a bit.
 *
 * Atomically clear bit number @a bit of @a target.
 * The target may be a single atomic variable or an array of them.
 *
 * @param target Address of atomic variable or array.
 * @param bit Bit number (starting from 0).
 *
 * @return N/A
 */
static inline void bitsop_clear_bit(bitsop_t *target, int bit)
{
	bitsop_val_t mask = BITSOP_MASK(bit);

	bitsop_and(BITSOP_ELEM(target, bit), ~mask);
}

/**
 * @brief Atomically set a bit.
 *
 * Atomically set bit number @a bit of @a target.
 * The target may be a single atomic variable or an array of them.
 *
 * @param target Address of atomic variable or array.
 * @param bit Bit number (starting from 0).
 *
 * @return N/A
 */
static inline void bitsop_set_bit(bitsop_t *target, int bit)
{
	bitsop_val_t mask = BITSOP_MASK(bit);

	bitsop_or(BITSOP_ELEM(target, bit), mask);
}

static inline void bitsop_set_bit_to(bitsop_t *target, int bit, bool val)
{
	bitsop_val_t mask = BITSOP_MASK(bit);

	bitsop_or(BITSOP_ELEM(target, bit), mask);

    if (val)
    {
        bitsop_or(BITSOP_ELEM(target, bit), mask);
    }
    else
    {
        bitsop_and(BITSOP_ELEM(target, bit), ~mask);
    }
}

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __BITSOP_H__ */
