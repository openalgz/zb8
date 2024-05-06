#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

// ZB8: Zero Byte Compression

// ZB8 is focused on compressing zero bytes.
// ZB8 has a maximum compression ratio of nearly 22000.
// If your data could be all zeros, ZB8 is fantastic.
// If ZB8 cannot compress your data efficiently
// then it simply sends the original data with an 8 byte header.
// ZB8 will only ever increase your data size by 8 bytes.

namespace zb8
{
   constexpr uint32_t header_size = 8; // encode the uncompressed size
   constexpr uint64_t uncompressed_flag = 0x8000000000000000;
   
   namespace detail
   {
      constexpr uint64_t mark_zeros(const uint64_t chunk) noexcept
      {
         //return (((chunk - 0x0101010101010101u) & ~chunk) & 0x8080808080808080u);
         constexpr uint64_t mask = 0x7F7F7F7F7F7F7F7Full;
         const uint64_t t0 = (((chunk & mask) + mask) | chunk);
         return (t0 & 0x8080808080808080ull) ^ 0x8080808080808080ull;
      }
      
      // https://stackoverflow.com/questions/12181352/high-order-bits-take-them-and-make-a-uint64-t-into-a-uint8-t
      // The multiplications all the bits into the most significant byte,
      // and the shift moves them to the least significant byte.
      // Since multiplication is fast on most modern CPUs this shouldn't be much slower than using assembly.
      constexpr uint8_t extract_msbs(const uint64_t x) noexcept {
         return (x * 0x2040810204081) >> 56;
      }
      
      constexpr int32_t uncompressed_run(const uint8_t value) noexcept {
         // ones indicate uncompressed bytes
         int32_t zeros = 0;
         int32_t uncompressed = 0;
         
         bool uncompressed_start = value & 0b00000001;

          for (int i = 0; i < 8; ++i) {
              // Check if the current bit is 1
              if ((value >> i) & 1) {
                 ++uncompressed;
                 uncompressed_start = true;
              } else {
                 if (uncompressed_start) {
                    break; // end of run
                 }
                 ++zeros;
              }
          }

          return uncompressed;
      }

      constexpr std::array<uint8_t[2], 256> run_table = []{
         std::array<uint8_t[2], 256> t{};
         for (uint32_t i = 0; i < 256; ++i) {
            t[i][0] = std::countr_one(uint8_t(i)); // ones represent zeros
            t[i][1] = uncompressed_run(~i); // ones represent uncompressed bytes
         }
         return t;
      }();
   }
   
