/*
 * Copyright (c) 2009 The University of Edinburgh
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __ARCH_POWER_INSTS_INTEGER_HH__
#define __ARCH_POWER_INSTS_INTEGER_HH__

#include "arch/power/insts/static_inst.hh"
#include "base/bitfield.hh"
#include "base/cprintf.hh"

namespace PowerISA
{

/**
 * We provide a base class for integer operations and then inherit for
 * several other classes. These specialise for instructions using immediate
 * values and also rotate instructions. We also need to have versions that
 * consider the Rc and OE bits.
 */

/**
 * Base class for integer operations.
 */
class IntOp : public PowerStaticInst
{
  protected:

    bool rcSet;
    bool oeSet;

    // Needed for srawi only
    uint32_t sh;

    /// Constructor
    IntOp(const char *mnem, MachInst _machInst, OpClass __opClass)
      : PowerStaticInst(mnem, _machInst, __opClass),
        rcSet(false), oeSet(false)
    {
    }

    /* Compute the CR (condition register) field using signed comparison */
    inline uint32_t
    makeCRField(int64_t a, int64_t b, uint32_t xerSO) const
    {
        uint32_t c = xerSO;

        /* We've pre-shifted the immediate values here */
        if (a < b)      { c += 0x8; }
        else if (a > b) { c += 0x4; }
        else            { c += 0x2; }
        return c;
    }

    inline uint32_t
    makeCRField(int64_t a, int32_t b, uint32_t xerSO) const
    {
        return makeCRField(a, (int64_t)b, xerSO);
    }

    inline uint32_t
    makeCRField(int32_t a, int32_t b, uint32_t xerSO) const
    {
        return makeCRField((int64_t)a, (int64_t)b, xerSO);
    }

    /* Compute the CR (condition register) field using unsigned comparison */
    inline uint32_t
    makeCRField(uint64_t a, uint64_t b, uint32_t xerSO) const
    {
        uint32_t c = xerSO;

        /* We've pre-shifted the immediate values here */
        if (a < b)      { c += 0x8; }
        else if (a > b) { c += 0x4; }
        else            { c += 0x2; }
        return c;
    }

    inline uint32_t
    makeCRField(uint64_t a, uint32_t b, uint32_t xerSO) const
    {
        return makeCRField(a, (uint64_t)b, xerSO);
    }

    inline uint32_t
    makeCRField(uint32_t a, uint32_t b, uint32_t xerSO) const
    {
        return makeCRField((uint64_t)a, (uint64_t)b, xerSO);
    }

    std::string generateDisassembly(
            Addr pc, const Loader::SymbolTable *symtab) const override;
};


/**
 * Class for integer immediate (signed and unsigned) operations.
 */
class IntImmOp : public IntOp
{
  protected:

    int32_t imm;
    uint32_t uimm;

    /// Constructor
    IntImmOp(const char *mnem, MachInst _machInst, OpClass __opClass)
      : IntOp(mnem, _machInst, __opClass),
        imm(sext<16>(machInst.si)),
        uimm(machInst.si)
    {
    }

    std::string generateDisassembly(
            Addr pc, const Loader::SymbolTable *symtab) const override;
};


/**
 * Class for integer arithmetic operations.
 */
class IntArithOp : public IntOp
{
  protected:

    /// Constructor
    IntArithOp(const char *mnem, MachInst _machInst, OpClass __opClass)
      : IntOp(mnem, _machInst, __opClass)
    {
    }

    /* Compute 128-bit sum of 128-bit to 64-bit unsigned integer addition */
    inline std::tuple<uint64_t, uint64_t>
    add(uint64_t ralo, uint64_t rahi, uint64_t rb) const
    {
        uint64_t slo, shi;
    #if defined(__SIZEOF_INT128__)
        __uint128_t ra = ((__uint128_t)rahi << 64) | ralo;
        __uint128_t sum = ra + rb;
        slo = sum;
        shi = sum >> 64;
    #else
        shi = rahi + ((ralo + rb) < ralo);
        slo = ralo + rb;
    #endif
        return std::make_tuple(slo, shi);
    }

