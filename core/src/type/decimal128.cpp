/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 * Description: decimal 128 type
 */

#include <array>
#include <iomanip>
#include "decimal128.h"

namespace omniruntime {
namespace type {
const int32_t PRINT_OUT_HEX_WIDTH = 16;

// All comparing operator remains due to template function
bool Decimal128::operator==(const Decimal128 &right) const
{
    return (lowBits == right.lowBits && highBits == right.highBits);
}

bool Decimal128::operator!=(const Decimal128 &right) const { return !operator==(right); }

bool Decimal128::operator<(const Decimal128 &right) const { return Compare(right) == -1; }

bool Decimal128::operator<=(const Decimal128 &right) const { return !operator>(right); }

bool Decimal128::operator>(const Decimal128 &right) const { return Compare(right) == 1; }

bool Decimal128::operator>=(const Decimal128 &right) const { return !operator<(right); }

std::ostream &operator<<(std::ostream &os, const Decimal128 &decimal128)
{
    os << std::hex << "0x" << std::setfill('0') << std::setw(PRINT_OUT_HEX_WIDTH) << decimal128.HighBits()
       << std::setfill('0') << std::setw(PRINT_OUT_HEX_WIDTH) << decimal128.LowBits();
    return os;
}
} // namespace type
} // namespace omniruntime
