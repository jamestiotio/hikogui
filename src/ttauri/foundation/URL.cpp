// Copyright 2019 Pokitec
// All rights reserved.

#include "ttauri/foundation/URL.hpp"
#include "ttauri/foundation/globals.hpp"
#include "ttauri/foundation/strings.hpp"
#include "ttauri/foundation/required.hpp"
#include "ttauri/foundation/url_parser.hpp"
#include "ttauri/foundation/glob.hpp"
#include "ttauri/foundation/Unicode.hpp"
#include "ttauri/foundation/FileView.hpp"
#include "ttauri/foundation/StaticResourceView.hpp"
#include <regex>

namespace tt {

URL::URL(std::string_view url) :
    value(normalize_url(url))
{
}

URL::URL(char const *url) :
    value(normalize_url(url))
{
}

URL::URL(std::string const &url) :
    value(normalize_url(url))
{
}

URL::URL(url_parts const &parts) :
    value(generate_url(parts))
{
}

size_t URL::hash() const noexcept
{
    return std::hash<std::string>{}(value);
}

std::string URL::string() const noexcept
{
    return value;
}

std::string_view URL::scheme() const noexcept
{
    return parse_url(value).scheme;
}

std::string URL::query() const noexcept
{
    return url_decode(parse_url(value).query, true);
}

std::string URL::fragment() const noexcept
{
    return url_decode(parse_url(value).fragment);
}

std::string URL::filename() const noexcept
{
    ttlet parts = parse_url(value);
    if (parts.segments.size() > 0) {
        return url_decode(parts.segments.back());
    } else {
        return {};
    }
}

std::string URL::directory() const noexcept
{
    auto parts = parse_url(value);
    if (parts.segments.size() > 0) {
        parts.segments.pop_back();
    }
    return generate_path(parts);
}

std::string URL::nativeDirectory() const noexcept
{
    auto parts = parse_url(value);
    if (parts.segments.size() > 0) {
        parts.segments.pop_back();
    }
    return generate_native_path(parts);
}

std::string URL::extension() const noexcept
{
    ttlet fn = filename();
    ttlet i = fn.rfind('.');
    return fn.substr((i != fn.npos) ? (i + 1) : fn.size());
}

std::vector<std::string> URL::pathSegments() const noexcept
{
    ttlet parts = parse_url(value);
    return transform<std::vector<std::string>>(parts.segments, [](auto x) {
        return url_decode(x);
        });
}

std::string URL::path() const noexcept
{
    return generate_path(parse_url(value));
}

std::string URL::nativePath() const noexcept
{
    return generate_native_path(parse_url(value));
}

std::wstring URL::nativeWPath() const noexcept
{
    return to_wstring(nativePath());
}

bool URL::isAbsolute() const noexcept
{
    return parse_url(value).absolute;
}

bool URL::isRelative() const noexcept
{
    return !isAbsolute();
}

bool URL::isRootDirectory() const noexcept
{
    return parse_url(value).segments.size() == 0;
}


URL URL::urlByAppendingPath(URL const &other) const noexcept
{
    ttlet this_parts = parse_url(value);
    ttlet other_parts = parse_url(other.value);
    ttlet new_parts = concatenate_url_parts(this_parts, other_parts);
    return URL(new_parts);
}

URL URL::urlByAppendingPath(std::string_view const other) const noexcept
{
    return urlByAppendingPath(URL::urlFromPath(other));
}

URL URL::urlByAppendingPath(std::string const &other) const noexcept
{
    return urlByAppendingPath(URL::urlFromPath(other));
}

URL URL::urlByAppendingPath(char const *other) const noexcept
{
    return urlByAppendingPath(URL::urlFromPath(other));
}

URL URL::urlByAppendingPath(std::wstring_view const other) const noexcept
{
    return urlByAppendingPath(URL::urlFromWPath(other));
}

URL URL::urlByAppendingPath(std::wstring const &other) const noexcept
{
    return urlByAppendingPath(URL::urlFromWPath(other));
}

URL URL::urlByAppendingPath(wchar_t const *other) const noexcept
{
    return urlByAppendingPath(URL::urlFromWPath(other));
}

URL URL::urlByRemovingFilename() const noexcept
{
    auto parts = parse_url(value);
    if (parts.segments.size() > 0) {
        parts.segments.pop_back();
    }
    return URL(parts);
}

static void urlsByRecursiveScanning(std::string const &base, glob_token_list_t const &glob, std::vector<URL> &result) noexcept
{
    for (ttlet &filename: URL::filenamesByScanningDirectory(base)) {
        if (filename.back() == '/') {
            ttlet directory = std::string_view(filename.data(), filename.size() - 1);
            auto recursePath = base + "/";
            recursePath += directory;

            if (matchGlob(glob, recursePath) != glob_match_result_t::No) {
                urlsByRecursiveScanning(recursePath, glob, result);
            }

        } else {
            ttlet finalPath = base + '/' + filename;
            if (matchGlob(glob, finalPath) == glob_match_result_t::Match) {
                result.push_back(URL::urlFromPath(finalPath));
            }
        }
    }
}

std::vector<URL> URL::urlsByScanningWithGlobPattern() const noexcept
{
    ttlet glob = parseGlob(path());
    ttlet basePath = basePathOfGlob(glob);

    std::vector<URL> urls;
    urlsByRecursiveScanning(basePath, glob, urls);
    return urls;
}

URL URL::urlFromPath(std::string_view const path) noexcept
{
    std::string tmp;
    ttlet parts = parse_path(path, tmp);
    return URL(parts);
}

URL URL::urlFromWPath(std::wstring_view const path) noexcept
{
    return urlFromPath(to_string(path));
}

URL URL::urlFromExecutableDirectory() noexcept
{
    static auto r = urlFromExecutableFile().urlByRemovingFilename();
    return r;
}

URL URL::urlFromApplicationLogDirectory() noexcept
{
    return urlFromApplicationDataDirectory() / "Log";
}

std::ostream& operator<<(std::ostream& lhs, const URL& rhs)
{
    lhs << rhs.string();
    return lhs;
}

std::string URL::nativePathFromPath(std::string_view path) noexcept
{
    std::string r = static_cast<std::string>(path);

    for (auto &c: r) {
        if (c == '/') {
            c = native_path_seperator;
        }
    }
    return r;
}

std::wstring URL::nativeWPathFromPath(std::string_view path) noexcept
{
    return to_wstring(nativePathFromPath(path));
}

std::unique_ptr<ResourceView> URL::loadView() const
{
    if (scheme() == "resource") {
        try {
            auto view = StaticResourceView::loadView(filename());
            LOG_INFO("Loaded resource {} from executable.", *this);
            return view;

        } catch (key_error) {
            ttlet absoluteLocation = URL::urlFromResourceDirectory() / *this;
            auto view = FileView::loadView(absoluteLocation);
            LOG_INFO("Loaded resource {} from filesystem at {}.", *this, absoluteLocation);
            return view;
        }

    } else if (scheme() == "file") {
        auto view = FileView::loadView(*this);
        LOG_INFO("Loaded resource {} from filesystem.", *this);
        return view;

    } else {
        TTAURI_THROW(url_error("Unknown scheme for loading a resource")
            .set<url_tag>(*this)
        );
    }
}

}