    /* Compute 128-bit sum of 128-bit to 64-bit signed integer addition */
    inline std::tuple<uint64_t, int64_t>
    add(uint64_t ralo, int64_t rahi, int64_t rb) const
    {
        uint64_t slo;
        int64_t shi;
    #if defined(__SIZEOF_INT128__)
        __int128_t ra = ((__int128_t)rahi << 64) | ralo;
        __int128_t sum = (__int128_t)ra + rb;
        slo = sum;
        shi = sum >> 64;
    #else
        if (rb < 0) {
            shi = rahi - 1;
            slo = ralo + rb;
            if (slo < rb) {
                shi++;
            }
        } else {
            shi = rahi;
            slo = ralo + rb;
            if (slo < rb) {
                shi++;
            }
        }
    #endif
        return std::make_tuple(slo, shi);
    }

    /* Compute 128-bit product of 64-bit unsigned integer multiplication */
    inline std::tuple<uint64_t, uint64_t>
    multiply(uint64_t ra, uint64_t rb) const
    {
        uint64_t plo, phi;
    #if defined(__SIZEOF_INT128__)
        __uint128_t prod = (__uint128_t)ra * rb;
        plo = prod;
        phi = prod >> 64;
    #else
        uint64_t ralo = (uint32_t)ra, rahi = ra >> 32;
        uint64_t rblo = (uint32_t)rb, rbhi = rb >> 32;
        uint64_t pp0 = ralo * rblo;
        uint64_t pp1 = rahi * rblo;
        uint64_t pp2 = ralo * rbhi;
        uint64_t pp3 = rahi * rbhi;
        uint64_t c = ((uint32_t)pp1) + ((uint32_t)pp2) + (pp0 >> 32);
        phi = pp3 + (pp2 >> 32) + (pp1 >> 32) + (c >> 32);
        plo = (c << 32) | ((uint32_t)pp0);
    #endif
        return std::make_tuple(plo, phi);
    }

    /* Compute 128-bit product of 64-bit signed integer multiplication */
    inline std::tuple<uint64_t, int64_t>
    multiply(int64_t ra, int64_t rb) const
    {
        uint64_t plo, phi;
    #if defined(__SIZEOF_INT128__)
        __int128_t prod = (__int128_t)ra * rb;
        plo = prod;
        phi = prod >> 64;
    #else
        std::tie(plo, phi) = multiply((uint64_t)ra, (uint64_t)rb);
        if (rb < 0) phi -= (uint64_t)ra;
        if (ra < 0) phi -= (uint64_t)rb;
    #endif
        return std::make_tuple(plo, (int64_t)phi);
    }

    /* Compute 128-bit result of 64-bit unsigned integer multiplication
       followed by addition */
    inline std::tuple<uint64_t, uint64_t>
    multiplyAdd(uint64_t ra, uint64_t rb, uint64_t rc) const
    {
        uint64_t rlo, rhi;
    #if defined(__SIZEOF_INT128__)
        __uint128_t res = ((__uint128_t)ra * rb) + rc;
        rlo = res;
        rhi = res >> 64;
    #else
        uint64_t plo, phi;
        std::tie(plo, phi) = multiply(ra, rb);
        std::tie(rlo, rhi) = add(plo, phi, rc);
    #endif
        return std::make_tuple(rlo, rhi);
    }

    /* Compute 128-bit result of 64-bit signed integer multiplication
       followed by addition */
    inline std::tuple<uint64_t, int64_t>
    multiplyAdd(int64_t ra, int64_t rb, int64_t rc) const
    {
        uint64_t rlo;
        int64_t rhi;
    #if defined(__SIZEOF_INT128__)
        __int128_t res = (__int128_t)ra * rb + rc;
        rlo = res;
        rhi = res >> 64;
    #else
        uint64_t plo;
        int64_t phi;
        std::tie(plo, phi) = multiply(ra, rb);
        std::tie(rlo, rhi) = add(plo, phi, rc);
    #endif
        return std::make_tuple(rlo, rhi);
    }

