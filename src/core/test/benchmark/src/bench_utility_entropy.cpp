
#include "bench_utility_entropy.h"

#include <uBitStreamWriter.h>

#include <cassert>
#include <memory>
#include <queue>

using lcevc_dec::utility::BitStreamByteWriterRawMemory;
using lcevc_dec::utility::BitStreamWriter;

// -----------------------------------------------------------------------------

uint8_t lengthBitWidth(uint8_t lengthDiff)
{
    static const uint8_t table[32] = {
        // Figure 17 & 18: out = log2(max_length - min_length + 1);
        1, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, // 0   - 15
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, // 16  - 31
    };

    return table[lengthDiff];
}

// -----------------------------------------------------------------------------

constexpr uint32_t kSymbolCount = 256;

// -----------------------------------------------------------------------------

struct HuffmanCode
{
    HuffmanCode(uint8_t symbol_, uint8_t bits_)
        : symbol(symbol_)
        , bits(bits_)
    {}
    uint8_t symbol;
    uint8_t bits;
    uint8_t value = 0;
};

class HuffmanWriter
{
public:
    HuffmanWriter() = default;

    explicit HuffmanWriter(const std::vector<HuffmanCode>& codes)
        : m_codes(codes)
    {}

    // Write codes + lengths to bitstream
    void writeCodes(BitStreamWriter& bitstream);

    // Write a coded symbol to the bitstream
    void writeSymbol(BitStreamWriter& bitstream, uint8_t symbol) const;

private:
    std::vector<HuffmanCode> m_codes;
};

// -----------------------------------------------------------------------------

// Resolve symbol codes and write codes+lengths to bitstream
void HuffmanWriter::writeCodes(BitStreamWriter& bitstream)
{
    if (m_codes.empty()) {
        // No symbols
        bitstream.WriteBits(5, 31); // min_code_length
        bitstream.WriteBits(5, 31); // max_code_length
        return;
    }

    if (m_codes.size() == 1) {
        // Single symbol
        bitstream.WriteBits(5, 0);                      // min_code_length
        bitstream.WriteBits(5, 0);                      // max_code_length
        bitstream.WriteBits(8, m_codes.front().symbol); // single_symbol
        return;
    }

    const uint8_t minCodeLength = m_codes.front().bits;
    const uint8_t maxCodeLength = m_codes.back().bits;
    const uint8_t lengthBits = lengthBitWidth(maxCodeLength - minCodeLength);

    bitstream.WriteBits(5, minCodeLength); // min_code_length
    bitstream.WriteBits(5, maxCodeLength); // max_code_length

    if (m_codes.size() > 31) {
        // More than 31 coded symbols - write a 'presence' bitmap
        bitstream.WriteBits(1, 1); // presence_bitmap=1

        uint8_t lengths[kSymbolCount] = {0};

        for (const auto& code : m_codes) {
            lengths[code.symbol] = code.bits;
        }

        for (auto length : lengths) {
            if (length) {
                bitstream.WriteBits(1, 1);                               // presence
                bitstream.WriteBits(lengthBits, length - minCodeLength); // length
            } else {
                bitstream.WriteBits(1, 0); // presence
            }
        }
    } else {
        // 31 or less coded symbols - Write symbol/length pairs
        bitstream.WriteBits(1, 0);                                     // presence_bitmap=0
        bitstream.WriteBits(5, static_cast<uint32_t>(m_codes.size())); // count

        for (const auto& c : m_codes) {
            bitstream.WriteBits(8, c.symbol);                        // symbol
            bitstream.WriteBits(lengthBits, c.bits - minCodeLength); // length
        }
    }
}

// Write a coded symbol to the bitstream
void HuffmanWriter::writeSymbol(BitStreamWriter& bitstream, uint8_t symbol) const
{
    // Find symbols in codes
    for (const auto& code : m_codes) {
        if (code.symbol == symbol) {
            bitstream.WriteBits(code.bits, code.value); // code-bits
            return;
        }
    }

    assert(false);
}

// -----------------------------------------------------------------------------

// Node of huffman coding
struct Node
{
    Node(uint32_t symbol_, uint32_t count_)
        : symbol(symbol_)
        , count(count_)
    {}

    Node(Node* left_, Node* right_, uint32_t node_number)
        : symbol(kSymbolCount + node_number)
        , count(left_->count + right_->count)
        , left(left_)
        , right(right_)
    {}

    uint32_t symbol = kSymbolCount;
    uint32_t count = 0;
    uint32_t bits = 0;
    Node* left = nullptr;
    Node* right = nullptr;
};

