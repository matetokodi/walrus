/*
 * Copyright (c) 2022-present Samsung Electronics Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Only included by jit-backend.cc */

struct MemAddress {
    void check(sljit_compiler* compiler, Operand* params, sljit_u32 offset, sljit_s32 size);

    JITArg memArg;
    sljit_s32 offsetReg;
    bool addRequired;
};

void MemAddress::check(sljit_compiler* compiler, Operand* params, sljit_u32 offset, sljit_s32 size)
{
    CompileContext* context = CompileContext::get(compiler);
    JITArg offsetArg;

    operandToArg(params, offsetArg);

    if (SLJIT_IS_IMM(offsetArg.arg)) {
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
        sljit_sw totalOffset = static_cast<sljit_sw>(offset) + static_cast<sljit_sw>(static_cast<sljit_u32>(offsetArg.argw)) + size;
#else /* !SLJIT_64BIT_ARCHITECTURE */
        sljit_u32 totalOffset = static_cast<sljit_u32>(offsetArg.argw);

        if (totalOffset + offset < (totalOffset | offset)) {
            sljit_set_label(sljit_emit_jump(compiler, SLJIT_JUMP), context->memoryTrapLabel);
            memArg.arg = 0;
            return;
        }

        totalOffset += offset;

        if (totalOffset > UINT32_MAX - size) {
            sljit_set_label(sljit_emit_jump(compiler, SLJIT_JUMP), context->memoryTrapLabel);
            memArg.arg = 0;
            return;
        }

        totalOffset += size;
#endif /* SLJIT_64BIT_ARCHITECTURE */

        sljit_emit_op1(compiler, SLJIT_MOV_P, SLJIT_R0, 0, SLJIT_MEM1(kContextReg), OffsetOfContextField(memory0));
        sljit_emit_op1(compiler, SLJIT_MOV_U32, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R0), JITFieldAccessor::memorySizeInByteOffset());

        if (totalOffset > 255) {
            sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, static_cast<sljit_sw>(totalOffset));
        }

        sljit_emit_op1(compiler, SLJIT_MOV_P, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R0), JITFieldAccessor::memoryBufferOffset());

        if (totalOffset <= 255) {
            sljit_set_label(sljit_emit_cmp(compiler, SLJIT_LESS, SLJIT_R1, 0, SLJIT_IMM, static_cast<sljit_sw>(totalOffset)), context->memoryTrapLabel);
        } else {
            sljit_set_label(sljit_emit_cmp(compiler, SLJIT_LESS, SLJIT_R1, 0, SLJIT_R2, 0), context->memoryTrapLabel);
        }

        if (totalOffset <= 255) {
            memArg.arg = SLJIT_MEM1(SLJIT_R0);
            memArg.argw = totalOffset - size;
            offsetReg = 0;
        } else {
            memArg.arg = SLJIT_MEM1(SLJIT_R0);
            memArg.argw = -size;
            offsetReg = SLJIT_R2;
        }
        return;
    }

#if (defined SLJIT_32BIT_ARCHITECTURE && SLJIT_32BIT_ARCHITECTURE)
    if (offset >= UINT32_MAX - size) {
        // This memory load is never successful.
        sljit_set_label(sljit_emit_jump(compiler, SLJIT_JUMP), context->memoryTrapLabel);
        memArg.arg = 0;
        return;
    }
#endif /* SLJIT_32BIT_ARCHITECTURE */

    offsetReg = GET_SOURCE_REG(offsetArg.arg, SLJIT_R2);

    sljit_emit_op1(compiler, SLJIT_MOV_P, SLJIT_R0, 0, SLJIT_MEM1(kContextReg), OffsetOfContextField(memory0));

    MOVE_TO_REG(compiler, SLJIT_MOV_U32, offsetReg, offsetArg.arg, offsetArg.argw);
    sljit_emit_op1(compiler, SLJIT_MOV_U32, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R0), JITFieldAccessor::memorySizeInByteOffset());
    sljit_emit_op1(compiler, SLJIT_MOV_P, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R0), JITFieldAccessor::memoryBufferOffset());

#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
    sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R2, 0, offsetReg, 0, SLJIT_IMM, static_cast<sljit_sw>(offset) + size);
#else /* !SLJIT_64BIT_ARCHITECTURE */
    sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_SET_CARRY, SLJIT_R2, 0, offsetReg, 0, SLJIT_IMM, static_cast<sljit_sw>(offset) + size);
    sljit_set_label(sljit_emit_jump(compiler, SLJIT_CARRY), context->memoryTrapLabel);
#endif /* SLJIT_64BIT_ARCHITECTURE */

    sljit_set_label(sljit_emit_cmp(compiler, SLJIT_LESS, SLJIT_R1, 0, SLJIT_R2, 0), context->memoryTrapLabel);

    memArg.arg = SLJIT_MEM1(SLJIT_R0);
    memArg.argw = -size;
    offsetReg = SLJIT_R2;
}

static void emitLoad(sljit_compiler* compiler, Instruction* instr)
{
    sljit_s32 opcode;
    sljit_s32 size;
    sljit_u32 offset = 0;

    switch (instr->opcode()) {
    case Load32Opcode:
        opcode = SLJIT_MOV32;
        size = 4;
        break;
    case Load64Opcode:
        opcode = SLJIT_MOV;
        size = 8;
        break;
    case I32LoadOpcode:
        opcode = SLJIT_MOV32;
        size = 4;
        break;
    case I32Load8SOpcode:
        opcode = SLJIT_MOV32_S8;
        size = 1;
        break;
    case I32Load8UOpcode:
        opcode = SLJIT_MOV32_U8;
        size = 1;
        break;
    case I32Load16SOpcode:
        opcode = SLJIT_MOV32_S16;
        size = 2;
        break;
    case I32Load16UOpcode:
        opcode = SLJIT_MOV32_U16;
        size = 2;
        break;
    case I64LoadOpcode:
        opcode = SLJIT_MOV;
        size = 8;
        break;
    case I64Load8SOpcode:
        opcode = SLJIT_MOV_S8;
        size = 1;
        break;
    case I64Load8UOpcode:
        opcode = SLJIT_MOV_U8;
        size = 1;
        break;
    case I64Load16SOpcode:
        opcode = SLJIT_MOV_S16;
        size = 2;
        break;
    case I64Load16UOpcode:
        opcode = SLJIT_MOV_U16;
        size = 2;
        break;
    case I64Load32SOpcode:
        opcode = SLJIT_MOV_S32;
        size = 4;
        break;
    case I64Load32UOpcode:
        opcode = SLJIT_MOV_U32;
        size = 4;
        break;
    case F32LoadOpcode:
        opcode = SLJIT_MOV_F32;
        size = 4;
        break;
    default:
        ASSERT(instr->opcode() == F64LoadOpcode);
        opcode = SLJIT_MOV_F64;
        size = 8;
        break;
    }

    if (instr->opcode() != Load32Opcode && instr->opcode() != Load64Opcode) {
        MemoryLoad* loadOperation = reinterpret_cast<MemoryLoad*>(instr->byteCode());
        offset = loadOperation->offset();
    }

    Operand* operands = instr->operands();
    MemAddress addr;

    addr.check(compiler, operands, offset, size);

#if (defined SLJIT_32BIT_ARCHITECTURE && SLJIT_32BIT_ARCHITECTURE)
    if (addr.memArg.arg == 0) {
        return;
    }
#endif /* SLJIT_32BIT_ARCHITECTURE */

    if (addr.offsetReg != 0) {
        sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, addr.offsetReg, 0);
    }

    if (opcode == SLJIT_MOV_F64 || opcode == SLJIT_MOV_F32) {
        JITArg valueArg;
        floatOperandToArg(compiler, operands + 1, valueArg, SLJIT_FR0);

        // TODO: sljit_emit_fmem for unaligned access
        sljit_emit_fop1(compiler, opcode, valueArg.arg, valueArg.argw, addr.memArg.arg, addr.memArg.argw);
        return;
    }

