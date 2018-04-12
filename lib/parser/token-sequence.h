#ifndef FORTRAN_PARSER_TOKEN_SEQUENCE_H_
#define FORTRAN_PARSER_TOKEN_SEQUENCE_H_

// A buffer class capable of holding a contiguous sequence of characters
// and a partitioning thereof into preprocessing tokens, along with their
// associated provenances.

#include "char-block.h"
#include "provenance.h"
#include <cstddef>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace Fortran {
namespace parser {

// Buffers a contiguous sequence of characters that has been partitioned into
// a sequence of preprocessing tokens with provenances.
class TokenSequence {
public:
  TokenSequence() {}
  TokenSequence(const TokenSequence &that) { Put(that); }
  TokenSequence(
      const TokenSequence &that, std::size_t at, std::size_t count = 1) {
    Put(that, at, count);
  }
  TokenSequence(TokenSequence &&that)
    : start_{std::move(that.start_)}, nextStart_{that.nextStart_},
      char_{std::move(that.char_)}, provenances_{std::move(that.provenances_)} {
  }
  TokenSequence(const std::string &s, Provenance p) { Put(s, p); }

  TokenSequence &operator=(const TokenSequence &that) {
    clear();
    Put(that);
    return *this;
  }
  TokenSequence &operator=(TokenSequence &&that) {
    start_ = std::move(that.start_);
    nextStart_ = that.nextStart_;
    char_ = std::move(that.char_);
    return *this;
  }

  bool empty() const { return start_.empty(); }
  void clear();
  void pop_back();
  void shrink_to_fit();

  std::size_t SizeInTokens() const { return start_.size(); }
  std::size_t SizeInChars() const { return char_.size(); }

  CharBlock ToCharBlock() const { return {&char_[0], char_.size()}; }
  std::string ToString() const { return ToCharBlock().ToString(); }

  CharBlock TokenAt(std::size_t token) const {
    return {&char_[start_.at(token)], TokenBytes(token)};
  }

  char CharAt(std::size_t j) const { return char_.at(j); }

  void PutNextTokenChar(char ch, Provenance provenance) {
    char_.emplace_back(ch);
    provenances_.Put({provenance, 1});
  }

  void CloseToken() {
    start_.emplace_back(nextStart_);
    nextStart_ = char_.size();
  }

  void ReopenLastToken() {
    nextStart_ = start_.back();
    start_.pop_back();
  }

  void Put(const TokenSequence &);
  void Put(const TokenSequence &, ProvenanceRange);
  void Put(const TokenSequence &, std::size_t at, std::size_t tokens = 1);
  void Put(const char *, std::size_t, Provenance);
  void Put(const CharBlock &, Provenance);
  void Put(const std::string &, Provenance);
  void Put(const std::stringstream &, Provenance);
  Provenance GetTokenProvenance(
      std::size_t token, std::size_t offset = 0) const;
  ProvenanceRange GetTokenProvenanceRange(
      std::size_t token, std::size_t offset = 0) const;
  ProvenanceRange GetIntervalProvenanceRange(
      std::size_t token, std::size_t tokens = 1) const;
  ProvenanceRange GetProvenanceRange() const;

  char *GetMutableCharData() { return &char_[0]; }
  TokenSequence &ToLowerCase();
  void Emit(CookedSource *) const;

private:
  std::size_t TokenBytes(std::size_t token) const {
    return (token + 1 >= start_.size() ? char_.size() : start_[token + 1]) -
        start_[token];
  }

  std::vector<std::size_t> start_;
  std::size_t nextStart_{0};
  std::vector<char> char_;
  OffsetToProvenanceMappings provenances_;
};
}  // namespace parser
}  // namespace Fortran
#endif  // FORTRAN_PARSER_TOKEN_SEQUENCE_H_