class HuffmanEncoder
{
public:
    // Accumulate symbol counts
    void addSymbol(uint8_t symbol, uint32_t count);

    // Resolve code length & values and make a writer that will convert symbols
    // into codes.
    HuffmanWriter finish();

private:
    uint32_t m_symbolFrequency[kSymbolCount] = {0};
};

// -----------------------------------------------------------------------------

// Add a symbol to pending code tree
void HuffmanEncoder::addSymbol(uint8_t symbol, uint32_t count)
{
    assert(symbol < kSymbolCount);
    m_symbolFrequency[symbol] += count;
}

HuffmanWriter HuffmanEncoder::finish()
{
    std::vector<std::unique_ptr<Node>> nodes;

    // Build vector of uncoded nodes, sorted by frequency
    auto nodeCompare = [](const Node* l, const Node* r) {
        if (l->count == r->count) {
            // Equal counts - order t by symbol or node number - leaves are lower than inter nodes
            return l->symbol < r->symbol;
        } else
            return l->count > r->count;
    };

    std::priority_queue<Node*, std::vector<Node*>, decltype(nodeCompare)> heap(nodeCompare);

    for (uint32_t symbol = 0; symbol < kSymbolCount; ++symbol) {
        if (m_symbolFrequency[symbol]) {
            nodes.emplace_back(std::make_unique<Node>(symbol, m_symbolFrequency[symbol]));
            heap.emplace(nodes.back().get());
        }
    }

    if (heap.empty()) {
        // No symbols at all
        return HuffmanWriter();
    }

    const size_t symbolCount = nodes.size();

    // Build huffman tree on top of symbol leaves

    // While there is more than 1 pending node:
    //   take 2 smallest by frequency and combine them to make a new internal node
    while (heap.size() > 1) {
        Node* left = heap.top();
        heap.pop();
        Node* right = heap.top();
        heap.pop();
        nodes.emplace_back(std::make_unique<Node>(left, right, static_cast<uint32_t>(nodes.size())));
        heap.emplace(nodes.back().get());
    }

    // Starting with the root (last entry in nodes) - walk backwards filling in code lengths
    assert(heap.top() == nodes.back().get());
    for (auto node = nodes.rbegin(); node != nodes.rend(); ++node) {
        if ((*node)->symbol >= kSymbolCount) {
            (*node)->left->bits = (*node)->bits + 1;
            (*node)->right->bits = (*node)->bits + 1;
        }
    }

    // The symbol nodes will now have their final code lengths.
    // Sort leaf nodes by (decending coded length, ascending symbol)
    std::sort(nodes.begin(), nodes.begin() + symbolCount, [](const auto& a, const auto& b) {
        if (a->bits == b->bits) {
            return a->symbol > b->symbol;
        }

        return a->bits < b->bits;
    });

    // Generate code table for encoder
    std::vector<HuffmanCode> codes;
    for (auto n = nodes.begin(); n != nodes.begin() + symbolCount; ++n) {
        codes.emplace_back((*n)->symbol, (*n)->bits);
    }

    // Assign values to codes
    uint8_t currentLength = codes.back().bits;
    uint8_t currentValue = 0;

    for (auto code = codes.rbegin(); code != codes.rend(); ++code) {
        if (code->bits < currentLength) {
            currentValue >>= (currentLength - code->bits);
            currentLength = code->bits;
        }
        code->value = currentValue++;
    }

    return HuffmanWriter(codes);
}

// -----------------------------------------------------------------------------

enum class EntropyState
{
    LSB,
    MSB,
    Zero
};

constexpr uint32_t kEntropyStateCount = 3;
constexpr uint32_t kTransitionCount = 4;

constexpr EntropyState nextState(EntropyState state, uint8_t symbol)
{
    // Given the current context, and the states of bit 0 and bit 7 what's the next one?
    constexpr EntropyState kTable[kEntropyStateCount][kTransitionCount] = {
        // 0==0 && 7==0     0==1 && 7==0       0==0 && 7==1        0==1 && 7==1
        {EntropyState::LSB, EntropyState::MSB, EntropyState::Zero, EntropyState::MSB}, // EntropyState::LSB
        {EntropyState::LSB, EntropyState::LSB, EntropyState::Zero, EntropyState::Zero}, // EntropyState::MSB
        {EntropyState::LSB, EntropyState::LSB, EntropyState::Zero, EntropyState::Zero} // EntropyState::Zero
    };

    // Use the context transition table to move to the next context.
    const uint8_t stateBits = (symbol & 0x01) | (symbol & 0x80) >> 6;
    return kTable[static_cast<uint32_t>(state)][stateBits];
}

