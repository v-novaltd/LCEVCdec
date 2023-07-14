// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// Custom types for {fmt} - the formatting library.
//
#ifndef VN_LCEVC_UTILITY_TYPES_FMT_H
#define VN_LCEVC_UTILITY_TYPES_FMT_H

#include "LCEVC/lcevc_dec.h"
#include "LCEVC/utility/types_convert.h"

#include <fmt/format.h>

// LCEVC_ColorFormat
//
template <>
struct fmt::formatter<LCEVC_ColorFormat> : public formatter<string_view>
{
    template <typename FormatContext>
    auto format(const LCEVC_ColorFormat& cf, FormatContext& ctx) const
    {
        return formatter<string_view>::format(std::string("ColorFormat_") + std::string(lcevc_dec::utility::toString(cf)), ctx);
    }
};

// LCEVC_ReturnCode
//
template <>
struct fmt::formatter<LCEVC_ReturnCode> : public formatter<string_view>
{
    template <typename FormatContext>
    auto format(const LCEVC_ReturnCode& rc, FormatContext& ctx) const
    {
        return formatter<string_view>::format(std::string("ReturnCode_") + std::string(lcevc_dec::utility::toString(rc)), ctx);
    }
};

// LCEVC_ColorRange
template <>
struct fmt::formatter<LCEVC_ColorRange> : public formatter<string_view>
{
    template <typename FormatContext>
    auto format(const LCEVC_ColorRange& cr, FormatContext& ctx) const
    {
        return formatter<string_view>::format(std::string("ColorRange_") + std::string(lcevc_dec::utility::toString(cr)), ctx);
    }
};

// LCEVC_ColorStandard
template <>
struct fmt::formatter<LCEVC_ColorStandard> : public formatter<string_view>
{
    template <typename FormatContext>
    auto format(const LCEVC_ColorStandard& cs, FormatContext& ctx) const
    {
        return formatter<string_view>::format(std::string("ColorStandard_") + std::string(lcevc_dec::utility::toString(cs)), ctx);
    }
};

// LCEVC_ColorTransfer
template <>
struct fmt::formatter<LCEVC_ColorTransfer> : public formatter<string_view>
{
    template <typename FormatContext>
    auto format(const LCEVC_ColorTransfer& ct, FormatContext& ctx) const
    {
        return formatter<string_view>::format(std::string("ColorTransfer_") + std::string(lcevc_dec::utility::toString(ct)), ctx);
    }
};

// LCEVC_PictureFlag
template <>
struct fmt::formatter<LCEVC_PictureFlag> : public formatter<string_view>
{
    template <typename FormatContext>
    auto format(const LCEVC_PictureFlag& pf, FormatContext& ctx) const
    {
        return formatter<string_view>::format(std::string("PictureFlag_") + std::string(lcevc_dec::utility::toString(pf)), ctx);
    }
};

// LCEVC_Access
template <>
struct fmt::formatter<LCEVC_Access> : public formatter<string_view>
{
    template <typename FormatContext>
    auto format(const LCEVC_Access& ac, FormatContext& ctx) const
    {
        return formatter<string_view>::format(std::string("Access_") + std::string(lcevc_dec::utility::toString(ac)), ctx);
    }
};

// LCEVC_Event
template <>
struct fmt::formatter<LCEVC_Event> : public formatter<string_view>
{
    template <typename FormatContext>
    auto format(const LCEVC_Event& ev, FormatContext& ctx) const
    {
        return formatter<string_view>::format(std::string("Event_") + std::string(lcevc_dec::utility::toString(ev)), ctx);
    }
};

// LCEVC_DecoderHandle
template <>
struct fmt::formatter<LCEVC_DecoderHandle>
{
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const LCEVC_DecoderHandle& h, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "DecoderHandle_{:04x}", h.hdl);
    }
};

// LCEVC_PictureHandle
template <>
struct fmt::formatter<LCEVC_PictureHandle>
{
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const LCEVC_PictureHandle& h, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "PictureHandle_{:04x}", h.hdl);
    }
};

// LCEVC_AccelContextHandle
template <>
struct fmt::formatter<LCEVC_AccelContextHandle>
{
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const LCEVC_AccelContextHandle& h, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "AccelContextHandle_{:04x}", h.hdl);
    }
};

// LCEVC_AccelBufferHandle
template <>
struct fmt::formatter<LCEVC_AccelBufferHandle>
{
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const LCEVC_AccelBufferHandle& h, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "AccelBufferHandle_{:04x}", h.hdl);
    }
};

// LCEVC_PictureLockHandle
template <>
struct fmt::formatter<LCEVC_PictureLockHandle>
{
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const LCEVC_PictureLockHandle& h, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "PictureLockHandle_{:04x}", h.hdl);
    }
};

#endif
