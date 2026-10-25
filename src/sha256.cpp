#include "sha256.h"

namespace Sha256 {

namespace {
constexpr uint32_t kRound[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U,
    0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU,
    0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU,
    0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U,
    0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U,
    0xc67178f2U};

inline uint32_t rotr(uint32_t value, unsigned bits) {
  return (value >> bits) | (value << (32U - bits));
}

void compress(uint32_t state[8], const uint8_t block[64]) {
  uint32_t w[64];
  for (int i = 0; i < 16; ++i) {
    w[i] = (static_cast<uint32_t>(block[i * 4]) << 24U) |
           (static_cast<uint32_t>(block[i * 4 + 1]) << 16U) |
           (static_cast<uint32_t>(block[i * 4 + 2]) << 8U) | static_cast<uint32_t>(block[i * 4 + 3]);
  }
  for (int i = 16; i < 64; ++i) {
    const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3U);
    const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10U);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }

  uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
  uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
  for (int i = 0; i < 64; ++i) {
    const uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
    const uint32_t choose = (e & f) ^ (~e & g);
    const uint32_t temp1 = h + s1 + choose + kRound[i] + w[i];
    const uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
    const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
    const uint32_t temp2 = s0 + majority;
    h = g;
    g = f;
    f = e;
    e = d + temp1;
    d = c;
    c = b;
    b = a;
    a = temp1 + temp2;
  }
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  state[5] += f;
  state[6] += g;
  state[7] += h;
}
}  // namespace

std::string hex(const uint8_t* data, std::size_t length) {
  uint32_t state[8] = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                       0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
  uint8_t block[64];
  std::size_t offset = 0;
  while (length - offset >= 64U) {
    compress(state, data + offset);
    offset += 64U;
  }

  const std::size_t remaining = length - offset;
  for (std::size_t i = 0; i < remaining; ++i) {
    block[i] = data[offset + i];
  }
  block[remaining] = 0x80U;
  std::size_t used = remaining + 1U;
  if (used > 56U) {
    for (std::size_t i = used; i < 64U; ++i) {
      block[i] = 0;
    }
    compress(state, block);
    used = 0;
  }
  for (std::size_t i = used; i < 56U; ++i) {
    block[i] = 0;
  }
  const uint64_t bitLength = static_cast<uint64_t>(length) * 8U;
  for (int i = 0; i < 8; ++i) {
    block[56 + i] = static_cast<uint8_t>(bitLength >> (56 - 8 * i));
  }
  compress(state, block);

  static const char kDigits[] = "0123456789abcdef";
  std::string out;
  out.reserve(64);
  for (uint32_t word : state) {
    for (int shift = 28; shift >= 0; shift -= 4) {
      out.push_back(kDigits[(word >> shift) & 0x0FU]);
    }
  }
  return out;
}

std::string hex(const std::string& data) {
  return hex(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

}  // namespace Sha256
