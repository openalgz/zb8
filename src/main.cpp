#include "zb8/zb8.hpp"

#include <bitset>
#include <cmath>
#include <random>
#include <chrono>
#include <iostream>
#include <vector>

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
   using namespace zb8::detail;
   for (uint32_t i = 0; i < 256; ++i) {
      std::cout << i << ": " << std::bitset<8>(i) << " | " << int(run_table[i][0]) << ", " << int(run_table[i][1]) << '\n';
   }
}

void profile()
{
   /*std::vector<uint64_t> vec;
   for (size_t i = 0; i < 100'000; ++i)
   {
      //const auto v = 100 * std::sin(double(i));
      //const auto v = int(100 * std::sin(double(i)));
      //const auto v = double(i);
      //const auto v = uint64_t(i);
      const uint64_t v = i / 3;
      //const uint64_t v = 0;
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
   
   std::uniform_int_distribution<uint16_t> dist{0, 150};
   std::mt19937_64 generator{};
   std::string data;
   
   for (size_t i = 0; i < 1'000'000 * sizeof(uint64_t); ++i) {
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
   profile();
   //assess_table();
   return 0;
}