#if (defined SLJIT_32BIT_ARCHITECTURE && SLJIT_32BIT_ARCHITECTURE)
    if (!(opcode & SLJIT_32)) {
        JITArgPair valueArgPair;
        operandToArgPair(operands + 1, valueArgPair);

        sljit_s32 dstReg1 = GET_TARGET_REG(valueArgPair.arg1, SLJIT_R0);

        if (opcode == SLJIT_MOV) {
            sljit_s32 dstReg2 = GET_TARGET_REG(valueArgPair.arg2, SLJIT_R1);

            sljit_emit_mem(compiler, opcode | SLJIT_MEM_LOAD, SLJIT_REG_PAIR(dstReg1, dstReg2), addr.memArg.arg, addr.memArg.argw);
            if (SLJIT_IS_MEM(valueArgPair.arg1)) {
#if (defined SLJIT_BIG_ENDIAN && SLJIT_BIG_ENDIAN)
                sljit_emit_mem(compiler, opcode | SLJIT_MEM_STORE, SLJIT_REG_PAIR(dstReg1, dstReg2), valueArgPair.arg2, valueArgPair.arg2w);
#else /* !SLJIT_BIG_ENDIAN */
                sljit_emit_mem(compiler, opcode | SLJIT_MEM_STORE, SLJIT_REG_PAIR(dstReg1, dstReg2), valueArgPair.arg1, valueArgPair.arg1w);
#endif /* SLJIT_BIG_ENDIAN */
            }
            return;
        }

        SLJIT_ASSERT(size <= 4);
        // TODO: sljit_emit_mem for unaligned access
        sljit_emit_op1(compiler, opcode, dstReg1, 0, addr.memArg.arg, addr.memArg.argw);

        if (opcode == SLJIT_MOV_S8 || opcode == SLJIT_MOV_S16 || opcode == SLJIT_MOV_S32) {
            sljit_emit_op2(compiler, SLJIT_ASHR, valueArgPair.arg2, valueArgPair.arg2w, dstReg1, 0, SLJIT_IMM, 31);
        } else {
            sljit_emit_op1(compiler, SLJIT_MOV, valueArgPair.arg2, valueArgPair.arg2w, SLJIT_IMM, 0);
        }

        MOVE_FROM_REG(compiler, SLJIT_MOV, valueArgPair.arg1, valueArgPair.arg1w, dstReg1);
        return;
    }
#endif /* SLJIT_32BIT_ARCHITECTURE */

    JITArg valueArg;
    operandToArg(operands + 1, valueArg);

    sljit_s32 dstReg = GET_TARGET_REG(valueArg.arg, SLJIT_R0);

    // TODO: sljit_emit_mem for unaligned access
    sljit_emit_op1(compiler, opcode, dstReg, 0, addr.memArg.arg, addr.memArg.argw);

    if (!SLJIT_IS_MEM(valueArg.arg)) {
        return;
    }

#if (defined SLJIT_32BIT_ARCHITECTURE && SLJIT_32BIT_ARCHITECTURE)
    opcode = SLJIT_MOV;
#else /* !SLJIT_32BIT_ARCHITECTURE */
    opcode = (opcode == SLJIT_MOV32 || (opcode & SLJIT_32)) ? SLJIT_MOV32 : SLJIT_MOV;
#endif /* SLJIT_32BIT_ARCHITECTURE */

    sljit_emit_op1(compiler, opcode, valueArg.arg, valueArg.argw, dstReg, 0);
}

static void emitStore(sljit_compiler* compiler, Instruction* instr)
{
    sljit_s32 opcode;
    sljit_s32 size;
    sljit_u32 offset = 0;

    switch (instr->opcode()) {
    case Store32Opcode:
        opcode = SLJIT_MOV32;
        size = 4;
        break;
    case Store64Opcode:
        opcode = SLJIT_MOV;
        size = 8;
        break;
    case I32StoreOpcode:
        opcode = SLJIT_MOV32;
        size = 4;
        break;
    case I32Store8Opcode:
        opcode = SLJIT_MOV32_U8;
        size = 1;
        break;
    case I32Store16Opcode:
        opcode = SLJIT_MOV32_U16;
        size = 2;
        break;
    case I64StoreOpcode:
        opcode = SLJIT_MOV;
        size = 8;
        break;
    case I64Store8Opcode:
        opcode = SLJIT_MOV_U8;
        size = 1;
        break;
    case I64Store16Opcode:
        opcode = SLJIT_MOV_U16;
        size = 2;
        break;
    case I64Store32Opcode:
        opcode = SLJIT_MOV_U32;
        size = 4;
        break;
    case F32StoreOpcode:
        opcode = SLJIT_MOV_F32;
        size = 4;
        break;
    default:
        ASSERT(instr->opcode() == F64StoreOpcode);
        opcode = SLJIT_MOV_F64;
        size = 8;
        break;
    }

    if (instr->opcode() != Store32Opcode && instr->opcode() != Store64Opcode) {
        MemoryStore* storeOperation = reinterpret_cast<MemoryStore*>(instr->byteCode());
        offset = storeOperation->offset();
    }

    Operand* operands = instr->operands();
    MemAddress addr;

    addr.check(compiler, operands, offset, size);

#if (defined SLJIT_32BIT_ARCHITECTURE && SLJIT_32BIT_ARCHITECTURE)
    if (addr.memArg.arg == 0) {
        return;
    }
#endif /* SLJIT_32BIT_ARCHITECTURE */

    if (opcode == SLJIT_MOV_F64 || opcode == SLJIT_MOV_F32) {
        JITArg valueArg;
        floatOperandToArg(compiler, operands + 1, valueArg, SLJIT_FR0);

        if (addr.offsetReg != 0) {
            if (SLJIT_IS_MEM(valueArg.arg)) {
                sljit_emit_fop1(compiler, opcode, SLJIT_FR0, 0, valueArg.arg, valueArg.argw);
                valueArg.arg = SLJIT_FR0;
                valueArg.argw = 0;
            }

            sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, addr.offsetReg, 0);
        }

        // TODO: sljit_emit_fmem for unaligned access
        sljit_emit_fop1(compiler, opcode, addr.memArg.arg, addr.memArg.argw, valueArg.arg, valueArg.argw);
        return;
    }

    JITArg valueArg;
