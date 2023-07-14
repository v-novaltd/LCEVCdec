/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

#include "bench_fixture.h"
#include "bench_utility.h"
#include "bench_utility_entropy.h"

#include <benchmark/benchmark.h>
#include <uBitStreamWriter.h>
#include <uMultiByte.h>

#include <chrono>
#include <cmath>
#include <functional>
#include <map>
#include <random>
#include <sstream>

extern "C"
{
#include "context.h"
#include "decode/entropy.h"
#include "surface/surface.h"
}

// -----------------------------------------------------------------------------

struct QuantizeParameters
{
    int32_t multiplier = 0;
    int32_t shift = 0;
    int32_t deadzone = 0;
};

// Pretty basic rate-controller that attempts to generate the requested number of
// bytes from the compressor - this is achieved by performing a binary search on
// the step-width domain until we've achieved a certain depth or we're within a
// certain range of the target.
class EntropyRateController
{
public:
    static constexpr uint16_t kStepWidthMin = 4;
    static constexpr uint16_t kStepWidthMax = 16383;
    static constexpr int32_t kStepWidthInitial = kStepWidthMax >> 1;
    static constexpr int32_t kIterationLimit = 16;
    static constexpr float kTargetBitrateRangePct = 0.01f;
    static constexpr float kDeadzoneFactor = 0.3f;

    enum class StepResult
    {
        Continue,
        Stop
    };

    enum class EncodingState
    {
        TooBig,
        TooSmall,
        CloseEnough
    };

    EntropyRateController(size_t targetBytes)
        : m_targetBytes{targetBytes}
    {
        const float targetRange = kTargetBitrateRangePct * static_cast<float>(m_targetBytes);
        m_targetBytesMin = static_cast<int32_t>(m_targetBytes - targetRange);
        m_targetBytesMax = static_cast<int32_t>(m_targetBytes + targetRange);
    }

    StepResult step(size_t encodedSize)
    {
        // First iteration has no input.
        if (m_iteration == 0) {
            m_iteration++;
            setStepWidth(kStepWidthInitial);
            return StepResult::Continue;
        }

        // Stop processing now, previous iteration set the best step-width.
        if (m_iteration > kIterationLimit) {
            return StepResult::Stop;
        }

        // Final iteration will just use the best step-width found thus far.
        if (m_iteration == kIterationLimit) {
            m_iteration++;
            setStepWidth(findBestStepWidth());
            return StepResult::Continue;
        }

        m_iteration++;

        m_encodedSizes.push_back(encodedSize);

        // Determine what we've achieved.
        EncodingState state = evaluateEncodedSize(encodedSize);

        // Nailed it, no more work to do.
        if (state == EncodingState::CloseEnough) {
            return StepResult::Stop;
        }

        // Select the new sub-range to encode in.
        if (state == EncodingState::TooBig) {
            // Increase step-width to produce smaller data
            m_stepWidthMin = m_currentStepWidth;
        } else {
            // Decrease step-width to produce big data.
            m_stepWidthMax = m_currentStepWidth;
        }

        setStepWidth(m_stepWidthMin + ((m_stepWidthMax - m_stepWidthMin) >> 1));

        return StepResult::Continue;
    }

    int32_t findBestStepWidth() const
    {
        // Search list of encodes to find step-width that resulted in closest size.
        auto count = m_encodedSizes.size();

        int32_t bestStepWidth = m_stepWidths[0];
        int32_t bestRange = std::abs(static_cast<int32_t>(m_targetBytes - m_encodedSizes[0]));

        for (size_t i = 1; i < count; ++i) {
            const int32_t candidateRange =
                std::abs(static_cast<int32_t>(m_targetBytes - m_encodedSizes[i]));

            if (candidateRange < bestRange) {
                bestStepWidth = m_stepWidths[i];
                bestRange = candidateRange;
            }
        }

        return bestStepWidth;
    }

    const auto& getQuantizeParameters() const { return m_quantize; }