   inline void compress(const std::string_view in, std::string& out)
   {
      constexpr uint8_t z127 = 0b0'1111111; // 127 zeros
      constexpr uint8_t u127 = 0b1'1111111; // 127 uncompressed
      constexpr uint16_t v65535 = 65535;
      
      auto* it = in.data();
      const auto n_input = in.size();
      const auto* end = it + n_input;
      auto* u_ptr = it;
      
      out.resize(header_size + 2 * in.size()); // we don't need this much memory
      auto* dst = out.data();
      std::memcpy(dst, &n_input, header_size);
      dst += header_size;
      
      uint64_t zeros_count{};
      uint64_t uncompressed_count{};
      
      auto write_zeros = [&] {
         if (zeros_count > 2 * 127) {
            while (zeros_count > v65535) {
               *dst = 0;
               ++dst;
               std::memcpy(dst, &v65535, 2);
               dst += 2;
               zeros_count -= v65535;
            }
            
            if (zeros_count > 2 * 127) {
               *dst = 0;
               ++dst;
               std::memcpy(dst, &zeros_count, 2);
               dst += 2;
               zeros_count = 0;
            }
         }
         
         while (zeros_count > 127) {
            std::memcpy(dst, &z127, 1);
            ++dst;
            zeros_count -= 127;
         }
         
         if (zeros_count) {
            std::memcpy(dst, &zeros_count, 1);
            ++dst;
            zeros_count = 0;
         }
      };
      
      auto write_uncompressed = [&] {
         if (uncompressed_count > 2 * 127) {
            while (uncompressed_count > v65535) {
               *dst = 0b10000000;
               ++dst;
               std::memcpy(dst, &v65535, 2);
               dst += 2;
               std::memcpy(dst, u_ptr, v65535);
               u_ptr += v65535;
               dst += v65535;
               uncompressed_count -= v65535;
            }
            
            if (uncompressed_count > 2 * 127) {
               *dst = 0b10000000;
               ++dst;
               std::memcpy(dst, &uncompressed_count, 2);
               dst += 2;
               std::memcpy(dst, u_ptr, uncompressed_count);
               dst += uncompressed_count;
               uncompressed_count = 0;
            }
         }
         
         while (uncompressed_count > 127) {
            std::memcpy(dst, &u127, 1);
            ++dst;
            std::memcpy(dst, u_ptr, 127);
            u_ptr += 127;
            dst += 127;
            uncompressed_count -= 127;
         }
         
         if (uncompressed_count) {
            std::memcpy(dst, &uncompressed_count, 1);
            *dst |= 0b1'0000000;
            ++dst;
            std::memcpy(dst, u_ptr, uncompressed_count);
            dst += uncompressed_count;
            uncompressed_count = 0;
         }
      };
      
      for (; it < end;)
      {
         uint64_t chunk;
         std::memcpy(&chunk, it, 8);
         
         if (chunk)
         {
            const uint64_t zeros = detail::mark_zeros(chunk);
            if (zeros)
            {
               const uint8_t zeros_layout = detail::extract_msbs(zeros);
               const auto[n_zeros, run_length] = detail::run_table[zeros_layout];
               
               if ((zeros_count || n_zeros) && uncompressed_count) {
                  write_uncompressed();
               }
               
               zeros_count += n_zeros;
               if (zeros_count) {
                  write_zeros();
                  it += n_zeros;
               }
               
               if (uncompressed_count == 0) {
                  u_ptr = it;
               }
               
               it += run_length;
               uncompressed_count += run_length;
            }
            else {
               if (zeros_count) {
                  write_zeros();
               }
               
               if (uncompressed_count == 0) {
                  u_ptr = it;
               }
               
               uncompressed_count += 8;
               it += 8;
            }
         }
         else {
            if (uncompressed_count) {
               write_uncompressed();
            }
            
            zeros_count += 8;
            it += 8;
         }
      }
      
      write_uncompressed();
      write_zeros();
      
      const auto compressed_size = size_t(dst - out.data());
      if (compressed_size > in.size()) {
         // do not compress the data
         const auto size_indicator = n_input | uncompressed_flag;
         std::memcpy(out.data(), &size_indicator, header_size);
         std::memcpy(out.data() + header_size, in.data(), in.size());
         out.resize(header_size + in.size());
      }
      else {
         out.resize(compressed_size);
      }
   }
   
   inline void decompress(const std::string_view in, std::string& out)
   {
      auto* it = in.data();
      const auto* end = it + in.size();
      
      uint64_t size_indicator;
      std::memcpy(&size_indicator, it, header_size);
      it += header_size;
      
      if (size_indicator & uncompressed_flag) {
         const auto size = size_indicator & ~uncompressed_flag;
         out.resize(size);
         
         std::memcpy(out.data(), it, size);
      }
      else {
         out.clear(); // need to set all zeros
         out.resize(size_indicator);
         auto dst = out.data();
         
         for (; it < end;) {
            if (*it & 0b10000000) {
               // uncompresssed bytes
               const uint8_t n = *it & 0b01111111;
               if (n == 0) {
                  // two byte uncompresssed
                  ++it;
                  uint16_t size;
                  std::memcpy(&size, it, 2);
                  it += 2;
                  std::memcpy(dst, it, size);
                  it += size;
                  dst += size;
               }
               else {
                  ++it;
                  std::memcpy(dst, it, n);
                  it += n;
                  dst += n;
               }
            }
            else if (*it == 0) {
               // two byte zeros
               ++it;
               uint16_t size;
               std::memcpy(&size, it, 2);
               dst += size;
               it += 2;
            }
            else {
               // compressed zeros
               dst += uint8_t(*it);
               ++it;
            }
         }
      }
   }
}