#if (defined SLJIT_32BIT_ARCHITECTURE && SLJIT_32BIT_ARCHITECTURE)
    if (!(opcode & SLJIT_32)) {
        JITArgPair valueArgPair;
        operandToArgPair(operands + 1, valueArgPair);

        if (opcode == SLJIT_MOV) {
            if (addr.offsetReg != 0) {
                sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, addr.offsetReg, 0);
            }

            sljit_s32 dstReg1 = GET_SOURCE_REG(valueArgPair.arg1, SLJIT_R1);
            sljit_s32 dstReg2 = GET_SOURCE_REG(valueArgPair.arg2, SLJIT_R2);

            if (SLJIT_IS_MEM(valueArgPair.arg1)) {
#if (defined SLJIT_BIG_ENDIAN && SLJIT_BIG_ENDIAN)
                sljit_emit_mem(compiler, opcode | SLJIT_MEM_LOAD, SLJIT_REG_PAIR(dstReg1, dstReg2), valueArgPair.arg2, valueArgPair.arg2w);
#else /* !SLJIT_BIG_ENDIAN */
                sljit_emit_mem(compiler, opcode | SLJIT_MEM_LOAD, SLJIT_REG_PAIR(dstReg1, dstReg2), valueArgPair.arg1, valueArgPair.arg1w);
#endif /* SLJIT_BIG_ENDIAN */
            } else if (SLJIT_IS_IMM(valueArgPair.arg1)) {
#if (defined SLJIT_BIG_ENDIAN && SLJIT_BIG_ENDIAN)
                sljit_emit_op1(compiler, SLJIT_MOV, dstReg1, 0, SLJIT_IMM, valueArgPair.arg1w);
                sljit_emit_op1(compiler, SLJIT_MOV, dstReg2, 0, SLJIT_IMM, valueArgPair.arg2w);
#else /* !SLJIT_BIG_ENDIAN */
                sljit_emit_op1(compiler, SLJIT_MOV, dstReg2, 0, SLJIT_IMM, valueArgPair.arg2w);
                sljit_emit_op1(compiler, SLJIT_MOV, dstReg1, 0, SLJIT_IMM, valueArgPair.arg1w);
#endif /* SLJIT_BIG_ENDIAN */
            }

            sljit_emit_mem(compiler, opcode | SLJIT_MEM_STORE, SLJIT_REG_PAIR(dstReg1, dstReg2), addr.memArg.arg, addr.memArg.argw);
            return;
        }

        // Ignore the high word.
        SLJIT_ASSERT(size <= 4);
        valueArg.arg = valueArgPair.arg1;
        valueArg.argw = valueArgPair.arg1w;
    } else {
        operandToArg(operands + 1, valueArg);
    }
#else /* !SLJIT_32BIT_ARCHITECTURE */
    operandToArg(operands + 1, valueArg);
#endif /* SLJIT_32BIT_ARCHITECTURE */

    if (SLJIT_IS_MEM(valueArg.arg)) {
#if (defined SLJIT_32BIT_ARCHITECTURE && SLJIT_32BIT_ARCHITECTURE)
        sljit_s32 mov_opcode = SLJIT_MOV;
#else /* !SLJIT_32BIT_ARCHITECTURE */
        sljit_s32 mov_opcode = (opcode == SLJIT_MOV32 || (opcode & SLJIT_32)) ? SLJIT_MOV32 : SLJIT_MOV;
#endif /* SLJIT_32BIT_ARCHITECTURE */

        sljit_emit_op1(compiler, mov_opcode, SLJIT_R1, 0, valueArg.arg, valueArg.argw);
        valueArg.arg = SLJIT_R1;
        valueArg.argw = 0;
    }

    if (addr.offsetReg != 0) {
        sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, addr.offsetReg, 0);
    }

    // TODO: sljit_emit_mem for unaligned access
    sljit_emit_op1(compiler, opcode, addr.memArg.arg, addr.memArg.argw, valueArg.arg, valueArg.argw);
}

#define MODIFY_SIZE_CONSTRAINT(old, op, modify, size) (((old) & (~(size))) | ((size) & (old op modify)))
#define MODIFY_SIZE_CONSTRAINT_XCHG(oldVal, newVal, size) (((oldVal) & ~(size)) | ((newVal) & (size)))
#define SIZE_MASK_64 0xffffffffffffffff
#define SIZE_MASK_32 0xffffffff
#define SIZE_MASK_16 0xffff
#define SIZE_MASK_8 0xff
// TODO: these are not good, if if ever more OP2 sljit operations are added
#define OP_XCHG (SLJIT_OP2_BASE + 16)
#define OP_CMPXCHG (SLJIT_OP2_BASE + 17)
#define OP_LOAD (SLJIT_OP2_BASE + 18)
#define OP_STORE (SLJIT_OP2_BASE + 19)

uint64_t getModifyMask(sljit_s32 size)
{
    switch (size) {
    case 8:
        return SIZE_MASK_8;
    case 16:
        return SIZE_MASK_16;
    case 32:
        return SIZE_MASK_32;
    case 64:
        return SIZE_MASK_64;
    default:
        ASSERT_NOT_REACHED();
    }
}

#if (defined SLJIT_32BIT_ARCHITECTURE && SLJIT_32BIT_ARCHITECTURE)
static int64_t atomicRmwGenericLoad64(std::atomic<int64_t>& shared, sljit_s32 modify_mask_size)
{
    return shared.load(std::memory_order_relaxed) & getModifyMask(modify_mask_size);
}

static void atomicRmwGenericStore64(std::atomic<int64_t>& shared, int64_t* value, sljit_s32* modify_mask_size)
{
    int64_t oldValue = shared.load(std::memory_order_relaxed);
    uint64_t modify_mask = getModifyMask(*modify_mask_size);
    while (!shared.compare_exchange_weak(oldValue, (oldValue & ~modify_mask) | ((*value) & modify_mask))) {}
}

static int64_t atomicRmwGeneric64(std::atomic<int64_t>& shared, int64_t* value, int64_t* params)
{
    sljit_s32 op = (sljit_s32)((sljit_s32)((*params) >> 32));
    int64_t modify_mask = getModifyMask((*params));

    int64_t oldValue = shared.load(std::memory_order_relaxed);
    int64_t newValue;

    switch (op) {
    case SLJIT_ADD:
        newValue = MODIFY_SIZE_CONSTRAINT(oldValue, +, *value, modify_mask);
        break;
    case SLJIT_SUB:
        newValue = MODIFY_SIZE_CONSTRAINT(oldValue, -, *value, modify_mask);
        break;
    case SLJIT_AND:
        newValue = MODIFY_SIZE_CONSTRAINT(oldValue, &, *value, modify_mask);
        break;
    case SLJIT_OR:
        newValue = MODIFY_SIZE_CONSTRAINT(oldValue, |, *value, modify_mask);
        break;
    case SLJIT_XOR:
        newValue = MODIFY_SIZE_CONSTRAINT(oldValue, ^, *value, modify_mask);
        break;
    case OP_XCHG:
        newValue = MODIFY_SIZE_CONSTRAINT_XCHG(oldValue, *value, modify_mask);
        break;
    }
    while (!shared.compare_exchange_weak(oldValue, newValue)) {}
    return oldValue & modify_mask;
}

