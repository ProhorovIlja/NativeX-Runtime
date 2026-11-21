# Synthetic command-list record safety contract

This original C++17 project tests bounds handling and parsing of an INVENTED little-endian command stream. It is not an Xbox Series command format, an Xbox API, a GPU driver, or a game loader.

It uses an 8-byte header containing a synthetic opcode, total length, and sequence. Graphics commands use Topology (1 data word) and Draw (2 words). A separate synthetic DMA queue has Copy (3 words). The bounded writer uses a 128-byte fixed buffer with 16-byte sentinel guards. Capacity failures and disallowed opcodes must leave prior output byte-identical. Reset clears the logical stream and sequence number. The independent parser checks header truncation, lengths, operation/queue mismatch and sequence order, and preserves the caller's previous parsed result when corruption is found.

Tests include fixed boundary cases plus 256 deterministic generated sequences. Linux GCC and Windows MSVC CTest run these with warnings treated as errors.

Motivation: the author of ivall/xcl at commit e555b87c07482c139c18baf0c20effa51b4b3938 reports that inline D3D12.X code writes records directly into console command-list internals; ordinary desktop D3D12 command lists have unrelated memory layouts. Source: https://github.com/ivall/xcl/commit/e555b87c07482c139c18baf0c20effa51b4b3938 .

This test does NOT establish console record lengths, opcodes, pointer offsets, vtable layout, GPU synchronization, or actual graphics output. None of the invented bytes should be used as an Xbox decoder. Its only verified deliverable is a disciplined defensive C++ record buffer/parser interface that can later be compared against separately verified real formats. No third-party GPL source or game binary is used.
