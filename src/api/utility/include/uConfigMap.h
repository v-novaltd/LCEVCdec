/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */
#ifndef VN_UTIL_CONFIGURER_H_
#define VN_UTIL_CONFIGURER_H_

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
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

// Templated binding for scalar config memebers
//
template <typename C, typename T>
struct ConfigBinding : public ConfigBindingBase<C>
{
    ConfigBinding(T C::*ptr)
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

// Templated binding for vector config memebers
//
template <typename C, typename T, size_t N>
struct ConfigBindingArr : public ConfigBindingBase<C>
{
    ConfigBindingArr(T (C::*ptr)[N])
        : memberPointer(ptr)
    {}

    bool set(C& c, std::vector<T>& val) const
    {
        if (val.size() != N) {
            return false;
        }
        std::copy(val.begin(), val.end(), c.*memberPointer);
        return true;
    }

    T (C::*memberPointer)[N];
};

template <typename C, typename T, size_t N>
const ConfigBindingBase<C>* makeBindingArray(T (C::*ptr)[N])
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

#endif // VN_UTIL_CONFIGURER_H_