static int64_t atomicRmwGenericCmpxchg64(std::atomic<int64_t>& shared, int64_t oldValue, int64_t newValue, int64_t modify_mask)
{
    int64_t originalMemValue = shared.load(std::memory_order_relaxed);
    while (!shared.compare_exchange_weak(oldValue, MODIFY_SIZE_CONSTRAINT_XCHG(originalMemValue, newValue, modify_mask))) {}
    return originalMemValue & modify_mask;
}

static int64_t atomicRmw64Cmpxchg64(std::atomic<int64_t>& shared, int64_t* originalValue, int64_t* newValue)
{
    return atomicRmwGenericCmpxchg64(shared, *originalValue, *newValue, SIZE_MASK_64);
}

static int64_t atomicRmw8Cmpxchg64(std::atomic<int64_t>& shared, int64_t* originalValue, int64_t* newValue)
{
    return atomicRmwGenericCmpxchg64(shared, *originalValue, *newValue, SIZE_MASK_8);
}

static int64_t atomicRmw16Cmpxchg64(std::atomic<int64_t>& shared, int64_t* originalValue, int64_t* newValue)
{
    return atomicRmwGenericCmpxchg64(shared, *originalValue, *newValue, SIZE_MASK_16);
}

static int64_t atomicRmw32Cmpxchg64(std::atomic<int64_t>& shared, int64_t* originalValue, int64_t* newValue)
{
    return atomicRmwGenericCmpxchg64(shared, *originalValue, *newValue, SIZE_MASK_32);
}

static void emitAtomicLoad64(sljit_compiler* compiler, sljit_s32 opcode, JITArgPair* args, MemAddress memAddr)
{
    sljit_s32 type = SLJIT_ARGS3(W, P, W, W);
    sljit_s32 addr;
    sljit_s32 size;

    switch (opcode) {
    case I64AtomicLoadOpcode: {
        size = 64;
        break;
    }
    case I64AtomicLoad8UOpcode: {
        size = 8;
        break;
    }
    case I64AtomicLoad16UOpcode: {
        size = 16;
        break;
    }
    case I64AtomicLoad32UOpcode: {
        size = 32;
        break;
    }
    }

    addr = GET_FUNC_ADDR(sljit_sw, atomicRmwGenericLoad64);

    sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, memAddr.memArg.arg & ~SLJIT_MEM, 0, SLJIT_IMM, memAddr.memArg.argw);
    sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, size);
    sljit_emit_icall(compiler, SLJIT_CALL, type, SLJIT_IMM, addr);
    sljit_emit_op1(compiler, SLJIT_MOV, args[1].arg1, args[1].arg1w, SLJIT_R0, 0);
    sljit_emit_op1(compiler, SLJIT_MOV, args[1].arg2, args[1].arg2w, SLJIT_R1, 0);
}

static void emitAtomicStore64(sljit_compiler* compiler, sljit_s32 opcode, JITArgPair* args, MemAddress memAddr)
{
    sljit_s32 type = SLJIT_ARGS3(VOID, P, W, W);
    sljit_s32 addr;
    sljit_s32 size;


    switch (opcode) {
    case I64AtomicStoreOpcode: {
        size = 64;
        break;
    }
    case I64AtomicStore8Opcode: {
        size = 8;
        break;
    }
    case I64AtomicStore16Opcode: {
        size = 16;
        break;
    }
    case I64AtomicStore32Opcode: {
        size = 32;
        break;
    }
    }

    addr = GET_FUNC_ADDR(sljit_sw, atomicRmwGenericStore64);

    sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(kContextReg), OffsetOfContextField(tmp1) + WORD_LOW_OFFSET, args[1].arg1, args[1].arg1w);
    sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(kContextReg), OffsetOfContextField(tmp1) + WORD_HIGH_OFFSET, args[1].arg2, args[1].arg2w);
    sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(kContextReg), OffsetOfContextField(tmp2) + WORD_LOW_OFFSET, SLJIT_IMM, size);

    sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, memAddr.memArg.arg & ~SLJIT_MEM, 0, SLJIT_IMM, memAddr.memArg.argw);
    sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, kContextReg, 0, SLJIT_IMM, OffsetOfContextField(tmp1));
    sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R2, 0, kContextReg, 0, SLJIT_IMM, OffsetOfContextField(tmp2));
    sljit_emit_icall(compiler, SLJIT_CALL, type, SLJIT_IMM, addr);
}

