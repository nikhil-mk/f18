//===-- runtime/io.cpp ------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "io-api.h"
#include "format.h"
#include "io-stmt.h"
#include "memory.h"
#include "terminator.h"
#include "../lib/common/unsigned-const-division.h"
#include "../lib/decimal/decimal.h"
#include <cstdlib>
#include <memory>

namespace Fortran::runtime::io {

Cookie IONAME(BeginInternalFormattedOutput)(char *internal,
    std::size_t internalLength, const char *format, std::size_t formatLength,
    void ** /*scratchArea*/, std::size_t /*scratchBytes*/,
    const char *sourceFile, int sourceLine) {
  Terminator oom{sourceFile, sourceLine};
  return &New<InternalFormattedIoStatementState<false>>{}(oom, internal,
      internalLength, format, formatLength, sourceFile, sourceLine);
}

bool IONAME(OutputInteger64)(Cookie io, std::int64_t n) {
  DataEdit edit;
  io->GetNext(edit);
  char buffer[66], *end = &buffer[sizeof buffer], *p = end;
  std::uint64_t un{static_cast<std::uint64_t>(n < 0 ? -n : n)};
  int signChars{0};
  switch (edit.descriptor) {
  case 'G':
  case 'I':
    if (n < 0 || (edit.modes.editingFlags & signPlus)) {
      signChars = 1;  // '-' or '+'
    }
    while (un > 0) {
      auto quotient{common::DivideUnsignedBy<std::uint64_t, 10>(un)};
      *--p = '0' + un - 10 * quotient;
      un = quotient;
    }
    break;
  case 'B':
    for (; un > 0; un >>= 1) {
      *--p = '0' + (un & 1);
    }
    break;
  case 'O':
    for (; un > 0; un >>= 3) {
      *--p = '0' + (un & 7);
    }
    break;
  case 'Z':
    for (; un > 0; un >>= 4) {
      int digit = un & 0xf;
      *--p = digit >= 10 ? 'A' + (digit - 10) : '0' + digit;
    }
    break;
  default:
    io->Crash(
        "Data edit descriptor '%c' does not correspond to an INTEGER data item",
        edit.descriptor);
    return false;
  }
  int digits = end - p;
  int leadingZeroes{0};
  if (edit.digits && digits <= *edit.digits) {  // Iw.m
    if (*edit.digits == 0 && n == 0) {
      // Iw.0 with zero value: output field must be blank.  For I0.0
      // and a zero value, emit one blank character.
      signChars = 0;  // in case of SP
      edit.width = std::max(1, edit.width);
    } else {
      leadingZeroes = *edit.digits - digits;
    }
  } else if (n == 0) {
    leadingZeroes = 1;
  }
  int total{signChars + leadingZeroes + digits};
  if (edit.width > 0 && total > edit.width) {
    for (int j{edit.width}; j > 0; --j) {
      if (!io->Emit("*", 1)) {
        return false;
      }
    }
    return true;
  }
  if (total < edit.width) {
    while (total++ < edit.width) {
      if (!io->Emit(" ", 1)) {
        return false;
      }
    }
    return false;
  }
  if (signChars) {
    if (!io->Emit(n < 0 ? "-" : "+", 1)) {
      return false;
    }
  }
  while (leadingZeroes-- > 0) {
    if (!io->Emit("0", 1)) {
      return false;
    }
  }
  return io->Emit(p, digits);
}

bool IONAME(OutputReal64)(Cookie io, double) {
  DataEdit edit;
  io->GetNext(edit);
  char stackBuffer[256], *p = stackBuffer;
  OwningPtr<char> heap;
  if (static_cast<std::size_t>(edit.width) > sizeof stackBuffer) {
    heap.reset(reinterpret_cast<char *>(
        AllocateMemoryOrCrash(*io, static_cast<std::size_t>(edit.width))));
    p = heap.get();
  }
  switch (edit.descriptor) {
  case 'E':
    // TODO: EN, ES, EX
    break;
  // TODO: case 'D';
  // TODO: case 'F';
  // TODO: case 'G';
  // TODO: case 'B';
  // TODO: case 'O';
  // TODO: case 'Z';
  default:
    io->Crash(
        "Data edit descriptor '%c' does not correspond to a REAL data item",
        edit.descriptor);
    return false;
  }
  return false;
}

enum Iostat IONAME(EndIoStatement)(Cookie io) {
  return static_cast<enum Iostat>(io->EndIoStatement());
}
}