// -----------------------------------------------------------------------------

class EntropyEncoder
{
public:
    void encodeRun(int16_t residual, uint32_t zeros);
    std::vector<uint8_t> finish(bool bRLEOnly);

private:
    void encodeHighCount(EntropyState state, uint32_t symbol);
    void encodeSymbol(EntropyState state, uint8_t symbol);

    struct RLESymbol
    {
        RLESymbol(EntropyState st, uint8_t sy)
            : state(st)
            , symbol(sy)
        {}

        EntropyState state = EntropyState::LSB;
        uint8_t symbol = 0;
    };

    HuffmanEncoder m_huffmanStates[kEntropyStateCount];
    std::vector<RLESymbol> m_rleSymbols;

    std::vector<uint8_t> m_rleData;
};

// -----------------------------------------------------------------------------

void EntropyEncoder::encodeRun(int16_t residual, uint32_t runLength)
{
    // Is there a run?
    const uint16_t runBit = (runLength > 0) ? 0x80 : 0;

    // Encode value
    if (residual >= -32 && residual < 32) {
        encodeSymbol(EntropyState::LSB, static_cast<uint8_t>((residual * 2 + 0x40) | runBit));
    } else {
        const auto value = static_cast<uint16_t>(std::min(std::max(residual + 0x2000, 0), 0x3fff) << 1);
        encodeSymbol(EntropyState::LSB, static_cast<uint8_t>((value & 0xfe) | 0x01));
        encodeSymbol(EntropyState::MSB, static_cast<uint8_t>(((value >> 8) & 0x7f) | runBit));
    }

    // Encode run-length
    if (runBit) {
        if (runLength > 0x7f) {
            encodeHighCount(EntropyState::Zero, runLength >> 7);
        }

        encodeSymbol(EntropyState::Zero, runLength & 0x7f);
    }
}

std::vector<uint8_t> EntropyEncoder::finish(bool bRLEOnly)
{
    // RLE data is ready to be consumed immediately.
    if (bRLEOnly) {
        return m_rleData;
    }

    // Build memory buffer to write into.
    std::vector<uint8_t> backing;

    if (!m_rleData.empty()) {
        BitStreamWriter bitstream([&backing](uint8_t byte) {
            backing.push_back(byte);
            return true;
        });

        // Write Huffman headers
        std::vector<HuffmanWriter> huffmanWriters;

        for (auto& huffmanState : m_huffmanStates) {
            huffmanWriters.emplace_back(huffmanState.finish());
            huffmanWriters.back().writeCodes(bitstream);
        }

        // Always start in LSB state, and write Huffman codes.
        EntropyState state = EntropyState::LSB;
        for (const auto symbol : m_rleData) {
            huffmanWriters[static_cast<uint32_t>(state)].writeSymbol(bitstream, symbol);
            state = nextState(state, symbol);
        }

        bitstream.Finish();
    }

    return backing;
}

void EntropyEncoder::encodeHighCount(EntropyState state, uint32_t value)
{
    if (value > 0x7f) {
        encodeHighCount(state, value >> 7);
    }

    encodeSymbol(state, (value & 0x7f) | 0x80);
}

void EntropyEncoder::encodeSymbol(EntropyState state, uint8_t symbol)
{
    m_rleData.push_back(symbol);
    m_rleSymbols.emplace_back(state, symbol);
    m_huffmanStates[static_cast<uint32_t>(state)].addSymbol(symbol, 1);
}

// -----------------------------------------------------------------------------

std::vector<uint8_t> entropyEncode(const Surface_t& surface, bool bRLEOnly, const PelFunction& pelOp)
{
    EntropyEncoder encoder;
    int16_t value = 0;
    uint32_t zeroRunLength = 0;

    for (uint32_t y = 0; y < surface.height; ++y) {
        const auto* row = reinterpret_cast<const int16_t*>(surfaceGetLine(&surface, y));

        for (uint32_t x = 0; x < surface.width; ++x) {
            const int16_t pel = pelOp(row[x]);

            // Special case - always have to encode first symbol, even if it is zero
            if (x == 0 && y == 0) {
                value = pel;
                zeroRunLength = 0;
                continue;
            }

            if (pel == 0) {
                // Extend current run
                zeroRunLength++;
            } else {
                // Emit previous run and record this residual
                encoder.encodeRun(value, zeroRunLength);
                value = pel;
                zeroRunLength = 0;
            }
        }
    }

    // Emit last run
    encoder.encodeRun(value, zeroRunLength);

    return encoder.finish(bRLEOnly);
}

// -----------------------------------------------------------------------------