    EncodingState evaluateEncodedSize(size_t encodedSize) const
    {
        if (encodedSize >= m_targetBytesMin && encodedSize <= m_targetBytesMax) {
            return EncodingState::CloseEnough;
        }

        return encodedSize < m_targetBytes ? EncodingState::TooSmall : EncodingState::TooBig;
    }

private:
    void setStepWidth(int32_t newStepWidth)
    {
        m_stepWidths.push_back(newStepWidth);
        m_currentStepWidth = newStepWidth;
        m_quantize.shift = m_currentStepWidth > 1024 ? 25 : 16;
        m_quantize.multiplier = (1 << m_quantize.shift) / m_currentStepWidth;
        m_quantize.deadzone =
            static_cast<int32_t>(static_cast<float>(m_currentStepWidth) * kDeadzoneFactor);
    }

    int32_t m_stepWidthMin = kStepWidthMin;
    int32_t m_stepWidthMax = kStepWidthMax;

    int32_t m_currentStepWidth = kStepWidthInitial;
    QuantizeParameters m_quantize;
    size_t m_targetBytes = 0;
    size_t m_targetBytesMin = 0;
    size_t m_targetBytesMax = 0;
    int32_t m_iteration = 0;

    std::vector<size_t> m_encodedSizes;
    std::vector<int32_t> m_stepWidths;
};

// -----------------------------------------------------------------------------

int16_t quantize(const QuantizeParameters& parameters, int16_t value)
{
    // This is a bit more detailed than strictly necessary in that it implements
    // deadzone too - but DZ does help with rounding issues for near zero values.
    const int16_t sign = 1 | (value >> 15);
    const int32_t unsignedValue = value * sign;
    const int32_t quantized =
        std::max(0, ((unsignedValue - parameters.deadzone) * parameters.multiplier) >> parameters.shift);

    return static_cast<int16_t>(quantized * sign);
}

std::vector<uint8_t> compressToSize(const Surface_t& surface, int32_t targetBytes, bool bRLEOnly)
{
    EntropyRateController rateControl(targetBytes);
    std::vector<uint8_t> result;

    // Step until RC is happy.
    while (rateControl.step(result.size()) == EntropyRateController::StepResult::Continue) {
        // Perform RLE compression.
        const auto& params = rateControl.getQuantizeParameters();
        auto quantization = std::bind(quantize, params, std::placeholders::_1);

        result = entropyEncode(surface, bRLEOnly, quantization);
    }

    // RC breaks once generated data is acceptable.
    return result;
}

// -----------------------------------------------------------------------------

// @todo(bob): Replace this with some structured noise, simplex seems to be a
//             pretty decent choice for the baseline, then some features can be
//             injected in, see: https://www.redblobgames.com/maps/terrain-from-noise/
//             for some inspiration.
bool populateResidualSurface(Surface_t& surface, int16_t minValue = -16384,
                             int16_t maxValue = 16383, uint32_t seed = 0)
{
    if (fixedPointHighPrecision(surface.type) != surface.type) {
        return false;
    }

    // Determine range to generate for.
    if (minValue > maxValue) {
        std::swap(minValue, maxValue);
    }

    const uint32_t range = maxValue - minValue;

    if ((range == 0) || (minValue < -16384) || (maxValue > 16383)) {
        return false;
    }

    // Prepare rng
    if (seed == 0) {
        seed = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count());
    }
    std::mt19937 rng(seed);
    std::uniform_int_distribution<std::mt19937::result_type> data(0, range);

    // Fill surface with desired random data.
    auto* dst = reinterpret_cast<int16_t*>(surface.data);
    const uint32_t count = surface.stride * surface.height;
    for (uint32_t i = 0; i < count; ++i) {
        dst[i] = static_cast<int16_t>(data(rng) + minValue);
    }

    return true;
}

class CompressedDataCache
{
public:
    const std::vector<uint8_t>& getCompressedData(const Dimensions& dimensions, uint32_t targetSize,
                                                  bool bRLEOnly, uint32_t seed)
    {
        uint64_t hash = 0;
        hash |= static_cast<uint64_t>(bRLEOnly) << 63;
        hash |= static_cast<uint64_t>(dimensions.height & 0x7FFF) << 48;
        hash |= static_cast<uint64_t>(dimensions.width & 0xFFFF) << 32;
        hash |= (targetSize ^ seed);

        if (auto found = m_entries.find(hash); found != m_entries.end()) {
            return found->second;
        }

        // @todo(bob): This should be stored in a cache.
        Surface_t surface{};

        surfaceIdle(ctx, &surface);

        surfaceInitialise(ctx, &surface, FPS8, dimensions.width, dimensions.height, dimensions.width, ILNone);

        static constexpr int16_t kSurfaceMin = -1024;
        static constexpr int16_t kSurfaceMax = 1024;
        populateResidualSurface(surface, kSurfaceMin, kSurfaceMax, seed);

        auto [result, _] = m_entries.try_emplace(hash, compressToSize(surface, targetSize, bRLEOnly));

        surfaceRelease(ctx, &surface);

        return result->second;
    }