    /* Compute overflow, 64-bit quotient and 64-bit remainder of
       128-bit by 64-bit unsigned integer division */
    inline std::tuple<bool, uint64_t, uint64_t>
    divide(uint64_t ralo, uint64_t rahi, uint64_t rb) const
    {
        bool ov;
        uint64_t q, r;
    #if defined(__SIZEOF_INT128__)
        if (rb == 0) {
            ov = true;
        } else {
            __uint128_t ra = ((__uint128_t)rahi << 64) | ralo;
            __uint128_t res = ra / rb;
            q = res;
            r = ra % rb;
            ov = res > UINT64_MAX;
        }
    #else
        uint64_t c = 0;

        if (rb == 0) {
            ov = true;
        } else if (rahi == 0) {
            q  = ralo / rb;
            r = ralo % rb;
            ov = false;
        } else if (rahi >= rb) {
            ov = true;
        } else {
            for (int i = 0; i < 64; ++i) {
                c = rahi >> 63;
                rahi = (rahi << 1) | (ralo >> 63);
                if (c || (rahi >= rb)) {
                    rahi -= rb;
                    c = 1;
                } else {
                    c = 0;
                }
                ralo = (ralo << 1) | c;
            }
            q = ralo;
            r = rahi;
            ov = false;
        }
    #endif
        return std::make_tuple(ov, q, r);
    }

    /* Compute overflow, 64-bit quotient and 64-bit remainder of
       128-bit by 64-bit signed integer division */
    inline std::tuple<bool, int64_t, int64_t>
    divide(uint64_t ralo, int64_t rahi, int64_t rb) const
    {
        bool ov;
        int64_t q, r;
    #if defined(__SIZEOF_INT128__)
        if (rb == 0) {
            ov = true;
        } else {
            __int128_t ra = ((__int128_t)rahi << 64) | ralo;
            __int128_t res = ra / rb;
            q = res;
            r = ra % rb;
            ov = res != q;
        }
    #else
        bool raneg = rahi < 0;
        bool rbneg = rb < 0;

        if (raneg) {
            ralo = ~(ralo);
            rahi = ~(rahi);
            if (ralo == -1ULL) {
                ralo = 0;
                rahi++;
            } else {
                ralo++;
            }
        }

        if (rbneg) rb = -rb;
        std::tie(ov, q, r) = divide(ralo, (uint64_t)rahi, (uint64_t)rb);
        if (raneg ^ rbneg) q = -q;
        if (raneg) r = -r;
        if (!ov) ov = ((q < 0) ^ (raneg ^ rbneg));
    #endif
        return std::make_tuple(ov, q, r);
    }

    std::string generateDisassembly(
            Addr pc, const Loader::SymbolTable *symtab) const override;
};


/**
 * Class for integer immediate arithmetic operations.
 */
class IntImmArithOp : public IntArithOp
{
  protected:

    int32_t simm;

    /// Constructor
    IntImmArithOp(const char *mnem, MachInst _machInst, OpClass __opClass)
      : IntArithOp(mnem, _machInst, __opClass),
        simm((int16_t)machInst.si)
    {
    }

    std::string generateDisassembly(
            Addr pc, const Loader::SymbolTable *symtab) const override;
};


/**
 * Class for integer arithmetic operations with displacement.
 */
class IntDispArithOp : public IntArithOp
{
  protected:

    int32_t disp;

    /// Constructor
    IntDispArithOp(const char *mnem, MachInst _machInst, OpClass __opClass)
      : IntArithOp(mnem, _machInst, __opClass),
        disp((int16_t)((machInst.d0 << 6) | (machInst.d1 << 1) | machInst.d2))
    {
    }

    std::string generateDisassembly(
            Addr pc, const Loader::SymbolTable *symtab) const override;
};


