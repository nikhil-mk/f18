//===-- runtime/io-stmt.cpp -------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "io-stmt.h"
#include "memory.h"
#include <algorithm>
#include <cstring>

namespace Fortran::runtime::io {

// Defaults
int IoStatementState::EndIoStatement() { return GetIoStat(); }
void IoStatementState::GetNext(DataEdit &, int) {
  Crash("GetNext() called for I/O statement that is not a formatted data "
        "transfer statement");
}
bool IoStatementState::Emit(const char *, std::size_t) {
  Crash("Emit() called for I/O statement that is not a formatted output "
        "statement");
  return false;
}
bool IoStatementState::Emit(const char16_t *, std::size_t) {
  Crash("Emit() called for I/O statement that is not a formatted output "
        "statement");
  return false;
}
bool IoStatementState::Emit(const char32_t *, std::size_t) {
  Crash("Emit() called for I/O statement that is not a formatted output "
        "statement");
  return false;
}
bool IoStatementState::HandleRelativePosition(int) {
  Crash("HandleRelativePosition() called for I/O statement that is not a "
        "formatted data transfer statement");
  return false;
}
bool IoStatementState::HandleAbsolutePosition(int) {
  Crash("HandleAbsolutePosition() called for I/O statement that is not a "
        "formatted data transfer statement");
  return false;
}

int InternalIoStatementState::EndIoStatement() {
  auto result{GetIoStat()};
  if (free_) {
    FreeMemory(this);
  }
  return result;
}

InternalIoStatementState::InternalIoStatementState(
    const char *sourceFile, int sourceLine)
  : IoStatementState(sourceFile, sourceLine) {}

template<bool isInput, typename CHAR>
InternalFormattedIoStatementState<isInput,
    CHAR>::InternalFormattedIoStatementState(Buffer internal,
    std::size_t internalLength, const CHAR *format, std::size_t formatLength,
    const char *sourceFile, int sourceLine)
  : InternalIoStatementState{sourceFile, sourceLine}, FormatContext{},
    internal_{internal}, internalLength_{internalLength}, format_{*this, format,
                                                              formatLength} {
  std::fill_n(internal_, internalLength_, static_cast<CHAR>(' '));
}

template<bool isInput, typename CHAR>
bool InternalFormattedIoStatementState<isInput, CHAR>::Emit(
    const CHAR *data, std::size_t chars) {
  if constexpr (isInput) {
    FormatContext::Emit(data, chars);  // default Crash()
    return false;
  } else if (at_ + chars > internalLength_) {
    SignalEor();
    if (at_ < internalLength_) {
      std::memcpy(
          internal_ + at_, data, (internalLength_ - at_) * sizeof(CHAR));
      at_ = internalLength_;
    }
    return false;
  } else {
    std::memcpy(internal_ + at_, data, chars * sizeof(CHAR));
    at_ += chars;
    return true;
  }
}

template<bool isInput, typename CHAR>
bool InternalFormattedIoStatementState<isInput, CHAR>::HandleAbsolutePosition(
    int n) {
  if (n < 0) {
    n = 0;
  }
  if (static_cast<std::size_t>(n) >= internalLength_) {
    SignalEor();
    return false;
  } else {
    at_ = n;
    return true;
  }
}

template<bool isInput, typename CHAR>
bool InternalFormattedIoStatementState<isInput, CHAR>::HandleRelativePosition(
    int n) {
  if (n < 0) {
    at_ -= std::min(at_, -static_cast<std::size_t>(n));
  } else {
    if (at_ + n > internalLength_) {
      SignalEor();
      at_ = internalLength_;
      return false;
    } else {
      at_ += n;
    }
  }
  return true;
}

template<bool isInput, typename CHAR>
int InternalFormattedIoStatementState<isInput, CHAR>::EndIoStatement() {
  format_.FinishOutput(*this);
  auto result{GetIoStat()};
  if (free_) {
    FreeMemory(this);
  }
  return result;
}

template class InternalFormattedIoStatementState<false>;
}
