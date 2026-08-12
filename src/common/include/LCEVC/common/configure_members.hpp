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

#ifndef VN_LCEVC_COMMON_CONFIGURE_MEMBERS_H
#define VN_LCEVC_COMMON_CONFIGURE_MEMBERS_H

#include <LCEVC/common/class_utils.hpp>
#include <LCEVC/common/configure.hpp>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lcevc_dec::common {

// All the configuration binding types are derived from this - top level config code can use this
// to dispatch. C is the configuration object whose members you want to set.
//
template <typename C>
struct ConfigBindingBase
{
    ConfigBindingBase() = default;

    virtual ~ConfigBindingBase() = default;
    virtual bool set(C& /*cfg*/, const bool& /*val*/) const { return false; }
    virtual bool set(C& /*cfg*/, const int32_t& /*val*/) const { return false; }
    virtual bool set(C& /*cfg*/, const float& /*val*/) const { return false; }
    virtual bool set(C& /*cfg*/, const std::string& /*val*/) const { return false; }

    virtual bool set(C& /*cfg*/, const std::vector<bool>& /*arr*/) const { return false; }
    virtual bool set(C& /*cfg*/, const std::vector<int32_t>& /*arr*/) const { return false; }
    virtual bool set(C& /*cfg*/, const std::vector<float>& /*arr*/) const { return false; }
    virtual bool set(C& /*cfg*/, const std::vector<std::string>& /*arr*/) const { return false; }

    VNNoCopyNoMove(ConfigBindingBase);

private:
};

// Concrete binding for a particular member type
//
template <typename C, typename T>
struct ConfigBinding : public ConfigBindingBase<C>
{
    explicit ConfigBinding(T C::*ptr)
        : memberPointer(ptr)
    {}

    bool set(C& cfg, const T& val) const override
    {
        cfg.*memberPointer = val;
        return true;
    }

    T C::*memberPointer;
};

template <typename C, typename T>
std::shared_ptr<const ConfigBindingBase<C>> makeBinding(T C::*ptr)
{
    return std::make_shared<ConfigBinding<C, T>>(ptr);
}

// Templated binding for entries of array config members
//
template <size_t Offset, size_t ArrLen, typename C, typename ElementType>
struct ConfigBindingArrElement : public ConfigBindingBase<C>
{
    explicit ConfigBindingArrElement(std::array<ElementType, ArrLen> C::*ptr)
        : memberPointer(ptr)
    {}

    bool set(C& cfg, const ElementType& val) const override
    {
        (cfg.*memberPointer)[Offset] = val;
        return true;
    }

    std::array<ElementType, ArrLen> C::*memberPointer;
};

template <size_t Offset, size_t ArrLen, typename C, typename ElementType>
std::shared_ptr<const ConfigBindingBase<C>> makeBindingArrElement(
    std::array<ElementType, ArrLen> C::*ptr)
{
    return std::make_shared<ConfigBindingArrElement<Offset, ArrLen, C, ElementType>>(ptr);
}

// Templated binding for array config members (settable by iterating over a vector).
//
template <typename C, typename T, size_t N>
struct ConfigBindingArr : public ConfigBindingBase<C>
{
    explicit ConfigBindingArr(std::array<T, N> C::*ptr)
        : memberPointer(ptr)
    {}

    bool set(C& cfg, const std::vector<T>& val) const override
    {
        if (val.size() > N) {
            return false;
        }
        std::copy(val.begin(), val.end(), (cfg.*memberPointer).begin());
        return true;
    }

    std::array<T, N> C::*memberPointer;
};

template <typename C, typename T, size_t N>
std::shared_ptr<const ConfigBindingBase<C>> makeBindingArray(std::array<T, N> C::*ptr)
{
    return std::make_shared<ConfigBindingArr<C, T, N>>(ptr);
}

// Special cases for unsigned integers
// Concrete binding for a particular member type
//
template <typename C>
struct ConfigBindingUnsigned : public ConfigBindingBase<C>
{
    explicit ConfigBindingUnsigned(uint32_t C::*ptr)
        : memberPointer(ptr)
    {}

