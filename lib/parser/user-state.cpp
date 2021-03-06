//===-- lib/parser/user-state.cpp -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "flang/parser/user-state.h"
#include "stmt-parser.h"
#include "type-parsers.h"
#include "flang/parser/parse-state.h"
#include <optional>

namespace Fortran::parser {

std::optional<Success> StartNewSubprogram::Parse(ParseState &state) {
  if (auto *ustate{state.userState()}) {
    ustate->NewSubprogram();
  }
  return Success{};
}

std::optional<CapturedLabelDoStmt::resultType> CapturedLabelDoStmt::Parse(
    ParseState &state) {
  static constexpr auto parser{statement(indirect(Parser<LabelDoStmt>{}))};
  auto result{parser.Parse(state)};
  if (result) {
    if (auto *ustate{state.userState()}) {
      ustate->NewDoLabel(std::get<Label>(result->statement.value().t));
    }
  }
  return result;
}

std::optional<EndDoStmtForCapturedLabelDoStmt::resultType>
EndDoStmtForCapturedLabelDoStmt::Parse(ParseState &state) {
  static constexpr auto parser{
      statement(indirect(construct<EndDoStmt>("END DO" >> maybe(name))))};
  if (auto enddo{parser.Parse(state)}) {
    if (enddo->label) {
      if (const auto *ustate{state.userState()}) {
        if (ustate->IsDoLabel(enddo->label.value())) {
          return enddo;
        }
      }
    }
  }
  return std::nullopt;
}

std::optional<Success> EnterNonlabelDoConstruct::Parse(ParseState &state) {
  if (auto *ustate{state.userState()}) {
    ustate->EnterNonlabelDoConstruct();
  }
  return {Success{}};
}

std::optional<Success> LeaveDoConstruct::Parse(ParseState &state) {
  if (auto ustate{state.userState()}) {
    ustate->LeaveDoConstruct();
  }
  return {Success{}};
}

std::optional<Name> OldStructureComponentName::Parse(ParseState &state) {
  if (std::optional<Name> n{name.Parse(state)}) {
    if (const auto *ustate{state.userState()}) {
      if (ustate->IsOldStructureComponent(n->source)) {
        return n;
      }
    }
  }
  return std::nullopt;
}

std::optional<DataComponentDefStmt> StructureComponents::Parse(
    ParseState &state) {
  static constexpr auto stmt{Parser<DataComponentDefStmt>{}};
  std::optional<DataComponentDefStmt> defs{stmt.Parse(state)};
  if (defs) {
    if (auto *ustate{state.userState()}) {
      for (const auto &decl : std::get<std::list<ComponentDecl>>(defs->t)) {
        ustate->NoteOldStructureComponent(std::get<Name>(decl.t).source);
      }
    }
  }
  return defs;
}
}
