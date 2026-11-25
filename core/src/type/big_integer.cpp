/*
 * Â© 2018 and later: Unicode, Inc. and others.
 * Copyright 2010 the V8 project authors. All rights reserved.
 *
 * Project license notice for OmniOperator
 * Existing file-specific copyright and license terms continue to apply.
 *
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 *
 * Third-party license and source notices
 *
 * License & terms of use: http://www.unicode.org/copyright.html
 *
 * From the double-conversion library. Original license:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Google Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
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


#include <algorithm>
#include "big_integer.h"

namespace omniruntime::type {
BigInteger::LimbType &BigInteger::RawBigit(const int index)
{
    return dataBuffer_[index];
}

const BigInteger::LimbType &BigInteger::RawBigit(const int index) const
{
    return dataBuffer_[index];
}

template<typename S>
static int BitSize(const S value)
{
    (void)value;  // Mark variable as used.
    return 8 * sizeof(value);
}

// Guaranteed to lie in one Bigit.
void BigInteger::AssignUInt16(const uint16_t value)
{
    Zero();
    if (value > 0) {
        RawBigit(0) = value;
        usedData_ = 1;
    }
}

void BigInteger::AssignUInt64(uint64_t value)
{
    Zero();
    for (int i = 0; value > 0; ++i) {
        RawBigit(i) = value & DATA_MASK;
        value >>= DATA_SIZE;
        ++usedData_;
    }
}

static uint64_t ReadUInt64(const std::string_view buffer, const int from, const int digitsToRead)
{
    uint64_t result = 0;
    for (int i = from; i < from + digitsToRead; ++i) {
        const int digit = buffer[i] - '0';
        result = result * 10 + digit;
    }
    return result;
}

BigInteger::BigInteger(const std::string_view value)
{
    // 2^64 = 18446744073709551616 > 10^19
    static const int MAX_DECIMAL64_DIGITS = 19;
    int length = static_cast<int>(value.length());
    int pos = 0;
    // Let's just say that each digit needs 4 bits.
    while (length >= MAX_DECIMAL64_DIGITS) {
        const uint64_t digits = ReadUInt64(value, pos, MAX_DECIMAL64_DIGITS);
        pos += MAX_DECIMAL64_DIGITS;
        length -= MAX_DECIMAL64_DIGITS;
        MultiplyByPowerOfTen(MAX_DECIMAL64_DIGITS);
        AddUInt64(digits);
    }
    const uint64_t digits = ReadUInt64(value, pos, length);
    MultiplyByPowerOfTen(length);
    AddUInt64(digits);
    Clamp();
}

void BigInteger::AddUInt64(const uint64_t operand)
{
    if (operand == 0) {
        return;
    }
    BigInteger other;
    other.AssignUInt64(operand);
    AddBigInteger(other);
}

void BigInteger::AddBigInteger(const BigInteger &other)
{
    // If this has a greater exponent than other append zero-bigits to this.
    // After this call e_ <= other.e_.
    Align(other);

    // There are two possibilities:
    //   ddddddddddd 0000  (where the 0s represent d's exponent)
    //     aaaaa 00000000
    //   ----------------
    //   aaaaaaaaaaa 0000
    // or
    //    dddddddddd 0000
    //  aaaaaaaaa 0000000
    //  -----------------
    //  aaaaaaaaaaaa 0000
    // In both cases we might need a carry bigit.
    EnsureCapacity(1 + (std::max)(BigIntegerLength(), other.BigIntegerLength()) - exponent_);
    LimbType carry = 0;
    int bigitPos = other.exponent_ - exponent_;
    for (int i = usedData_; i < bigitPos; ++i) {
        RawBigit(i) = 0;
    }
    for (int i = 0; i < other.usedData_; ++i) {
        const LimbType my = (bigitPos < usedData_) ? RawBigit(bigitPos) : 0;
        const LimbType sum = my + other.RawBigit(i) + carry;
        RawBigit(bigitPos) = sum & DATA_MASK;
        carry = sum >> DATA_SIZE;
        ++bigitPos;
    }
    while (carry != 0) {
        const LimbType my = (bigitPos < usedData_) ? RawBigit(bigitPos) : 0;
        const LimbType sum = my + carry;
        RawBigit(bigitPos) = sum & DATA_MASK;
        carry = sum >> DATA_SIZE;
        ++bigitPos;
    }
    usedData_ = static_cast<int16_t>(std::max(bigitPos, static_cast<int>(usedData_)));
}

void BigInteger::SubtractBigInteger(const BigInteger &other)
{
    Align(other);

    const int offset = other.exponent_ - exponent_;
    LimbType borrow = 0;
    int i;
    for (i = 0; i < other.usedData_; ++i) {
        const LimbType difference = RawBigit(i + offset) - other.RawBigit(i) - borrow;
        RawBigit(i + offset) = difference & DATA_MASK;
        borrow = difference >> (TYPE_SIZE - 1);
    }
    while (borrow != 0) {
        const LimbType difference = RawBigit(i + offset) - borrow;
        RawBigit(i + offset) = difference & DATA_MASK;
        borrow = difference >> (TYPE_SIZE - 1);
        ++i;
    }
    Clamp();
}

void BigInteger::ShiftLeft(const int32_t shiftAmount)
{
    if (usedData_ == 0) {
        return;
    }
    exponent_ = static_cast<int16_t>(exponent_ + static_cast<int16_t>(shiftAmount / DATA_SIZE));
    const int localShift = shiftAmount % DATA_SIZE;
    EnsureCapacity(usedData_ + 1);
    BigitsShiftLeft(localShift);
}

void BigInteger::MultiplyByUInt32(const uint32_t factor)
{
    if (factor == 1) {
        return;
    }
    if (factor == 0) {
        Zero();
        return;
    }
    if (usedData_ == 0) {
        return;
    }
    // The product of a bigit with the factor is of size DATA_SIZE + 32.
    // Assert that this number + 1 (for the carry) fits into double_limb_type.
    DoubleLimbType carry = 0;
    for (int i = 0; i < usedData_; ++i) {
        const DoubleLimbType product = static_cast<DoubleLimbType>(factor) * RawBigit(i) + carry;
        RawBigit(i) = static_cast<LimbType>(product & DATA_MASK);
        carry = (product >> DATA_SIZE);
    }
    while (carry != 0) {
        EnsureCapacity(usedData_ + 1);
        RawBigit(usedData_) = carry & DATA_MASK;
        usedData_++;
        carry >>= DATA_SIZE;
    }
}

void BigInteger::MultiplyByUInt64(const uint64_t factor)
{
    if (factor == 1) {
        return;
    }
    if (factor == 0) {
        Zero();
        return;
    }
    if (usedData_ == 0) {
        return;
    }
    uint64_t carry = 0;
    const uint64_t low = factor & 0xFFFFFFFF;
    const uint64_t high = factor >> 32;
    for (int i = 0; i < usedData_; ++i) {
        const uint64_t productLow = low * RawBigit(i);
        const uint64_t productHigh = high * RawBigit(i);
        const uint64_t tmp = (carry & DATA_MASK) + productLow;
        RawBigit(i) = tmp & DATA_MASK;
        carry = (carry >> DATA_SIZE) + (tmp >> DATA_SIZE) +
            (productHigh << (32 - DATA_SIZE));
    }
    while (carry != 0) {
        EnsureCapacity(usedData_ + 1);
        RawBigit(usedData_) = carry & DATA_MASK;
        usedData_++;
        carry >>= DATA_SIZE;
    }
}

void BigInteger::MultiplyByPowerOfTen(const int exponent)
{
    static const uint64_t FIVE_27 = DOUBLE_CONVERSION_UINT64_2PART_C(0x6765c793, fa10079d);
    static const uint16_t FIVE_1 = 5;
    static const uint16_t FIVE_2 = FIVE_1 * 5;
    static const uint16_t FIVE_3 = FIVE_2 * 5;
    static const uint16_t FIVE_4 = FIVE_3 * 5;
    static const uint16_t FIVE_5 = FIVE_4 * 5;
    static const uint16_t FIVE_6 = FIVE_5 * 5;
    static const uint32_t FIVE_7 = FIVE_6 * 5;
    static const uint32_t FIVE_8 = FIVE_7 * 5;
    static const uint32_t FIVE_9 = FIVE_8 * 5;
    static const uint32_t FIVE_10 = FIVE_9 * 5;
    static const uint32_t FIVE_11 = FIVE_10 * 5;
    static const uint32_t FIVE_12 = FIVE_11 * 5;
    static const uint32_t FIVE_13 = FIVE_12 * 5;
    static const uint32_t FIVE_1_TO_12[] = {
        FIVE_1, FIVE_2, FIVE_3, FIVE_4, FIVE_5, FIVE_6, FIVE_7, FIVE_8, FIVE_9, FIVE_10, FIVE_11, FIVE_12};

    if (exponent == 0) {
        return;
    }
    if (usedData_ == 0) {
        return;
    }
    // We shift by exponent at the end just before returning.
    int remainingExponent = exponent;
    while (remainingExponent >= 27) {
        MultiplyByUInt64(FIVE_27);
        remainingExponent -= 27;
    }
    while (remainingExponent >= 13) {
        MultiplyByUInt32(FIVE_13);
        remainingExponent -= 13;
    }
    if (remainingExponent > 0) {
        MultiplyByUInt32(FIVE_1_TO_12[remainingExponent - 1]);
    }
    ShiftLeft(exponent);
}

void BigInteger::Square()
{
    const int productLength = 2 * usedData_;
    EnsureCapacity(productLength);

    if (usedData_ >= SQUARE_DATA_SIZE) {
        throw omniruntime::exception::OmniException("Runtime Error", "BigInteger is out of range");
    }
    DoubleLimbType accumulator = 0;
    // First shift the digits, so we don't overwrite them.
    const int copyOffset = usedData_;
    for (int i = 0; i < usedData_; ++i) {
        RawBigit(copyOffset + i) = RawBigit(i);
    }
    // We have two loops to avoid some 'if's in the loop.
    for (int i = 0; i < usedData_; ++i) {
        // Process temporary digit i with power i.
        // The sum of the two indices must be equal to i.
        int index1 = i;
        int index2 = 0;
        // Sum all of the sub-products.
        while (index1 >= 0) {
            const LimbType chunk1 = RawBigit(copyOffset + index1);
            const LimbType chunk2 = RawBigit(copyOffset + index2);
            accumulator += static_cast<DoubleLimbType>(chunk1) * chunk2;
            index1--;
            index2++;
        }
        RawBigit(i) = static_cast<LimbType>(accumulator) & DATA_MASK;
        accumulator >>= DATA_SIZE;
    }
    for (int i = usedData_; i < productLength; ++i) {
        int index1 = usedData_ - 1;
        int index2 = i - index1;
        // Invariant: sum of both indices is again equal to i.
        // Inner loop runs 0 times on last iteration, emptying accumulator.
        while (index2 < usedData_) {
            const LimbType chunk1 = RawBigit(copyOffset + index1);
            const LimbType chunk2 = RawBigit(copyOffset + index2);
            accumulator += static_cast<DoubleLimbType>(chunk1) * chunk2;
            index1--;
            index2++;
        }
        // The overwritten RawBigit(i) will never be read in further loop iterations,
        // because index1 and index2 are always greater
        // than i - usedData_.
        RawBigit(i) = static_cast<LimbType>(accumulator) & DATA_MASK;
        accumulator >>= DATA_SIZE;
    }

    // Don't forget to update the used_digits and the exponent.
    usedData_ = static_cast<int16_t>(productLength);
    exponent_ *= 2;
    Clamp();
}

BigInteger::BigInteger(uint16_t base, const int powerExponent)
{
    if (powerExponent == 0) {
        AssignUInt16(1);
        return;
    }

    Zero();
    int shifts = 0;
    // We expect base to be in range 2-32, and most often to be 10.
    // It does not make much sense to implement different algorithms for counting the bits.
    while ((base & 1) == 0) {
        base >>= 1;
        shifts++;
    }
    int bitSize = 0;
    int tmpBase = base;
    while (tmpBase != 0) {
        tmpBase >>= 1;
        bitSize++;
    }
    const int finalSize = bitSize * powerExponent;
    // 1 extra bigit for the shifting, and one for rounded finalSize.
    EnsureCapacity(finalSize / DATA_SIZE + 2);

    // Left to Right exponentiation.
    int mask = 1;
    while (powerExponent >= mask) mask <<= 1;

    // The mask is now pointing to the bit above the most significant 1-bit of
    // powerExponent.
    // Get rid of first 1-bit;
    mask >>= 2;
    uint64_t thisValue = base;

    bool delayedMultiplication = false;
    const uint64_t max32bits = 0xFFFFFFFF;
    while (mask != 0 && thisValue <= max32bits) {
        thisValue = thisValue * thisValue;
        // Verify that there is enough space in this_value to perform the
        // multiplication.  The first bitSize bits must be 0.
        if ((powerExponent & mask) != 0) {
            const uint64_t baseBitsMask = ~((static_cast<uint64_t>(1) << (64 - bitSize)) - 1);
            const bool highBitsZero = (thisValue & baseBitsMask) == 0;
            if (highBitsZero) {
                thisValue *= base;
            } else {
                delayedMultiplication = true;
            }
        }
        mask >>= 1;
    }
    AssignUInt64(thisValue);
    if (delayedMultiplication) {
        MultiplyByUInt32(base);
    }

    // Now do the same thing as a BigInteger.
    while (mask != 0) {
        Square();
        if ((powerExponent & mask) != 0) {
            MultiplyByUInt32(base);
        }
        mask >>= 1;
    }

    // And finally add the saved shifts.
    ShiftLeft(shifts * powerExponent);
}

// Precondition: this/other < 16bit.
uint16_t BigInteger::DivideModuloIntBigInteger(const BigInteger &other)
{
    // Easy case: if we have less digits than the divisor than the result is 0.
    // Note: this handles the case where this == 0, too.
    if (BigIntegerLength() < other.BigIntegerLength()) {
        return 0;
    }
    Align(other);
    uint16_t result = 0;

    // Start by removing multiples of 'other' until both numbers have the same
    // number of digits.
    while (BigIntegerLength() > other.BigIntegerLength()) {
        // This naive approach is extremely inefficient if `this` divided by other
        // is big. This function is implemented for doubleToString where
        // the result should be small (less than 10).
        // Remove the multiples of the first digit.
        // Example this = 23 and other equals 9. -> Remove 2 multiples.
        result += static_cast<uint16_t>(RawBigit(usedData_ - 1));
        SubtractTimes(other, static_cast<int32_t>(RawBigit(usedData_ - 1)));
    }

    // Both BigIntegers are at the same length now.
    // Since other has more than 0 digits we know that the access to
    // RawBigit(usedData_ - 1) is safe.
    const LimbType thisBigit = RawBigit(usedData_ - 1);
    const LimbType otherBigit = other.RawBigit(other.usedData_ - 1);

    if (other.usedData_ == 1) {
        // Shortcut for easy (and common) case.
        int quotient = static_cast<int32_t>(thisBigit / otherBigit);
        RawBigit(usedData_ - 1) = thisBigit - otherBigit * quotient;
        result += static_cast<uint16_t>(quotient);
        Clamp();
        return result;
    }

    const int divisionEstimate = static_cast<int32_t>(thisBigit / (otherBigit + 1));
    result += static_cast<uint16_t>(divisionEstimate);
    SubtractTimes(other, divisionEstimate);

    if (otherBigit * (divisionEstimate + 1) > thisBigit) {
        // No need to even try to subtract. Even if other's remaining digits were 0
        // another subtraction would be too much.
        return result;
    }

    while (LessEqual(other, *this)) {
        SubtractBigInteger(other);
        result++;
    }
    return result;
}

template<typename S>
static int SizeInHexChars(S number)
{
    int result = 0;
    while (number != 0) {
        number >>= 4;
        result++;
    }
    return result;
}

static char HexCharOfValue(const int32_t value)
{
    if (value < 10) {
        return static_cast<char>(value + '0');
    }
    return static_cast<char>(value - 10 + 'A');
}

std::string BigInteger::ToHexString() const
{
    static const int HEX_CHARS_PER_BIGIT = DATA_SIZE / 4;

    if (usedData_ == 0) {
        return "0";
    }
    // We add 1 for the terminating '\0' character.
    const int neededChars = (BigIntegerLength() - 1) * HEX_CHARS_PER_BIGIT +
        SizeInHexChars(RawBigit(usedData_ - 1)) + 1;

    char *buffer = new char[neededChars];

    int stringIndex = neededChars - 1;
    buffer[stringIndex--] = '\0';
    for (int i = 0; i < exponent_; ++i) {
        for (int j = 0; j < HEX_CHARS_PER_BIGIT; ++j) {
            buffer[stringIndex--] = '0';
        }
    }
    for (int i = 0; i < usedData_ - 1; ++i) {
        LimbType currentData = RawBigit(i);
        for (int j = 0; j < HEX_CHARS_PER_BIGIT; ++j) {
            buffer[stringIndex--] = HexCharOfValue(static_cast<int32_t>(currentData & 0xF));
            currentData >>= 4;
        }
    }
    // And finally the last bigit.
    LimbType mostSignificantData = RawBigit(usedData_ - 1);
    while (mostSignificantData != 0) {
        buffer[stringIndex--] = HexCharOfValue(static_cast<int32_t>(mostSignificantData & 0xF));
        mostSignificantData >>= 4;
    }

    std::string result(buffer);
    delete[] buffer;
    return result;
}

BigInteger::LimbType BigInteger::BigitOrZero(const int index) const
{
    if (index >= BigIntegerLength()) {
        return 0;
    }
    if (index < exponent_) {
        return 0;
    }
    return RawBigit(index - exponent_);
}

int BigInteger::Compare(const BigInteger &a, const BigInteger &b)
{
    const int lengthA = a.BigIntegerLength();
    const int lengthB = b.BigIntegerLength();
    if (lengthA < lengthB) {
        return -1;
    }
    if (lengthA > lengthB) {
        return +1;
    }
    for (int i = lengthA - 1; i >= (std::min)(a.exponent_, b.exponent_); --i) {
        const LimbType dataA = a.BigitOrZero(i);
        const LimbType dataB = b.BigitOrZero(i);
        if (dataA < dataB) {
            return -1;
        }
        if (dataA > dataB) {
            return +1;
        }
        // Otherwise they are equal up to this digit. Try the next digit.
    }
    return 0;
}

int BigInteger::PlusCompare(const BigInteger &a, const BigInteger &b, const BigInteger &c)
{
    if (a.BigIntegerLength() < b.BigIntegerLength()) {
        return PlusCompare(b, a, c);
    }
    if (a.BigIntegerLength() + 1 < c.BigIntegerLength()) {
        return -1;
    }
    if (a.BigIntegerLength() > c.BigIntegerLength()) {
        return +1;
    }
    // The exponent encodes 0-bigits. So if there are more 0-digits in 'a' than
    // 'b' has digits, then the bigit-length of 'a'+'b' must be equal to the one
    // of 'a'.
    if (a.exponent_ >= b.BigIntegerLength() && a.BigIntegerLength() < c.BigIntegerLength()) {
        return -1;
    }

    LimbType borrow = 0;
    // Starting at minExponent all digits are == 0. So no need to compare them.
    const int minExponent = (std::min)((std::min)(a.exponent_, b.exponent_), c.exponent_);
    for (int i = c.BigIntegerLength() - 1; i >= minExponent; --i) {
        const LimbType dataA = a.BigitOrZero(i);
        const LimbType dataB = b.BigitOrZero(i);
        const LimbType dataC = c.BigitOrZero(i);
        const LimbType sum = dataA + dataB;
        if (sum > dataC + borrow) {
            return +1;
        } else {
            borrow = dataC + borrow - sum;
            if (borrow > 1) {
                return -1;
            }
            borrow <<= DATA_SIZE;
        }
    }
    if (borrow == 0) {
        return 0;
    }
    return -1;
}

void BigInteger::Clamp()
{
    while (usedData_ > 0 && RawBigit(usedData_ - 1) == 0) {
        usedData_--;
    }
    if (usedData_ == 0) {
        // Zero.
        exponent_ = 0;
    }
}

void BigInteger::Align(const BigInteger &other)
{
    if (exponent_ > other.exponent_) {
        const int32_t zeroBigits = exponent_ - other.exponent_;
        EnsureCapacity(usedData_ + zeroBigits);
        for (int i = usedData_ - 1; i >= 0; --i) {
            RawBigit(i + zeroBigits) = RawBigit(i);
        }
        for (int i = 0; i < zeroBigits; ++i) {
            RawBigit(i) = 0;
        }
        usedData_ = static_cast<int16_t>(usedData_ + zeroBigits);
        exponent_ = static_cast<int16_t>(exponent_ - zeroBigits);
    }
}

void BigInteger::BigitsShiftLeft(const int shiftCount)
{
    LimbType carry = 0;
    for (int i = 0; i < usedData_; ++i) {
        const LimbType newCarry = RawBigit(i) >> (DATA_SIZE - shiftCount);
        RawBigit(i) = ((RawBigit(i) << shiftCount) + carry) & DATA_MASK;
        carry = newCarry;
    }
    if (carry != 0) {
        RawBigit(usedData_) = carry;
        usedData_++;
    }
}

void BigInteger::SubtractTimes(const BigInteger &other, const int factor)
{
    if (factor < 3) {
        for (int i = 0; i < factor; ++i) {
            SubtractBigInteger(other);
        }
        return;
    }
    LimbType borrow = 0;
    const int exponentDiff = other.exponent_ - exponent_;
    for (int i = 0; i < other.usedData_; ++i) {
        const DoubleLimbType product = static_cast<DoubleLimbType>(factor) * other.RawBigit(i);
        const DoubleLimbType remove = borrow + product;
        const LimbType difference = RawBigit(i + exponentDiff) - (remove & DATA_MASK);
        RawBigit(i + exponentDiff) = difference & DATA_MASK;
        borrow = static_cast<LimbType>((difference >> (TYPE_SIZE - 1)) +
            (remove >> DATA_SIZE));
    }
    for (int i = other.usedData_ + exponentDiff; i < usedData_; ++i) {
        if (borrow == 0) {
            return;
        }
        const LimbType difference = RawBigit(i) - borrow;
        RawBigit(i) = difference & DATA_MASK;
        borrow = difference >> (TYPE_SIZE - 1);
    }
    Clamp();
}
}