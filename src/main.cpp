#include <algorithm>
#include <bit>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// Zero Byte 8-Chunk Compression: ZB8

// ZB8 is focused on compressing zero bytes.
// ZB8 has a maximum compression ratio of nearly 22000.
// If your data could be all zeros, ZB8 is fantastic.
// If ZB8 cannot compress your data efficiently
// then it simply sends the original data with an 8 byte header.
// ZB8 will only ever increase your data size by 8 bytes.

// Data preparation:
// It is common to have to prepare for ZB8, in order to expose zero bytes.
// The ideal preparation is data dependent and may include delta encoding or xor encoding.
// ZB8 provides higher level compression algorithms with data preparation:
// - zb8::xor_sequence...
// - zb8::xor_difference...
// - zb8::delta...
// - zb8::delta_delta...
// The core algorithms are simply zb8::compress and zb8::decompress.

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
      
      constexpr uint8_t calculate_cost(const uint8_t value, const bool zero_lead) noexcept {
         uint8_t cost = 0;
         uint8_t zeros = 0;
         uint8_t ones = 0;

          // Iterate over each bit of the uint8_t value
          for (int i = 0; i < 8; ++i) {
              // Check if the current bit is 1
              if ((value >> i) & 1) {
                 if (ones == 0) {
                    ++cost; // For encountering a one
                    zeros = 0;
                 }
                 ++cost;
                 ++ones;
              } else {
                 if (zeros == 0) {
                    ++cost;
                    ones = 0;
                 }
                 ++zeros;
              }
          }
         
         if (zero_lead && (value % 2 == 0)) {
            // even numbers have leading zeros
            // if we are just adding zeros to a list of zeros
            // then we can reduce the cost by one
            --cost;
         }

          return cost;
      }

      constexpr std::array<uint8_t, 256> cost_table = []{
         std::array<uint8_t, 256> t{};
         for (uint32_t i = 0; i < 256; ++i) {
            t[i] = calculate_cost(i, false);
         }
         return t;
      }();

      constexpr std::array<uint8_t, 256> cost_table_zero_lead = []{
         std::array<uint8_t, 256> t{};
         for (uint32_t i = 0; i < 256; ++i) {
            t[i] = calculate_cost(i, true);
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
               const uint8_t n_zeros = std::countr_one(zeros_layout);
               //const uint8_t n_zeros = std::countr_zero(chunk) >> 3;
               
               if ((zeros_count || n_zeros) && uncompressed_count) {
                  write_uncompressed();
               }
               
               zeros_count += n_zeros;
               write_zeros();
               it += n_zeros;
               
               if (uncompressed_count == 0) {
                  u_ptr = it;
               }
               
               uint32_t i = n_zeros;
               for (; i < 8; ++i, ++it) {
                  if (*it == 0) {
                     break;
                  }
               }
               uncompressed_count += i - n_zeros;
            }
            else {
               if (zeros_count) {
                  write_uncompressed();
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

#include <cmath>
#include <random>
#include <chrono>
#include <iostream>

void testing_swar()
{
   /*//uint64_t chunk = 0b0000000000000000000000000000000100000000000000010000000100000000;
   uint64_t chunk = 0b0000000000000000000000000000000000000000000000000000000100000000;
   //uint8_t chunk = 0b00000001;
   constexpr size_t N = sizeof(chunk) * 8;
   std::cout << std::bitset<N>(chunk) << '\n';
   // (((chunk - 0x0101010101010101u) & ~chunk) & 0x8080808080808080u);
   std::cout << std::bitset<N>(chunk - 0x0101010101010101u) << '\n';
   std::cout << std::bitset<N>(~chunk) << '\n';
   std::cout << std::bitset<N>((chunk - 0x0101010101010101u) & ~chunk) << '\n';
   std::cout << std::bitset<N>(((chunk - 0x0101010101010101u) & ~chunk) & 0x8080808080808080u) << '\n';
   
   {
      uint64_t word = chunk;
      constexpr uint64_t mask = 0x7F7F7F7F7F7F7F7Full;
      uint64_t t0 = (((word & mask) + mask) | word);
      uint64_t t2 = (t0 & 0x8080808080808080ull) ^ 0x8080808080808080ull;
      
      std::cout << std::bitset<N>(t2) << '\n';
   }*/
}

void assess_table()
{
   for (uint32_t i = 0; i < 256; ++i) {
      std::cout << i << ": " << std::bitset<8>(i) << " | " << int(zb8::detail::cost_table_zero_lead[i]) << '\n';
   }
}

void profile()
{
   /*std::vector<uint64_t> vec;
   for (size_t i = 0; i < 100'000; ++i)
   {
      //const auto v = 100 * std::sin(double(i));
      //const auto v = double(i);
      //const auto v = uint64_t(i);
      //const uint64_t v = i / 3;
      //const uint64_t v = 0;
      //auto& element = data.emplace_back();
      //std::memcpy(&element, &v, 8);
      
      //const auto v = 1000 * std::sin(double(i));
      auto& element = vec.emplace_back();
      std::memcpy(&element, &v, 8);
   }
   
   for (size_t i = 1; i < vec.size(); ++i) {
      vec[i] = vec[i] ^ vec[i - 1]; // XOR current element with previous one
   }
   
   std::string data;
   data.resize(vec.size() * sizeof(double));
   std::memcpy(data.data(), vec.data(), vec.size() * sizeof(double));*/
   
   std::uniform_int_distribution<uint16_t> dist{0, 2};
   std::mt19937_64 generator{};
   std::string data;
   
   for (size_t i = 0; i < 100'000 * sizeof(uint64_t); ++i) {
      data.push_back(char(!bool(dist(generator))));
      //data.push_back(char(0));
      //data.push_back(char(1));
   }
   
   /*for (size_t i = 0; i < 100'000 * sizeof(uint64_t); ++i) {
      data.push_back(char(0));
   }*/
   
   auto original = data;
   
   std::string compressed{};
   auto t0 = std::chrono::steady_clock::now();
   zb8::compress(data, compressed);
   auto t1 = std::chrono::steady_clock::now();
   auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() * 1e-9;
   std::cout << "MB/s compress: " << (double(data.size()) / 1'000'000) / duration << '\n';
   
   const auto n_elements = data.size();
   std::cout << "Original size: " << n_elements << '\n';
   std::cout << "Compressed size: " << compressed.size() << '\n';
   std::cout << "Compression ratio: " << double(n_elements) / compressed.size() << '\n';
   
   data.clear();
   t0 = std::chrono::steady_clock::now();
   zb8::decompress(compressed, data);
   t1 = std::chrono::steady_clock::now();
   duration = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() * 1e-9;
   std::cout << "MB/s decompress: " << (double(data.size()) / 1'000'000) / duration << '\n';
   
   if (data != original) {
      throw std::runtime_error("Decompression failed!");
   }
}

int main() {
   //profile();
   assess_table();
   return 0;
}
