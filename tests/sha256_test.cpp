#include "sha256.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

void Check(std::string_view input, std::string_view expected) {
  for (std::size_t chunk : {std::size_t{1}, std::size_t{4}, std::size_t{64},
                            std::max(std::size_t{1}, input.size())}) {
    domain::detail::Sha256 hash;
    for (std::size_t offset = 0; offset < input.size(); offset += chunk) {
      auto part = input.substr(offset, chunk);
      hash.Update(
          {reinterpret_cast<unsigned char const*>(part.data()), part.size()});
    }
    std::string actual;
    for (auto byte : hash.Finish()) {
      actual += "0123456789abcdef"[byte >> 4];
      actual += "0123456789abcdef"[byte & 15];
    }
    if (actual != expected) throw std::runtime_error("SHA-256 mismatch");
  }
}

int main() {
  Check("abc",
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  Check(std::string(1000000, 'a'),
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
  // Independent reference digests from Python hashlib; exercise padding and
  // compression boundaries, including binary input and incremental updates.
  {
    std::string input;
    for (int i = 0; i < 0; ++i) input += static_cast<char>(i);
    Check(input,
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  }
  {
    std::string input;
    for (int i = 0; i < 55; ++i) input += static_cast<char>(i);
    Check(input,
          "463eb28e72f82e0a96c0a4cc53690c571281131f672aa229e0d45ae59b598b59");
  }
  {
    std::string input;
    for (int i = 0; i < 56; ++i) input += static_cast<char>(i);
    Check(input,
          "da2ae4d6b36748f2a318f23e7ab1dfdf45acdc9d049bd80e59de82a60895f562");
  }
  {
    std::string input;
    for (int i = 0; i < 63; ++i) input += static_cast<char>(i);
    Check(input,
          "29af2686fd53374a36b0846694cc342177e428d1647515f078784d69cdb9e488");
  }
  {
    std::string input;
    for (int i = 0; i < 64; ++i) input += static_cast<char>(i);
    Check(input,
          "fdeab9acf3710362bd2658cdc9a29e8f9c757fcf9811603a8c447cd1d9151108");
  }
  {
    std::string input;
    for (int i = 0; i < 65; ++i) input += static_cast<char>(i);
    Check(input,
          "4bfd2c8b6f1eec7a2afeb48b934ee4b2694182027e6d0fc075074f2fabb31781");
  }
  {
    std::string input;
    for (int i = 0; i < 119; ++i) input += static_cast<char>(i);
    Check(input,
          "da18797ed7c3a777f0847f429724a2d8cd5138e6ed2895c3fa1a6d39d18f7ec6");
  }
  {
    std::string input;
    for (int i = 0; i < 120; ++i) input += static_cast<char>(i);
    Check(input,
          "f52b23db1fbb6ded89ef42a23ce0c8922c45f25c50b568a93bf1c075420bbb7c");
  }
  {
    std::string input;
    for (int i = 0; i < 128; ++i) input += static_cast<char>(i);
    Check(input,
          "471fb943aa23c511f6f72f8d1652d9c880cfa392ad80503120547703e56a2be5");
  }
  std::cout << "SHA-256 reference vectors passed\n";
}
