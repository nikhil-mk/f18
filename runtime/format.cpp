//===-- runtime/format.cpp --------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "format.h"
#include "io-stmt.h"
#include "flang/common/format.h"
#include "flang/decimal/decimal.h"
#include <limits>

namespace Fortran::runtime::io {

// Default FormatContext virtual member functions
void FormatContext::Emit(const char *, std::size_t) {
  Crash("Cannot emit data from this FORMAT string");
}
void FormatContext::Emit(const char16_t *, std::size_t) {
  Crash("Cannot emit data from this FORMAT string");
}
void FormatContext::Emit(const char32_t *, std::size_t) {
  Crash("Cannot emit data from this FORMAT string");
}
void FormatContext::HandleSlash(int) {
  Crash("A / control edit descriptor may not appear in this FORMAT string");
}
void FormatContext::HandleAbsolutePosition(int) {
  Crash("A Tn control edit descriptor may not appear in this FORMAT string");
}
void FormatContext::HandleRelativePosition(int) {
  Crash("An nX, TLn, or TRn control edit descriptor may not appear in this "
        "FORMAT string");
}

template<typename CHAR>
FormatControl<CHAR>::FormatControl(Terminator &terminator, const CHAR *format,
    std::size_t formatLength, int maxHeight)
  : maxHeight_{static_cast<std::uint8_t>(maxHeight)}, format_{format},
    formatLength_{static_cast<int>(formatLength)} {
  // The additional two items are for the whole string and a
  // repeated non-parenthesized edit descriptor.
  if (maxHeight > std::numeric_limits<std::int8_t>::max()) {
    terminator.Crash("internal Fortran runtime error: maxHeight %d", maxHeight);
  }
  stack_[0].start = offset_;
  stack_[0].remaining = Iteration::unlimited;  // 13.4(8)
}

template<typename CHAR>
int FormatControl<CHAR>::GetMaxParenthesisNesting(
    Terminator &terminator, const CHAR *format, std::size_t formatLength) {
  using Validator = common::FormatValidator<CHAR>;
  typename Validator::Reporter reporter{
      [&](const common::FormatMessage &message) {
        terminator.Crash(message.text, message.arg);
        return false;  // crashes on error above
      }};
  Validator validator{format, formatLength, reporter};
  validator.Check();
  return validator.maxNesting();
}

template<typename CHAR>
int FormatControl<CHAR>::GetIntField(Terminator &terminator, CHAR firstCh) {
  CHAR ch{firstCh ? firstCh : PeekNext()};
  if (ch != '-' && ch != '+' && (ch < '0' || ch > '9')) {
    terminator.Crash(
        "Invalid FORMAT: integer expected at '%c'", static_cast<char>(ch));
  }
  int result{0};
  bool negate{ch == '-'};
  if (negate) {
    firstCh = '\0';
    ch = PeekNext();
  }
  while (ch >= '0' && ch <= '9') {
    if (result >
        std::numeric_limits<int>::max() / 10 - (static_cast<int>(ch) - '0')) {
      terminator.Crash("FORMAT integer field out of range");
    }
    result = 10 * result + ch - '0';
    if (firstCh) {
      firstCh = '\0';
    } else {
      ++offset_;
    }
    ch = PeekNext();
  }
  if (negate && (result *= -1) > 0) {
    terminator.Crash("FORMAT integer field out of range");
  }
  return result;
}

static void HandleControl(
    FormatContext &context, std::uint16_t &scale, char ch, char next, int n) {
  MutableModes &modes{context.mutableModes()};
  switch (ch) {
  case 'B':
    if (next == 'Z') {
      modes.editingFlags |= blankZero;
      return;
    }
    if (next == 'N') {
      modes.editingFlags &= ~blankZero;
      return;
    }
    break;
  case 'D':
    if (next == 'C') {
      modes.editingFlags |= decimalComma;
      return;
    }
    if (next == 'P') {
      modes.editingFlags &= ~decimalComma;
      return;
    }
    break;
  case 'P':
    if (!next) {
      scale = n;  // kP - decimal scaling by 10**k (TODO)
      return;
    }
    break;
  case 'R':
    switch (next) {
    case 'N': modes.roundingMode = common::RoundingMode::TiesToEven; return;
    case 'Z': modes.roundingMode = common::RoundingMode::ToZero; return;
    case 'U': modes.roundingMode = common::RoundingMode::Up; return;
    case 'D': modes.roundingMode = common::RoundingMode::Down; return;
    case 'C':
      modes.roundingMode = common::RoundingMode::TiesAwayFromZero;
      return;
    default: break;
    }
    break;
  case 'X':
    if (!next) {
      context.HandleRelativePosition(n);
      return;
    }
    break;
  case 'S':
    if (next == 'P') {
      modes.editingFlags |= signPlus;
      return;
    }
    if (!next || next == 'S') {
      modes.editingFlags &= ~signPlus;
      return;
    }
    break;
  case 'T': {
    if (!next) {  // Tn
      context.HandleAbsolutePosition(n);
      return;
    }
    if (next == 'L' || next == 'R') {  // TLn & TRn
      context.HandleRelativePosition(next == 'L' ? -n : n);
      return;
    }
  } break;
  default: break;
  }
  if (next) {
    context.Crash("Unknown '%c%c' edit descriptor in FORMAT", ch, next);
  } else {
    context.Crash("Unknown '%c' edit descriptor in FORMAT", ch);
  }
}

// Locates the next data edit descriptor in the format.
// Handles all repetition counts and control edit descriptors.
// Generally assumes that the format string has survived the common
// format validator gauntlet.
template<typename CHAR>
int FormatControl<CHAR>::CueUpNextDataEdit(FormatContext &context, bool stop) {
  int unlimitedLoopCheck{-1};
  while (true) {
    std::optional<int> repeat;
    bool unlimited{false};
    CHAR ch{Capitalize(GetNextChar(context))};
    while (ch == ',' || ch == ':') {
      // Skip commas, and don't complain if they're missing; the format
      // validator does that.
      if (stop && ch == ':') {
        return 0;
      }
      ch = Capitalize(GetNextChar(context));
    }
    if (ch == '-' || ch == '+' || (ch >= '0' && ch <= '9')) {
      repeat = GetIntField(context, ch);
      ch = GetNextChar(context);
    } else if (ch == '*') {
      unlimited = true;
      ch = GetNextChar(context);
      if (ch != '(') {
        context.Crash("Invalid FORMAT: '*' may appear only before '('");
      }
    }
    if (ch == '(') {
      if (height_ >= maxHeight_) {
        context.Crash("FORMAT stack overflow: too many nested parentheses");
      }
      stack_[height_].start = offset_ - 1;  // the '('
      if (unlimited || height_ == 0) {
        stack_[height_].remaining = Iteration::unlimited;
        unlimitedLoopCheck = offset_ - 1;
      } else if (repeat) {
        if (*repeat <= 0) {
          *repeat = 1;  // error recovery
        }
        stack_[height_].remaining = *repeat - 1;
      } else {
        stack_[height_].remaining = 0;
      }
      ++height_;
    } else if (height_ == 0) {
      context.Crash("FORMAT lacks initial '('");
    } else if (ch == ')') {
      if (height_ == 1) {
        if (stop) {
          return 0;  // end of FORMAT and no data items remain
        }
        context.HandleSlash();  // implied / before rightmost )
      }
      if (stack_[height_ - 1].remaining == Iteration::unlimited) {
        offset_ = stack_[height_ - 1].start + 1;
        if (offset_ == unlimitedLoopCheck) {
          context.Crash(
              "Unlimited repetition in FORMAT lacks data edit descriptors");
        }
      } else if (stack_[height_ - 1].remaining-- > 0) {
        offset_ = stack_[height_ - 1].start + 1;
      } else {
        --height_;
      }
    } else if (ch == '\'' || ch == '"') {
      // Quoted 'character literal'
      CHAR quote{ch};
      auto start{offset_};
      while (offset_ < formatLength_ && format_[offset_] != quote) {
        ++offset_;
      }
      if (offset_ >= formatLength_) {
        context.Crash("FORMAT missing closing quote on character literal");
      }
      ++offset_;
      std::size_t chars{
          static_cast<std::size_t>(&format_[offset_] - &format_[start])};
      if (PeekNext() == quote) {
        // subtle: handle doubled quote character in a literal by including
        // the first in the output, then treating the second as the start
        // of another character literal.
      } else {
        --chars;
      }
      context.Emit(format_ + start, chars);
    } else if (ch == 'H') {
      // 9HHOLLERITH
      if (!repeat || *repeat < 1 || offset_ + *repeat > formatLength_) {
        context.Crash("Invalid width on Hollerith in FORMAT");
      }
      context.Emit(format_ + offset_, static_cast<std::size_t>(*repeat));
      offset_ += *repeat;
    } else if (ch >= 'A' && ch <= 'Z') {
      int start{offset_ - 1};
      CHAR next{Capitalize(PeekNext())};
      if (next < 'A' || next > 'Z') {
        next = '\0';
      }
      if (ch == 'E' ||
          (!next &&
              (ch == 'A' || ch == 'I' || ch == 'B' || ch == 'O' || ch == 'Z' ||
                  ch == 'F' || ch == 'D' || ch == 'G'))) {
        // Data edit descriptor found
        offset_ = start;
        return repeat && *repeat > 0 ? *repeat : 1;
      } else {
        // Control edit descriptor
        if (ch == 'T') {  // Tn, TLn, TRn
          repeat = GetIntField(context);
        }
        HandleControl(context, scale_, static_cast<char>(ch),
            static_cast<char>(next), repeat && *repeat > 0 ? *repeat : 1);
      }
    } else if (ch == '/') {
      context.HandleSlash(repeat && *repeat > 0 ? *repeat : 1);
    } else {
      context.Crash("Invalid character '%c' in FORMAT", static_cast<char>(ch));
    }
  }
}

template<typename CHAR>
void FormatControl<CHAR>::GetNext(
    FormatContext &context, DataEdit &edit, int maxRepeat) {

  // TODO: DT editing

  // Return the next data edit descriptor
  int repeat{CueUpNextDataEdit(context)};
  auto start{offset_};
  edit.descriptor = static_cast<char>(Capitalize(GetNextChar(context)));
  if (edit.descriptor == 'E') {
    edit.variation = static_cast<char>(Capitalize(PeekNext()));
    if (edit.variation >= 'A' && edit.variation <= 'Z') {
      ++offset_;
    } else {
      edit.variation = '\0';
    }
  } else {
    edit.variation = '\0';
  }

  edit.width = GetIntField(context);
  edit.modes = context.mutableModes();
  if (PeekNext() == '.') {
    ++offset_;
    edit.digits = GetIntField(context);
    CHAR ch{PeekNext()};
    if (ch == 'e' || ch == 'E' || ch == 'd' || ch == 'D') {
      ++offset_;
      edit.expoDigits = GetIntField(context);
    } else {
      edit.expoDigits.reset();
    }
  } else {
    edit.digits.reset();
    edit.expoDigits.reset();
  }

  // Handle repeated nonparenthesized edit descriptors
  if (repeat > 1) {
    stack_[height_].start = start;  // after repeat count
    stack_[height_].remaining = repeat;  // full count
    ++height_;
  }
  edit.repeat = 1;
  if (height_ > 1) {
    int start{stack_[height_ - 1].start};
    if (format_[start] != '(') {
      if (stack_[height_ - 1].remaining > maxRepeat) {
        edit.repeat = maxRepeat;
        stack_[height_ - 1].remaining -= maxRepeat;
        offset_ = start;  // repeat same edit descriptor next time
      } else {
        edit.repeat = stack_[height_ - 1].remaining;
        --height_;
      }
    }
  }
}

template<typename CHAR>
void FormatControl<CHAR>::FinishOutput(FormatContext &context) {
  CueUpNextDataEdit(context, true /* stop at colon or end of FORMAT */);
}

template class FormatControl<char>;
template class FormatControl<char16_t>;
template class FormatControl<char32_t>;
}