    bool set(C& cfg, const int32_t& val) const override
    {
        if (val < 0) {
            return false;
        }
        cfg.*memberPointer = val;
        return true;
    }

    uint32_t C::*memberPointer;
};

template <typename C>
std::shared_ptr<const ConfigBindingBase<C>> makeBinding(uint32_t C::*ptr)
{
    return std::make_shared<ConfigBindingUnsigned<C>>(ptr);
}

// Bind configuration member 'set()' methods
//
template <typename C, typename T>
struct ConfigBindingMethod : public ConfigBindingBase<C>
{
    explicit ConfigBindingMethod(bool (C::*methodPtr)(const T& val))
        : methodPtr(methodPtr)
    {}

    bool set(C& cfg, const T& val) const override { return (cfg.*methodPtr)(val); }

    bool (C::*methodPtr)(const T& val);
};

template <typename C, typename T>
std::shared_ptr<const ConfigBindingBase<C>> makeBinding(bool (C::*methodPtr)(const T& val))
{
    return std::make_shared<ConfigBindingMethod<C, T>>(methodPtr);
}

// The type for an initializer_list for ConfigurableMembers
//
template <typename C>
using ConfigMemberMapInitializer = std::pair<const char*, std::shared_ptr<const ConfigBindingBase<C>>>;

template <typename C>
class ConfigMemberMap
{
public:
    ConfigMemberMap(std::initializer_list<ConfigMemberMapInitializer<C>> list) noexcept
    {
        const ConfigMemberMapInitializer<C>* const data = list.begin();
        m_memberMap.reserve(list.size());
        for (size_t i = 0; i < list.size(); ++i) {
            const auto& val = data[i];
            if (!val.first) {
                continue;
            }
            m_memberMap.emplace_back(std::string(val.first), val.second);
        }
    }

    const std::shared_ptr<const ConfigBindingBase<C>>& getConfig(std::string_view nameView) const
    {
        // Default base that always returns false
        static const std::shared_ptr<const ConfigBindingBase<C>> defaultBase =
            std::make_shared<ConfigBindingBase<C>>();
        for (const auto& entry : m_memberMap) {
            if (entry.first == nameView) {
                return entry.second;
            }
        }
        return defaultBase;
    }

private:
    std::vector<std::pair<std::string, std::shared_ptr<const ConfigBindingBase<C>>>> m_memberMap;
};

// Implementation of `Configurable` that takes a map of configurable members.
//
// Can be used as a mix-in or as a member.
//
template <typename C>
class ConfigurableMembers : public Configurable
{
public:
    ConfigurableMembers(const ConfigMemberMap<C>& memberMap, C& config)
        : m_memberMap(memberMap)
        , m_config(config)
    {}
    ~ConfigurableMembers() = default;

    // The `Configurable` interface
    bool configure(std::string_view name, bool val) override
    {
        return m_memberMap.getConfig(name)->set(m_config, val);
    }
    bool configure(std::string_view name, int32_t val) override
    {
        return m_memberMap.getConfig(name)->set(m_config, val);
    }
    bool configure(std::string_view name, float val) override
    {
        return m_memberMap.getConfig(name)->set(m_config, val);
    }
    bool configure(std::string_view name, const std::string& val) override
    {
        return m_memberMap.getConfig(name)->set(m_config, val);
    }

    bool configure(std::string_view name, const std::vector<bool>& val) override
    {
        return m_memberMap.getConfig(name)->set(m_config, val);
    }
    bool configure(std::string_view name, const std::vector<int32_t>& val) override
    {
        return m_memberMap.getConfig(name)->set(m_config, val);
    }
    bool configure(std::string_view name, const std::vector<float>& val) override
    {
        return m_memberMap.getConfig(name)->set(m_config, val);
    }
    bool configure(std::string_view name, const std::vector<std::string>& val) override
    {
        return m_memberMap.getConfig(name)->set(m_config, val);
    }

    VNNoCopyNoMove(ConfigurableMembers);

private:
    const ConfigMemberMap<C>& m_memberMap;
    C& m_config;
};

} // namespace lcevc_dec::common

#endif // VN_LCEVC_COMMON_CONFIGURE_MEMBERS_H
