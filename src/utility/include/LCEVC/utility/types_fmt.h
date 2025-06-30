/* Copyright (c) V-Nova International Limited 2023-2025. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License by V-Nova Limited.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. However, the
 * software may be incorporated into a project under a compatible license provided the requirements
 * of the BSD-3-Clause-Clear license are respected, and V-Nova Limited remains
 * licensor of the software ONLY UNDER the BSD-3-Clause-Clear license (not the compatible license).
 * ANY ONWARD DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO
 * THE EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

// Custom types for {fmt} - the formatting library.
//
#ifndef VN_LCEVC_UTILITY_TYPES_FMT_H
#define VN_LCEVC_UTILITY_TYPES_FMT_H

#include <fmt/format.h>
#include <LCEVC/lcevc_dec.h>
#include <LCEVC/utility/types_convert.h>

// LCEVC_ColorFormat
//
template <>
struct fmt::formatter<LCEVC_ColorFormat> : public formatter<string_view>
{
    template <typename FormatContext>
    auto format(const LCEVC_ColorFormat& cf, FormatContext& ctx) const
    {
        return formatter<string_view>::format(
            std::string("ColorFormat_") + std::string(lcevc_dec::utility::toString(cf)), ctx);
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
        return formatter<string_view>::format(
            std::string("ReturnCode_") + std::string(lcevc_dec::utility::toString(rc)), ctx);
    }
};

// LCEVC_ColorRange
template <>
struct fmt::formatter<LCEVC_ColorRange> : public formatter<string_view>
{
    template <typename FormatContext>
    auto format(const LCEVC_ColorRange& cr, FormatContext& ctx) const
    {
        return formatter<string_view>::format(
            std::string("ColorRange_") + std::string(lcevc_dec::utility::toString(cr)), ctx);
    }
};

// LCEVC_ColorPrimaries
template <>
struct fmt::formatter<LCEVC_ColorPrimaries> : public formatter<string_view>
{
    template <typename FormatContext>
    auto format(const LCEVC_ColorPrimaries& cs, FormatContext& ctx) const
    {
        return formatter<string_view>::format(
            std::string("ColorPrimaries_") + std::string(lcevc_dec::utility::toString(cs)), ctx);
    }
};

// LCEVC_TransferCharacteristics
template <>
struct fmt::formatter<LCEVC_TransferCharacteristics> : public formatter<string_view>
{
    template <typename FormatContext>
    auto format(const LCEVC_TransferCharacteristics& ct, FormatContext& ctx) const
    {
        return formatter<string_view>::format(
            std::string("TransferCharacteristics_") + std::string(lcevc_dec::utility::toString(ct)), ctx);
    }
};

// LCEVC_PictureFlag
template <>
struct fmt::formatter<LCEVC_PictureFlag> : public formatter<string_view>
{
    template <typename FormatContext>
    auto format(const LCEVC_PictureFlag& pf, FormatContext& ctx) const
    {
        return formatter<string_view>::format(
            std::string("PictureFlag_") + std::string(lcevc_dec::utility::toString(pf)), ctx);
    }
};

// LCEVC_Access
template <>
struct fmt::formatter<LCEVC_Access> : public formatter<string_view>
{
    template <typename FormatContext>
    auto format(const LCEVC_Access& ac, FormatContext& ctx) const
    {
        return formatter<string_view>::format(
            std::string("Access_") + std::string(lcevc_dec::utility::toString(ac)), ctx);
    }
};

// LCEVC_Event
template <>
struct fmt::formatter<LCEVC_Event> : public formatter<string_view>
{
    template <typename FormatContext>
    auto format(const LCEVC_Event& ev, FormatContext& ctx) const
    {
        return formatter<string_view>::format(
            std::string("Event_") + std::string(lcevc_dec::utility::toString(ev)), ctx);
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

#endif // VN_LCEVC_UTILITY_TYPES_FMT_H
