/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
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

#ifndef VN_API_CONFIG_MAP_H_
#define VN_API_CONFIG_MAP_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// - ConfigBinding --------------------------------------------------------------------------------

// All the configuration binding types are derived from this - top level config code can use this
// to dispatch. C is the configuration object whose members you want to set.
//
template <typename C>
struct ConfigBindingBase
{
    virtual ~ConfigBindingBase() = default;
    virtual bool set(C& cfg, const bool& val) const { return false; }
    virtual bool set(C& cfg, const int32_t& val) const { return false; }
    virtual bool set(C& cfg, const float& val) const { return false; }
    virtual bool set(C& cfg, const std::string& val) const { return false; }

    virtual bool set(C& cfg, const std::vector<bool>& arr) const { return false; }
    virtual bool set(C& cfg, const std::vector<int32_t>& arr) const { return false; }
    virtual bool set(C& cfg, const std::vector<float>& arr) const { return false; }
    virtual bool set(C& cfg, const std::vector<std::string>& arr) const { return false; }
};

// Templated binding for scalar or vector config members (settable with `operator=`)
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
const ConfigBindingBase<C>* makeBinding(T C::*ptr)
{
    return new ConfigBinding<C, T>(ptr);
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
const ConfigBindingBase<C>* makeBindingArrElement(std::array<ElementType, ArrLen> C::*ptr)
{
    return new ConfigBindingArrElement<Offset, ArrLen, C, ElementType>(ptr);
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
const ConfigBindingBase<C>* makeBindingArray(std::array<T, N> C::*ptr)
{
    return new ConfigBindingArr<C, T, N>(ptr);
}

// - ConfigMap ------------------------------------------------------------------------------------

template <typename C>
class ConfigMap : private std::unordered_map<std::string, std::unique_ptr<const ConfigBindingBase<C>>>
{
    using BaseClass = std::unordered_map<std::string, std::unique_ptr<const ConfigBindingBase<C>>>;
    using ValueType = std::pair<const std::string, std::unique_ptr<const ConfigBindingBase<C>>>;
    using ConstructionType = std::pair<const char*, const ConfigBindingBase<C>*>;

public:
    ConfigMap(std::initializer_list<ConstructionType> list)
    {
        for (ConstructionType val : list) {
            BaseClass::operator[](std::string(val.first)) =
                std::unique_ptr<const ConfigBindingBase<C>>(val.second);
        }
    }

    const std::unique_ptr<const ConfigBindingBase<C>>& getConfig(const char* name) const
    {
        // Default base that always returns false
        static const std::unique_ptr<const ConfigBindingBase<C>> defaultBase =
            std::make_unique<ConfigBindingBase<C>>();
        if (BaseClass::count(name) == 0) {
            return defaultBase;
        }

        return BaseClass::at(name);
    }
};

#endif // VN_API_CONFIG_MAP_H_