static void emitAtomicRmw64(sljit_compiler* compiler, sljit_s32 opcode, JITArgPair* args, MemAddress memAddr)
{
    sljit_s32 type = SLJIT_ARGS3(W, P, W, W);
    sljit_s32 addr;
    sljit_s32 size;
    sljit_s32 op;

    switch (opcode) {
    case I64AtomicRmwAddOpcode:
    case I64AtomicRmw8AddUOpcode:
    case I64AtomicRmw16AddUOpcode:
    case I64AtomicRmw32AddUOpcode: {
        op = SLJIT_ADD;
        break;
    }
    case I64AtomicRmwSubOpcode:
    case I64AtomicRmw8SubUOpcode:
    case I64AtomicRmw16SubUOpcode:
    case I64AtomicRmw32SubUOpcode: {
        op = SLJIT_SUB;
        break;
    }
    case I64AtomicRmwAndOpcode:
    case I64AtomicRmw8AndUOpcode:
    case I64AtomicRmw16AndUOpcode:
    case I64AtomicRmw32AndUOpcode: {
        op = SLJIT_AND;
        break;
    }
    case I64AtomicRmwOrOpcode:
    case I64AtomicRmw8OrUOpcode:
    case I64AtomicRmw16OrUOpcode:
    case I64AtomicRmw32OrUOpcode: {
        op = SLJIT_OR;
        break;
    }
    case I64AtomicRmwXorOpcode:
    case I64AtomicRmw8XorUOpcode:
    case I64AtomicRmw16XorUOpcode:
    case I64AtomicRmw32XorUOpcode: {
        op = SLJIT_XOR;
        break;
    }
    case I64AtomicRmwXchgOpcode:
    case I64AtomicRmw8XchgUOpcode:
    case I64AtomicRmw16XchgUOpcode:
    case I64AtomicRmw32XchgUOpcode: {
        op = OP_XCHG;
        break;
    }
    }

    switch (opcode) {
    case I64AtomicRmwAddOpcode:
    case I64AtomicRmwSubOpcode:
    case I64AtomicRmwAndOpcode:
    case I64AtomicRmwOrOpcode:
    case I64AtomicRmwXorOpcode:
    case I64AtomicRmwXchgOpcode: {
        size = 64;
        break;
    }
    case I64AtomicRmw8AddUOpcode:
    case I64AtomicRmw8SubUOpcode:
    case I64AtomicRmw8AndUOpcode:
    case I64AtomicRmw8OrUOpcode:
    case I64AtomicRmw8XorUOpcode:
    case I64AtomicRmw8XchgUOpcode: {
        size = 8;
        break;
    }
    case I64AtomicRmw16AddUOpcode:
    case I64AtomicRmw16SubUOpcode:
    case I64AtomicRmw16AndUOpcode:
    case I64AtomicRmw16OrUOpcode:
    case I64AtomicRmw16XorUOpcode:
    case I64AtomicRmw16XchgUOpcode: {
        size = 16;
        break;
    }
    case I64AtomicRmw32AddUOpcode:
    case I64AtomicRmw32SubUOpcode:
    case I64AtomicRmw32AndUOpcode:
    case I64AtomicRmw32OrUOpcode:
    case I64AtomicRmw32XorUOpcode:
    case I64AtomicRmw32XchgUOpcode: {
        size = 32;
        break;
    }
    }

    addr = SLJIT_FUNC_ADDR(atomicRmwGeneric64);

    sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(kContextReg), OffsetOfContextField(tmp1) + WORD_LOW_OFFSET, args[1].arg1, args[1].arg1w);
    sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(kContextReg), OffsetOfContextField(tmp1) + WORD_HIGH_OFFSET, args[1].arg2, args[1].arg2w);
    sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(kContextReg), OffsetOfContextField(tmp2) + WORD_LOW_OFFSET, SLJIT_IMM, size);
    sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(kContextReg), OffsetOfContextField(tmp2) + WORD_HIGH_OFFSET, SLJIT_IMM, op);

    sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, memAddr.memArg.arg & ~SLJIT_MEM, 0, SLJIT_IMM, memAddr.memArg.argw);
    sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, kContextReg, 0, SLJIT_IMM, OffsetOfContextField(tmp1));
    sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R2, 0, kContextReg, 0, SLJIT_IMM, OffsetOfContextField(tmp2));
    sljit_emit_icall(compiler, SLJIT_CALL, type, SLJIT_IMM, addr);
    sljit_emit_op1(compiler, SLJIT_MOV, args[2].arg1, args[2].arg1w, SLJIT_R0, 0);
    sljit_emit_op1(compiler, SLJIT_MOV, args[2].arg2, args[2].arg2w, SLJIT_R1, 0);
}

static void emitAtomicCmpxchg64(sljit_compiler* compiler, sljit_s32 opcode, JITArgPair* args, MemAddress memAddr)
{
    sljit_s32 type = SLJIT_ARGS3(W, P, P, P);
    sljit_s32 addr;

    switch (opcode) {
    case I64AtomicRmwCmpxchgOpcode: {
        addr = GET_FUNC_ADDR(sljit_sw, atomicRmw64Cmpxchg64);
        break;
    }
    case I64AtomicRmw8CmpxchgUOpcode: {
        addr = GET_FUNC_ADDR(sljit_sw, atomicRmw8Cmpxchg64);
        break;
    }
    case I64AtomicRmw16CmpxchgUOpcode: {
        addr = GET_FUNC_ADDR(sljit_sw, atomicRmw16Cmpxchg64);
        break;
    }
    case I64AtomicRmw32CmpxchgUOpcode: {
        addr = GET_FUNC_ADDR(sljit_sw, atomicRmw32Cmpxchg64);
        break;
    }
    }

    sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(kContextReg), OffsetOfContextField(tmp1) + WORD_LOW_OFFSET, args[1].arg1, args[1].arg1w);
    sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(kContextReg), OffsetOfContextField(tmp1) + WORD_HIGH_OFFSET, args[1].arg2, args[1].arg2w);
    sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(kContextReg), OffsetOfContextField(tmp2) + WORD_LOW_OFFSET, args[2].arg1, args[2].arg1w);
    sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(kContextReg), OffsetOfContextField(tmp2) + WORD_HIGH_OFFSET, args[2].arg2, args[2].arg2w);

    sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, memAddr.memArg.arg & ~SLJIT_MEM, 0, SLJIT_IMM, memAddr.memArg.argw);
    sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, kContextReg, 0, SLJIT_IMM, OffsetOfContextField(tmp1));
    sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R2, 0, kContextReg, 0, SLJIT_IMM, OffsetOfContextField(tmp2));
    sljit_emit_icall(compiler, SLJIT_CALL, type, SLJIT_IMM, addr);
    sljit_emit_op1(compiler, SLJIT_MOV, args[3].arg1, args[3].arg1w, SLJIT_R0, 0);
    sljit_emit_op1(compiler, SLJIT_MOV, args[3].arg2, args[3].arg2w, SLJIT_R1, 0);
}
#endif /* SLJIT_32BIT_ARCHITECTURE */

#define ATOMIC_DATA_REG SLJIT_R0
#define ATOMIC_MEM_REG SLJIT_R1
#define ATOMIC_TEMP_REG SLJIT_R2

#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
#define IS_64_BIT 1
#else
#define IS_64_BIT 0
#endif