/**
 * Class for integer compare operations.
 */
class IntCompOp : public IntOp
{
  protected:

    uint32_t length;
    uint32_t field;

    /// Constructor
    IntCompOp(const char *mnem, MachInst _machInst, OpClass __opClass)
      : IntOp(mnem, _machInst, __opClass),
        length(machInst.l),
        field(machInst.bf)
    {
    }

    std::string generateDisassembly(
            Addr pc, const Loader::SymbolTable *symtab) const override;
};


/**
 * Class for integer immediate compare operations.
 */
class IntImmCompOp : public IntCompOp
{
  protected:

    int32_t simm;

    /// Constructor
    IntImmCompOp(const char *mnem, MachInst _machInst, OpClass __opClass)
      : IntCompOp(mnem, _machInst, __opClass),
        simm((int16_t)machInst.si)
    {
    }

    std::string generateDisassembly(
            Addr pc, const Loader::SymbolTable *symtab) const override;
};


/**
 * Class for integer immediate compare logical operations.
 */
class IntImmCompLogicOp : public IntCompOp
{
  protected:

    uint32_t uimm;

    /// Constructor
    IntImmCompLogicOp(const char *mnem, MachInst _machInst, OpClass __opClass)
      : IntCompOp(mnem, _machInst, __opClass),
        uimm(machInst.ui)
    {
    }

    std::string generateDisassembly(
            Addr pc, const Loader::SymbolTable *symtab) const override;
};


/**
 * Class for integer logical operations.
 */
class IntLogicOp : public IntOp
{
  protected:

    /// Constructor
    IntLogicOp(const char *mnem, MachInst _machInst, OpClass __opClass)
      : IntOp(mnem, _machInst, __opClass)
    {
    }

    /* Compute the number of consecutive zero bits starting from the
       leftmost bit and moving right in a 32-bit integer */
    inline int
    findLeadingZeros(uint32_t rs) const
    {
        if (rs) {
    #if defined(__GNUC__) || (defined(__clang__) && \
                              __has_builtin(__builtin_clz))
            return __builtin_clz(rs);
    #else
            return 31 - findMsbSet(rs);
    #endif
        } else {
            return 32;
        }
    }

    /* Compute the number of consecutive zero bits starting from the
       leftmost bit and moving right in a 64-bit integer */
    inline int
    findLeadingZeros(uint64_t rs) const
    {
        if (rs) {
    #if defined(__GNUC__) || (defined(__clang__) && \
                              __has_builtin(__builtin_clzll))
            return __builtin_clzll(rs);
    #else
            return 63 - findMsbSet(rs);
    #endif
        } else {
            return 64;
        }
    }

    /* Compute the number of consecutive zero bits starting from the
       rightmost bit and moving left in a 32-bit integer */
    inline int
    findTrailingZeros(uint32_t rs) const
    {
        if (rs) {
    #if defined(__GNUC__) || (defined(__clang__) && \
                              __has_builtin(__builtin_ctz))
            return __builtin_ctz(rs);
    #else
            return findLsbSet(rs);
    #endif
        } else {
            return 32;
        }
    }

    /* Compute the number of consecutive zero bits starting from the
       rightmost bit and moving left in a 64-bit integer */
    inline int
    findTrailingZeros(uint64_t rs) const
    {
        if (rs) {
    #if defined(__GNUC__) || (defined(__clang__) && \
                              __has_builtin(__builtin_ctzll))
            return __builtin_ctzll(rs);
    #else
            return findLsbSet(rs);
    #endif
        } else {
            return 64;
        }
    }

    std::string generateDisassembly(
            Addr pc, const Loader::SymbolTable *symtab) const override;
};


/**
 * Class for integer immediate logical operations.
 */
class IntImmLogicOp : public IntLogicOp
{
  protected:

    uint32_t uimm;

