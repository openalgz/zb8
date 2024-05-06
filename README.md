# ZB8
*Fast, simple, zero byte compression.*

> [!WARNING]
>
> This project is in pre-release development.

ZB8 is especially useful for sending differences between datasets, where the data is often the same.

Key details:

- Core algorithm compresses zero bytes
- Maximum compression ratio of nearly 22000
  - If your data is often all zeros, ZB8 is fantastic
- Byte-level encoding for high performance

## Max 8 byte size increase

If ZB8 cannot compress your data efficiently then it simply encodes the original data with an 8 byte header. ZB8 will only ever increase your data size by 8 bytes.

## Core Algorithm

### zb8::compress

```c++
zb8::compress(in, out);
```

###  zb8::decompress

```
zb8::decompress(in, out);
```

## Advanced Algorithms

It is common to prepare data for the core ZB8 compression algorithm, in order to expose zero bytes. The ideal preparation is data dependent and may include delta encoding or xor encoding.

ZB8 provides higher level compression algorithms with data preparation:

- **zb8::xor_sequence**
- **zb8::xor_difference**
- **zb8::delta**
- **zb8::delta_delta**

## Specification

The data must begin with a single byte header. Data is always in little endian. The most significant bit indicates whether the subsequent count is referring to zeros or uncompressed bytes.

```
0b0XXXXXXX -> 7 X bits denote the number of zero bytes
0b1XXXXXXX -> 7 X bits denote the number of uncompressed bytes
```

### Two Byte Zero Cases

A count of zero for either zero bytes or uncompressed bytes wouldn't make sense. Rather, a size of zero is used to denote a two byte size indicator.