    Context_t* ctx;

private:
    std::map<uint64_t, std::vector<uint8_t>> m_entries;
};

CompressedDataCache kCompressedDataCache;

// -----------------------------------------------------------------------------

class EntropyFixture : public lcevc_dec::Fixture
{
public:
    using Super = lcevc_dec::Fixture;

    void SetUp(benchmark::State& state) final
    {
        Super::SetUp(state);

        compressedDataCache.ctx = &ctx;

        bRLEOnly = static_cast<bool>(state.range(0));
        dimensions = Resolution::getDimensions(Resolution::e2160p);
        targetSize = static_cast<int32_t>(state.range(1));
    }

    bool bRLEOnly = false;
    Dimensions dimensions;
    int32_t targetSize = 0;

    CompressedDataCache compressedDataCache;
};

// -----------------------------------------------------------------------------

BENCHMARK_DEFINE_F(EntropyFixture, Decode)(benchmark::State& state)
{
    static constexpr uint32_t kDataSeed = 5866165;
    const auto& compressedData =
        compressedDataCache.getCompressedData(dimensions, targetSize, bRLEOnly, kDataSeed);

    for (auto _ : state) {
        Chunk_t chunk{};
        chunk.data = compressedData.data();
        chunk.size = static_cast<uint32_t>(compressedData.size());
        chunk.entropyEnabled = true;
        chunk.rleOnly = bRLEOnly;

        EntropyDecoder_t decoder{};
        if (entropyInitialise(&ctx, &decoder, &chunk, EDTDefault)) {
            state.SkipWithError("Failed to initialise layer decoder");
            return;
        }

        const int32_t expectedCount = dimensions.width * dimensions.height;
        int32_t decodedCount = 0;
        int16_t symbol;

        while (decodedCount < expectedCount) {
            decodedCount += entropyDecode(&decoder, &symbol) + 1;
        }

        entropyRelease(&decoder);

        if (decodedCount != expectedCount) {
            state.SkipWithError("Failed to decompress expected number of pixels");
            return;
        }
    }

    // Input size is 4k * int16_t.
    std::stringstream labelSS;
    labelSS << "compressed size: " << compressedData.size();
    state.SetLabel(labelSS.str());
}

// -----------------------------------------------------------------------------

constexpr int32_t operator""_MB(long double value) { return static_cast<int32_t>(value * 1000000); }

constexpr int32_t operator""_MB(unsigned long long value)
{
    return static_cast<int32_t>(value * 1000000);
}

BENCHMARK_REGISTER_F(EntropyFixture, Decode)
    ->ArgNames({"RLEOnly", "ByteSize"})

    ->Args({true, 0.1_MB})
    ->Args({true, 0.25_MB})
    ->Args({true, 0.5_MB})
    ->Args({true, 1_MB})
    ->Args({true, 2_MB})
    ->Args({true, 3_MB})
    ->Args({true, 4_MB})
    ->Args({true, 6_MB})
    ->Args({true, 8_MB})
    ->Args({true, 10_MB})
    ->Args({true, 12_MB})

    ->Args({false, 0.1_MB})
    ->Args({false, 0.25_MB})
    ->Args({false, 0.5_MB})
    ->Args({false, 1_MB})
    ->Args({false, 2_MB})
    ->Args({false, 3_MB})
    ->Args({false, 4_MB})
    ->Args({false, 6_MB})
    ->Args({false, 8_MB})
    ->Args({false, 10_MB})
    ->Args({false, 12_MB})

    ->Unit(benchmark::kMillisecond);

// -----------------------------------------------------------------------------