    /// Constructor
    IntImmLogicOp(const char *mnem, MachInst _machInst, OpClass __opClass)
      : IntLogicOp(mnem, _machInst, __opClass),
        uimm(machInst.si)
    {
    }

    std::string generateDisassembly(
            Addr pc, const Loader::SymbolTable *symtab) const override;
};


/**
 * Class for integer operations with a shift value obtained from
 * a register or an instruction field.
 */
class IntShiftOp : public IntOp
{
  protected:

    uint32_t shift;

    /// Constructor
    IntShiftOp(const char *mnem, MachInst _machInst, OpClass __opClass)
      : IntOp(mnem, _machInst, __opClass),
        shift(machInst.sh)
    {
    }

    std::string generateDisassembly(
            Addr pc, const Loader::SymbolTable *symtab) const override;
};


/**
 * Class for integer shift operations with a shift value obtained from
 * a register or by concatenating immediates.
 */
class IntConcatShiftOp : public IntOp
{
  protected:

    uint32_t shift;

    /// Constructor
    IntConcatShiftOp(const char *mnem, MachInst _machInst, OpClass __opClass)
      : IntOp(mnem, _machInst, __opClass),
        shift(((uint32_t)machInst.shn << 5) | machInst.sh)
    {
    }

    std::string generateDisassembly(
            Addr pc, const Loader::SymbolTable *symtab) const override;
};


/**
 * Class for integer rotate operations with a shift amount obtained
 * from a register or an immediate and the first and last bits of a
 * mask obtained from immediates.
 */
class IntRotateOp : public IntShiftOp
{
  protected:

    uint32_t maskBeg;
    uint32_t maskEnd;

    /// Constructor
    IntRotateOp(const char *mnem, MachInst _machInst, OpClass __opClass)
      : IntShiftOp(mnem, _machInst, __opClass),
        maskBeg(machInst.mb),
        maskEnd(machInst.me)
    {
    }

    inline uint64_t
    rotate(uint32_t rs, uint32_t sh) const
    {
        uint64_t res;
        sh = sh & 0x1f;
        res = rs;
        res = (res << 32) | res;
        res = (res << sh) | (res >> (32 - sh));
        return res;
    }

    inline uint64_t
    bitmask(uint32_t mb, uint32_t me) const
    {
        mb = mb & 0x1f;
        me = me & 0x1f;
        if (mb <= me) {
            return mask(31 - mb, 31 - me);
        } else {
            return ~mask(31 - (me + 1), 31 - (mb - 1));
        }
    }

    std::string generateDisassembly(
            Addr pc, const Loader::SymbolTable *symtab) const override;
};


/**
 * Class for integer rotate operations with a shift amount obtained
 * from a register or by concatenating immediate fields and the first
 * and last bits of a mask obtained by concatenating immediate fields.
 */
class IntConcatRotateOp : public IntConcatShiftOp
{
  protected:

    uint32_t maskBeg;
    uint32_t maskEnd;

    /// Constructor
    IntConcatRotateOp(const char *mnem, MachInst _machInst, OpClass __opClass)
      : IntConcatShiftOp(mnem, _machInst, __opClass),
        maskBeg(((uint32_t)machInst.mbn << 5) | machInst.mb),
        maskEnd(((uint32_t)machInst.men << 5) | machInst.mb)
    {
    }

    inline uint64_t
    rotate(uint64_t rs, uint32_t sh) const
    {
        sh = sh & 0x3f;
        return (rs << sh) | (rs >> (64 - sh));
    }

    inline uint64_t
    bitmask(uint32_t mb, uint32_t me) const
    {
        mb = mb & 0x3f;
        me = me & 0x3f;
        if (mb <= me) {
            return mask(63 - mb, 63 - me);
        } else {
            return ~mask(63 - (me + 1), 63 - (mb - 1));
        }
    }

    std::string generateDisassembly(
            Addr pc, const Loader::SymbolTable *symtab) const override;
};

} // namespace PowerISA

#endif //__ARCH_POWER_INSTS_INTEGER_HH__
