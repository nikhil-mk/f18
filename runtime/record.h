//===-- runtime/record.h ----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// Record file semantics for both external and internal units

#ifndef FORTRAN_RUNTIME_RECORD_H_
#define FORTRAN_RUNTIME_RECORD_H_

#include "io-error.h"
#include <cinttypes>
#include <optional>

namespace Fortran::runtime::io {

template<typename UNIT> class RecordFile {
public:
  bool mayRead() const { return mayRead_; }
  bool mayWrite() const { return mayWrite_; }

protected:
  void set_recordLength(std::size_t n) { recordLength_ = n; }
  void set_mayRead(bool yes = true) { mayRead_ = yes; }
  void set_mayWrite(bool yes = true) { mayWrite_ = yes; }

private:
  UNIT &AsUnit() { return static_cast<UNIT &>(*this); }

  // State established at OPEN time
  bool isSequential_{true};  // ACCESS='SEQUENTIAL', otherwise 'DIRECT'
  std::optional<std::size_t> recordLength_;  // occupied when RECL=
  bool mayRead_{true};  // ACTION='READ' or 'READWRITE'
  bool mayWrite_{true};  // ACTION='WRITE' or 'READWRITE'

  // Mutable state
  std::uint64_t nextRecordNumber_{1};
  std::optional<std::uint64_t> currentRecordNumber_;
  std::size_t positionInCurrentRecord_;
  std::optional<std::uint64_t> endfileRecordNumber_;
  bool lastStatementWasWrite_{false};
};
}

#endif  // FORTRAN_RUNTIME_RECORD_H_
