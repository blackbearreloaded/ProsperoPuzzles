// ProsperoPuzzles - The app version, read from the packaged param.json.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/version.hpp"

#include "core/save_file.hpp"

#include <cctype>

namespace ppz
{

std::string content_version(std::string_view param_json)
{
    constexpr std::string_view kKey = "\"contentVersion\"";
    const std::size_t key = param_json.find(kKey);
    if (key == std::string_view::npos)
        return {};
    const std::size_t open = param_json.find('"', param_json.find(':', key + kKey.size()));
    if (open == std::string_view::npos)
        return {};
    const std::size_t close = param_json.find('"', open + 1);
    if (close == std::string_view::npos)
        return {};
    const std::string_view value = param_json.substr(open + 1, close - open - 1);
    // NN.NNN.NNN, as the build and release tooling require.
    if (value.size() != 10 || value[2] != '.' || value[6] != '.')
        return {};
    for (std::size_t i = 0; i < value.size(); ++i)
    {
        if (i != 2 && i != 6 && std::isdigit(static_cast<unsigned char>(value[i])) == 0)
            return {};
    }
    return std::string(value);
}

std::string title_id(std::string_view param_json)
{
    constexpr std::string_view kKey = "\"titleId\"";
    const std::size_t key = param_json.find(kKey);
    if (key == std::string_view::npos)
        return {};
    const std::size_t open = param_json.find('"', param_json.find(':', key + kKey.size()));
    if (open == std::string_view::npos)
        return {};
    const std::size_t close = param_json.find('"', open + 1);
    if (close == std::string_view::npos)
        return {};
    const std::string_view value = param_json.substr(open + 1, close - open - 1);
    if (value.size() != 9)
        return {};
    for (std::size_t i = 0; i < value.size(); ++i)
    {
        const char c = value[i];
        if (i < 4 ? c < 'A' || c > 'Z' : std::isdigit(static_cast<unsigned char>(c)) == 0)
            return {};
    }
    return std::string(value);
}

std::string read_content_version(const std::string &path)
{
    std::string text;
    return save::read_file(path, &text) ? content_version(text) : std::string();
}

} // namespace ppz
