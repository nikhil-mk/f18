//===-- runtime/tools.h -----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef FORTRAN_RUNTIME_TOOLS_H_
#define FORTRAN_RUNTIME_TOOLS_H_
#include "memory.h"
namespace Fortran::runtime {

class Terminator;

OwningPtr<char> SaveDefaultCharacter(const char *, std::size_t, Terminator &);

// For validating and recognizing default CHARACTER values in a
// case-insensitive manner.  Returns the zero-based index into the
// null-terminated array of upper-case possibilities when the value is valid,
// or -1 when it has no match.
int IdentifyValue(
    const char *value, std::size_t length, const char *possibilities[]);
}
#endif  // FORTRAN_RUNTIME_TOOLS_H_
