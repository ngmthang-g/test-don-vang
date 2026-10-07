#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

namespace tlobf {

constexpr std::uint64_t Mix64(std::uint64_t x) {
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

constexpr std::uint64_t Seed(std::uint64_t line, std::uint64_t counter) {
    return Mix64(0x9e3779b97f4a7c15ULL ^ (line * 0xd6e8feb86659fd93ULL) ^
                 (counter * 0xa0761d6478bd642fULL));
}

#if defined(_MSC_VER)
#define TL_OBF_NOINLINE __declspec(noinline)
#else
#define TL_OBF_NOINLINE __attribute__((noinline))
#endif

template <typename CharT, std::size_t N, std::uint64_t Key>
class EncryptedLiteral {
public:
    using UnsignedCharT = typename std::make_unsigned<CharT>::type;

    constexpr explicit EncryptedLiteral(const CharT (&plain)[N]) : encrypted_{} {
        for (std::size_t i = 0; i < N; ++i) {
            encrypted_[i] = static_cast<UnsignedCharT>(plain[i]) ^ Mask(i);
        }
    }

    TL_OBF_NOINLINE std::basic_string<CharT> decrypt() const {
        if constexpr (N <= 1) return {};
        std::basic_string<CharT> out(N - 1, CharT{});
        // The volatile dependency prevents LTCG from folding the decryptor back
        // into a plaintext .rdata string while keeping the encrypted bytes static.
        volatile std::uint64_t runtimeKey = Key;
        const std::uint64_t observedKey = runtimeKey;
        for (std::size_t i = 0; i + 1 < N; ++i) {
            const UnsignedCharT mask = RuntimeMask(i, observedKey);
            out[i] = static_cast<CharT>(encrypted_[i] ^ mask);
        }
        return out;
    }

private:
    static constexpr UnsignedCharT Mask(std::size_t i) {
        const std::uint64_t mixed = Mix64(Key + (i + 1) * 0x9e3779b185ebca87ULL);
        return static_cast<UnsignedCharT>(mixed ^ (mixed >> 17) ^ (mixed >> 41));
    }

    static TL_OBF_NOINLINE UnsignedCharT RuntimeMask(std::size_t i, std::uint64_t runtimeKey) {
        std::uint64_t mixed = runtimeKey + (i + 1) * 0x9e3779b185ebca87ULL;
        mixed ^= mixed >> 30;
        mixed *= 0xbf58476d1ce4e5b9ULL;
        mixed ^= mixed >> 27;
        mixed *= 0x94d049bb133111ebULL;
        mixed ^= mixed >> 31;
        return static_cast<UnsignedCharT>(mixed ^ (mixed >> 17) ^ (mixed >> 41));
    }

    std::array<UnsignedCharT, N> encrypted_;
};

template <std::uint64_t Key, typename CharT, std::size_t N>
constexpr auto MakeEncrypted(const CharT (&plain)[N]) {
    return EncryptedLiteral<CharT, N, Key>(plain);
}

TL_OBF_NOINLINE inline std::uint64_t RuntimeMix(std::uint64_t x) {
    volatile std::uint64_t v = x;
    std::uint64_t y = v;
    y ^= y >> 30;
    y *= 0xbf58476d1ce4e5b9ULL;
    y ^= y >> 27;
    y *= 0x94d049bb133111ebULL;
    y ^= y >> 31;
    return y;
}

} // namespace tlobf

#define TL_OBF_IMPL(literal, line, counter) \
    ([]() { \
        static constexpr auto encryptedLiteral = \
            ::tlobf::MakeEncrypted<::tlobf::Seed((line), (counter))>(literal); \
        return encryptedLiteral.decrypt(); \
    }())

#define TL_OBF_A(literal) TL_OBF_IMPL(literal, __LINE__, __COUNTER__)
#define TL_OBF_W(literal) TL_OBF_IMPL(literal, __LINE__, __COUNTER__)
