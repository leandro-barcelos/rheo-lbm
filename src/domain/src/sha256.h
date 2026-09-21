#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

namespace domain::detail {

// Streaming SHA-256 for persisted DEM fingerprints. Words in the hash itself
// use big-endian order; the caller controls the serialization of DEM fields.
class Sha256 {
 public:
  void Update(std::span<unsigned char const> bytes) {
    byte_count_ += bytes.size();
    for (auto byte : bytes) Append(byte);
  }

  // Finalizes this instance; call only once, after all input has been supplied.
  std::array<unsigned char, 32> Finish() {
    auto bits = byte_count_ * 8;
    Append(0x80);
    while (used_ != 56) Append(0);
    for (int shift = 56; shift >= 0; shift -= 8)
      Append(static_cast<unsigned char>(bits >> shift));
    std::array<unsigned char, 32> result{};
    for (std::size_t i = 0; i < state_.size(); ++i)
      for (int j = 0; j < 4; ++j)
        result[i * 4 + j] =
            static_cast<unsigned char>(state_[i] >> (24 - j * 8));
    return result;
  }

 private:
  void Append(unsigned char byte) {
    block_[used_++] = byte;
    if (used_ == block_.size()) {
      Compress();
      used_ = 0;
    }
  }

  void Compress() {
    static constexpr std::array<std::uint32_t, 64> k{
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
        0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
        0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
        0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
        0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    std::array<std::uint32_t, 64> w{};
    for (std::size_t i = 0; i < 16; ++i)
      for (std::size_t j = 0; j < 4; ++j)
        w[i] = (w[i] << 8) | block_[i * 4 + j];
    for (std::size_t i = 16; i < w.size(); ++i) {
      auto x = w[i - 15], y = w[i - 2];
      auto s0 = std::rotr(x, 7) ^ std::rotr(x, 18) ^ (x >> 3);
      auto s1 = std::rotr(y, 17) ^ std::rotr(y, 19) ^ (y >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    auto [a, b, c, d, e, f, g, h] = state_;
    for (std::size_t i = 0; i < w.size(); ++i) {
      auto s1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
      auto t1 = h + s1 + ((e & f) ^ (~e & g)) + k[i] + w[i];
      auto s0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
      auto t2 = s0 + ((a & b) ^ (a & c) ^ (b & c));
      h = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    std::array words{a, b, c, d, e, f, g, h};
    for (std::size_t i = 0; i < state_.size(); ++i) state_[i] += words[i];
  }

  std::array<std::uint32_t, 8> state_{0x6a09e667, 0xbb67ae85, 0x3c6ef372,
                                      0xa54ff53a, 0x510e527f, 0x9b05688c,
                                      0x1f83d9ab, 0x5be0cd19};
  std::array<unsigned char, 64> block_{};
  std::size_t used_ = 0;
  std::uint64_t byte_count_ = 0;
};

}  // namespace domain::detail