static void emitAtomic(sljit_compiler* compiler, Instruction* instr)
{
    Operand* operands = instr->operands();
    sljit_s32 operation_size = SLJIT_MOV;
    sljit_s32 size = 0;
    sljit_s32 offset = 0;
    sljit_s32 operation;
    MemAddress addr;

    switch (instr->opcode()) {
    case I64AtomicLoadOpcode:
    case I64AtomicStoreOpcode:
    case I64AtomicRmwAddOpcode:
    case I64AtomicRmwSubOpcode:
    case I64AtomicRmwAndOpcode:
    case I64AtomicRmwOrOpcode:
    case I64AtomicRmwXorOpcode:
    case I64AtomicRmwXchgOpcode:
    case I64AtomicRmwCmpxchgOpcode: {
        operation_size = SLJIT_MOV;
        size = 8;
        break;
    }
    case I32AtomicLoadOpcode:
    case I64AtomicLoad32UOpcode:
    case I32AtomicStoreOpcode:
    case I64AtomicStore32Opcode:
    case I32AtomicRmwAddOpcode:
    case I64AtomicRmw32AddUOpcode:
    case I32AtomicRmwSubOpcode:
    case I64AtomicRmw32SubUOpcode:
    case I32AtomicRmwAndOpcode:
    case I64AtomicRmw32AndUOpcode:
    case I32AtomicRmwOrOpcode:
    case I64AtomicRmw32OrUOpcode:
    case I32AtomicRmwXorOpcode:
    case I64AtomicRmw32XorUOpcode:
    case I32AtomicRmwXchgOpcode:
    case I64AtomicRmw32XchgUOpcode:
    case I32AtomicRmwCmpxchgOpcode:
    case I64AtomicRmw32CmpxchgUOpcode: {
        operation_size = SLJIT_MOV32;
        size = 4;
        break;
    }
    case I32AtomicLoad8UOpcode:
    case I64AtomicLoad8UOpcode:
    case I32AtomicStore8Opcode:
    case I64AtomicStore8Opcode:
    case I32AtomicRmw8AddUOpcode:
    case I64AtomicRmw8AddUOpcode:
    case I32AtomicRmw8SubUOpcode:
    case I64AtomicRmw8SubUOpcode:
    case I32AtomicRmw8AndUOpcode:
    case I64AtomicRmw8AndUOpcode:
    case I32AtomicRmw8OrUOpcode:
    case I64AtomicRmw8OrUOpcode:
    case I32AtomicRmw8XorUOpcode:
    case I64AtomicRmw8XorUOpcode:
    case I32AtomicRmw8XchgUOpcode:
    case I64AtomicRmw8XchgUOpcode:
    case I32AtomicRmw8CmpxchgUOpcode:
    case I64AtomicRmw8CmpxchgUOpcode: {
        operation_size = SLJIT_MOV_U8;
        size = 1;
        break;
    }
    case I32AtomicLoad16UOpcode:
    case I64AtomicLoad16UOpcode:
    case I32AtomicStore16Opcode:
    case I64AtomicStore16Opcode:
    case I32AtomicRmw16AddUOpcode:
    case I64AtomicRmw16AddUOpcode:
    case I32AtomicRmw16SubUOpcode:
    case I64AtomicRmw16SubUOpcode:
    case I32AtomicRmw16AndUOpcode:
    case I64AtomicRmw16AndUOpcode:
    case I32AtomicRmw16OrUOpcode:
    case I64AtomicRmw16OrUOpcode:
    case I32AtomicRmw16XorUOpcode:
    case I64AtomicRmw16XorUOpcode:
    case I32AtomicRmw16XchgUOpcode:
    case I64AtomicRmw16XchgUOpcode:
    case I32AtomicRmw16CmpxchgUOpcode:
    case I64AtomicRmw16CmpxchgUOpcode: {
        operation_size = SLJIT_MOV_U16;
        size = 2;
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    switch (instr->opcode()) {
    case I32AtomicLoadOpcode:
    case I32AtomicLoad8UOpcode:
    case I32AtomicLoad16UOpcode:
    case I64AtomicLoadOpcode:
    case I64AtomicLoad8UOpcode:
    case I64AtomicLoad16UOpcode:
    case I64AtomicLoad32UOpcode: {
        operation = OP_LOAD;
        break;
    }
    case I32AtomicStoreOpcode:
    case I32AtomicStore8Opcode:
    case I32AtomicStore16Opcode:
    case I64AtomicStoreOpcode:
    case I64AtomicStore8Opcode:
    case I64AtomicStore16Opcode:
    case I64AtomicStore32Opcode: {
        operation = OP_STORE;
        break;
    }
    case I32AtomicRmwAddOpcode:
    case I32AtomicRmw8AddUOpcode:
    case I32AtomicRmw16AddUOpcode:
    case I64AtomicRmwAddOpcode:
    case I64AtomicRmw8AddUOpcode:
    case I64AtomicRmw16AddUOpcode:
    case I64AtomicRmw32AddUOpcode: {
        operation = SLJIT_ADD;
        break;
    }
    case I32AtomicRmwSubOpcode:
    case I32AtomicRmw8SubUOpcode:
    case I32AtomicRmw16SubUOpcode:
    case I64AtomicRmwSubOpcode:
    case I64AtomicRmw8SubUOpcode:
    case I64AtomicRmw16SubUOpcode:
    case I64AtomicRmw32SubUOpcode: {
        operation = SLJIT_SUB;
        break;
    }
    case I32AtomicRmwAndOpcode:
    case I32AtomicRmw8AndUOpcode:
    case I32AtomicRmw16AndUOpcode:
    case I64AtomicRmwAndOpcode:
    case I64AtomicRmw8AndUOpcode:
    case I64AtomicRmw16AndUOpcode:
    case I64AtomicRmw32AndUOpcode: {
        operation = SLJIT_AND;
        break;
    }
    case I32AtomicRmwOrOpcode:
    case I32AtomicRmw8OrUOpcode:
    case I32AtomicRmw16OrUOpcode:
    case I64AtomicRmwOrOpcode:
    case I64AtomicRmw8OrUOpcode:
    case I64AtomicRmw16OrUOpcode:
    case I64AtomicRmw32OrUOpcode: {
        operation = SLJIT_OR;
        break;
    }
    case I32AtomicRmwXorOpcode:
    case I32AtomicRmw8XorUOpcode:
    case I32AtomicRmw16XorUOpcode:
    case I64AtomicRmwXorOpcode:
    case I64AtomicRmw8XorUOpcode:
    case I64AtomicRmw16XorUOpcode:
    case I64AtomicRmw32XorUOpcode: {
        operation = SLJIT_XOR;
        break;
    }
    case I32AtomicRmwXchgOpcode:
    case I32AtomicRmw8XchgUOpcode:
    case I32AtomicRmw16XchgUOpcode:
    case I64AtomicRmwXchgOpcode:
    case I64AtomicRmw8XchgUOpcode:
    case I64AtomicRmw16XchgUOpcode:
    case I64AtomicRmw32XchgUOpcode: {
        operation = OP_XCHG;
        break;
    }
    case I32AtomicRmwCmpxchgOpcode:
    case I32AtomicRmw8CmpxchgUOpcode:
    case I32AtomicRmw16CmpxchgUOpcode:
    case I64AtomicRmwCmpxchgOpcode:
    case I64AtomicRmw8CmpxchgUOpcode:
    case I64AtomicRmw16CmpxchgUOpcode:
    case I64AtomicRmw32CmpxchgUOpcode: {
        operation = OP_CMPXCHG;
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    switch (operation) {
    case OP_LOAD: {
        AtomicLoad* loadOperation = reinterpret_cast<AtomicLoad*>(instr->byteCode());
        offset = loadOperation->offset();
        break;
    }
    case OP_STORE: {
        AtomicStore* storeOperation = reinterpret_cast<AtomicStore*>(instr->byteCode());
        offset = storeOperation->offset();
        break;
    }
    case SLJIT_ADD:
    case SLJIT_SUB:
    case SLJIT_AND:
    case SLJIT_OR:
    case SLJIT_XOR:
    case OP_XCHG: {
        AtomicRmw* rmwOperation = reinterpret_cast<AtomicRmw*>(instr->byteCode());
        offset = rmwOperation->offset();
        break;
    }
    case OP_CMPXCHG: {
        AtomicCmpxchg* cmpxchgOperation = reinterpret_cast<AtomicCmpxchg*>(instr->byteCode());
        offset = cmpxchgOperation->offset();
        break;
    }
    }

    addr.check(compiler, operands, offset, size);

    if (IS_64_BIT || (!IS_64_BIT && instr->info() & Instruction::kIs32Bit)) {
        JITArg args[instr->paramCount() + instr->resultCount()];
        for (unsigned int i = 0; i < instr->paramCount() + instr->resultCount(); ++i) {
            operandToArg(operands + i, args[i]);
        }

        sljit_emit_op2(compiler, SLJIT_ADD, ATOMIC_MEM_REG, 0, addr.memArg.arg & ~SLJIT_MEM, 0, SLJIT_IMM, addr.memArg.argw);

        switch (operation) {
        case OP_LOAD: {
            sljit_emit_atomic_load(compiler, operation_size, ATOMIC_DATA_REG, ATOMIC_MEM_REG);
            sljit_emit_op1(compiler, SLJIT_MOV, ATOMIC_TEMP_REG, 0, ATOMIC_DATA_REG, 0);
            sljit_emit_op1(compiler, SLJIT_MOV, args[1].arg, args[1].argw, ATOMIC_DATA_REG, 0);
            break;
        }
        case OP_STORE: {
            struct sljit_label* store_failure = sljit_emit_label(compiler);
            /* NOTE: on some architectures storing without a load to lock the memory will cause the store to always fail. */
            sljit_emit_atomic_load(compiler, operation_size, ATOMIC_DATA_REG, ATOMIC_MEM_REG); // but this would overwrite any changes to the data
            sljit_emit_op1(compiler, SLJIT_MOV, ATOMIC_DATA_REG, 0, args[1].arg, args[1].argw);
            sljit_emit_atomic_store(compiler, operation_size | SLJIT_SET_ATOMIC_STORED, ATOMIC_DATA_REG, ATOMIC_MEM_REG, ATOMIC_TEMP_REG);
            sljit_set_label(sljit_emit_jump(compiler, SLJIT_ATOMIC_NOT_STORED), store_failure);
            break;
        }
        case SLJIT_ADD:
        case SLJIT_SUB:
        case SLJIT_AND:
        case SLJIT_OR:
        case SLJIT_XOR: {
            struct sljit_label* store_failure = sljit_emit_label(compiler);
            sljit_emit_atomic_load(compiler, operation_size, ATOMIC_DATA_REG, ATOMIC_MEM_REG);
            sljit_emit_op1(compiler, SLJIT_MOV, ATOMIC_TEMP_REG, 0, ATOMIC_DATA_REG, 0);
            sljit_emit_op1(compiler, SLJIT_MOV, args[2].arg, args[2].argw, ATOMIC_DATA_REG, 0);
            sljit_emit_op2(compiler, operation, ATOMIC_DATA_REG, 0, ATOMIC_DATA_REG, 0, args[1].arg, args[1].argw);
            sljit_emit_atomic_store(compiler, operation_size | SLJIT_SET_ATOMIC_STORED, ATOMIC_DATA_REG, ATOMIC_MEM_REG, ATOMIC_TEMP_REG);
            sljit_set_label(sljit_emit_jump(compiler, SLJIT_ATOMIC_NOT_STORED), store_failure);
            break;
        }
        case OP_XCHG: {
            struct sljit_label* store_failure = sljit_emit_label(compiler);
            sljit_emit_atomic_load(compiler, operation_size, ATOMIC_DATA_REG, ATOMIC_MEM_REG);
            sljit_emit_op1(compiler, SLJIT_MOV, ATOMIC_TEMP_REG, 0, ATOMIC_DATA_REG, 0);
            sljit_emit_op1(compiler, SLJIT_MOV, args[2].arg, args[2].argw, ATOMIC_DATA_REG, 0);
            sljit_emit_op1(compiler, SLJIT_MOV, ATOMIC_DATA_REG, 0, args[1].arg, args[1].argw);
            sljit_emit_atomic_store(compiler, operation_size | SLJIT_SET_ATOMIC_STORED, ATOMIC_DATA_REG, ATOMIC_MEM_REG, ATOMIC_TEMP_REG);
            sljit_set_label(sljit_emit_jump(compiler, SLJIT_ATOMIC_NOT_STORED), store_failure);
            break;
        }
        case OP_CMPXCHG: {
            struct sljit_jump* cmp_value_mismatch;
            struct sljit_label* store_failure = sljit_emit_label(compiler);
            sljit_emit_atomic_load(compiler, operation_size, ATOMIC_DATA_REG, ATOMIC_MEM_REG);
            sljit_emit_op1(compiler, SLJIT_MOV, ATOMIC_TEMP_REG, 0, ATOMIC_DATA_REG, 0);
            sljit_emit_op1(compiler, SLJIT_MOV, args[3].arg, args[3].argw, ATOMIC_DATA_REG, 0);
            cmp_value_mismatch = sljit_emit_cmp(compiler, SLJIT_NOT_EQUAL, ATOMIC_DATA_REG, 0, args[1].arg, args[1].argw);
            sljit_emit_op1(compiler, SLJIT_MOV, ATOMIC_DATA_REG, 0, args[2].arg, args[2].argw);
            sljit_emit_atomic_store(compiler, operation_size | SLJIT_SET_ATOMIC_STORED, ATOMIC_DATA_REG, ATOMIC_MEM_REG, ATOMIC_TEMP_REG);
            sljit_set_label(sljit_emit_jump(compiler, SLJIT_ATOMIC_NOT_STORED), store_failure);
            sljit_set_label(cmp_value_mismatch, sljit_emit_label(compiler));
            break;
        }
        }
        return;
    }
#if (defined SLJIT_32BIT_ARCHITECTURE && SLJIT_32BIT_ARCHITECTURE)
    JITArgPair args[instr->paramCount() + instr->resultCount()];
    for (unsigned int i = 0; i < instr->paramCount() + instr->resultCount(); ++i) {
        operandToArgPair(operands + i, args[i]);
    }
    switch (instr->opcode()) {
    case I64AtomicLoadOpcode:
    case I64AtomicLoad8UOpcode:
    case I64AtomicLoad16UOpcode:
    case I64AtomicLoad32UOpcode: {
        emitAtomicLoad64(compiler, instr->opcode(), args, addr);
        break;
    }
    case I64AtomicStoreOpcode:
    case I64AtomicStore8Opcode:
    case I64AtomicStore16Opcode:
    case I64AtomicStore32Opcode: {
        emitAtomicStore64(compiler, instr->opcode(), args, addr);
        break;
    }
    case I64AtomicRmwAddOpcode:
    case I64AtomicRmw8AddUOpcode:
    case I64AtomicRmw16AddUOpcode:
    case I64AtomicRmw32AddUOpcode:
    case I64AtomicRmwSubOpcode:
    case I64AtomicRmw8SubUOpcode:
    case I64AtomicRmw16SubUOpcode:
    case I64AtomicRmw32SubUOpcode:
    case I64AtomicRmwAndOpcode:
    case I64AtomicRmw8AndUOpcode:
    case I64AtomicRmw16AndUOpcode:
    case I64AtomicRmw32AndUOpcode:
    case I64AtomicRmwOrOpcode:
    case I64AtomicRmw8OrUOpcode:
    case I64AtomicRmw16OrUOpcode:
    case I64AtomicRmw32OrUOpcode:
    case I64AtomicRmwXorOpcode:
    case I64AtomicRmw8XorUOpcode:
    case I64AtomicRmw16XorUOpcode:
    case I64AtomicRmw32XorUOpcode:
    case I64AtomicRmwXchgOpcode:
    case I64AtomicRmw8XchgUOpcode:
    case I64AtomicRmw16XchgUOpcode:
    case I64AtomicRmw32XchgUOpcode: {
        emitAtomicRmw64(compiler, instr->opcode(), args, addr);
        break;
    }
    case I64AtomicRmwCmpxchgOpcode:
    case I64AtomicRmw8CmpxchgUOpcode:
    case I64AtomicRmw16CmpxchgUOpcode:
    case I64AtomicRmw32CmpxchgUOpcode: {
        emitAtomicCmpxchg64(compiler, instr->opcode(), args, addr);
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
#endif /* SLJIT_32BIT_ARCHITECTURE  */
}

#undef MODIFY_SIZE_CONSTRAINT
#undef MODIFY_SIZE_CONSTRAINT_XCHG
#undef SIZE_MASK_64
#undef SIZE_MASK_32
#undef SIZE_MASK_16
#undef SIZE_MASK_8
#undef OP_XCHG
#undef OP_CMPXCHG
#undef OP_LOAD
#undef OP_STORE
#undef IS_64_BIT

static sljit_sw initMemory(uint32_t dstStart, uint32_t srcStart, uint32_t srcSize, ExecutionContext* context)
{
    try {
        uint32_t segmentIndex = *(sljit_u32*)&context->tmp1;
        DataSegment& sg = context->instance->dataSegment(segmentIndex);

        context->memory0->init(context->state, &sg, dstStart, srcStart, srcSize);
    } catch (std::unique_ptr<Exception>& e) {
        return ExecutionContext::OutOfBoundsMemAccessError;
    }
    return ExecutionContext::NoError;
}

static sljit_sw copyMemory(uint32_t dstStart, uint32_t srcStart, uint32_t size, ExecutionContext* context)
{
    try {
        context->memory0->copy(context->state, dstStart, srcStart, size);
    } catch (std::unique_ptr<Exception>& e) {
        return ExecutionContext::OutOfBoundsMemAccessError;
    }
    return ExecutionContext::NoError;
}

static sljit_sw fillMemory(uint32_t start, uint32_t value, uint32_t size, ExecutionContext* context)
{
    try {
        context->memory0->fill(context->state, start, static_cast<uint8_t>(value), size);
    } catch (std::unique_ptr<Exception>& e) {
        return ExecutionContext::OutOfBoundsMemAccessError;
    }
    return ExecutionContext::NoError;
}

static sljit_s32 growMemory(uint32_t newSize, ExecutionContext* context)
{
    uint32_t oldSize = context->memory0->sizeInPageSize();

    if (context->memory0->grow(static_cast<uint64_t>(newSize) * Memory::s_memoryPageSize)) {
        return static_cast<sljit_s32>(oldSize);
    }

    return -1;
}

static void emitMemory(sljit_compiler* compiler, Instruction* instr)
{
    CompileContext* context = CompileContext::get(compiler);
    Operand* params = instr->operands();
    OpcodeKind opcode = instr->opcode();

    switch (opcode) {
    case MemorySizeOpcode: {
        sljit_emit_op1(compiler, SLJIT_MOV_P, SLJIT_R0, 0, SLJIT_MEM1(kContextReg), OffsetOfContextField(memory0));

        JITArg dstArg;
        operandToArg(params, dstArg);

        sljit_s32 dstReg = GET_TARGET_REG(dstArg.arg, SLJIT_R0);

        sljit_emit_op1(compiler, SLJIT_MOV_U32, dstReg, 0, SLJIT_MEM1(SLJIT_R0), JITFieldAccessor::memorySizeInByteOffset());
        sljit_emit_op2(compiler, SLJIT_LSHR, dstReg, 0, dstReg, 0, SLJIT_IMM, 16);
        MOVE_FROM_REG(compiler, SLJIT_MOV_U32, dstArg.arg, dstArg.argw, dstReg);
        return;
    }
    case MemoryInitOpcode:
    case MemoryCopyOpcode:
    case MemoryFillOpcode: {
        JITArg srcArg;

        for (int i = 0; i < 3; i++) {
            operandToArg(params + i, srcArg);
            sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R(i), 0, srcArg.arg, srcArg.argw);
        }

        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, kContextReg, 0);

        sljit_sw addr;

        if (opcode == MemoryInitOpcode) {
            MemoryInit* memoryInit = reinterpret_cast<MemoryInit*>(instr->byteCode());

            sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(kContextReg), OffsetOfContextField(tmp1), SLJIT_IMM, memoryInit->segmentIndex());
            addr = GET_FUNC_ADDR(sljit_sw, initMemory);
        } else if (opcode == MemoryCopyOpcode) {
            addr = GET_FUNC_ADDR(sljit_sw, copyMemory);
        } else {
            addr = GET_FUNC_ADDR(sljit_sw, fillMemory);
        }

        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS4(W, 32, 32, 32, W), SLJIT_IMM, addr);

        // Currently all traps are OutOfBoundsMemAccessError.
        sljit_set_label(sljit_emit_cmp(compiler, SLJIT_NOT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, ExecutionContext::NoError), context->memoryTrapLabel);
        return;
    }
    case MemoryGrowOpcode: {
        JITArg arg;

        operandToArg(params, arg);
        MOVE_TO_REG(compiler, SLJIT_MOV32, SLJIT_R0, arg.arg, arg.argw);
        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, kContextReg, 0);

        sljit_sw addr = GET_FUNC_ADDR(sljit_sw, growMemory);
        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS2(32, 32, W), SLJIT_IMM, addr);

        operandToArg(params + 1, arg);
        MOVE_FROM_REG(compiler, SLJIT_MOV32, arg.arg, arg.argw, SLJIT_R0);
        return;
    }
    default: {
        return;
    }
    }
}

static void dropData(uint32_t segmentIndex, ExecutionContext* context)
{
    DataSegment& sg = context->instance->dataSegment(segmentIndex);
    sg.drop();
}

static void emitDataDrop(sljit_compiler* compiler, Instruction* instr)
{
    DataDrop* dataDrop = reinterpret_cast<DataDrop*>(instr->byteCode());

    sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, static_cast<sljit_sw>(dataDrop->segmentIndex()));
    sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, kContextReg, 0);

    sljit_sw addr = GET_FUNC_ADDR(sljit_sw, dropData);
    sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS2(VOID, 32, W), SLJIT_IMM, addr);
}
