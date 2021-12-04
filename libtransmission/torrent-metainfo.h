/*
 * This file Copyright (C) 2005-2014 Mnemosyne LLC
 *
 * It may be used under the GNU GPL versions 2 or 3
 * or any future license endorsed by Mnemosyne LLC.
 *
 */

#pragma once

#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "transmission.h"

#include "block-info.h"
#include "quark.h"

struct tr_error;
struct tr_info;
struct tr_variant;

struct tr_new_magnet_metainfo
{
public:
    bool parseMagnet(std::string_view magnet_link, tr_error** error = nullptr);
    std::string magnet() const;
    virtual ~tr_new_magnet_metainfo() = default;

    auto const& infoHash() const
    {
        return info_hash_;
    }
    auto const& name() const
    {
        return name_;
    }
    auto const& tiers() const
    {
        return tiers_;
    }
    auto const& webseeds() const
    {
        return webseed_urls_;
    }
    std::string_view infoHashString() const;

    virtual tr_info* toInfo() const;

    static bool convertAnnounceToScrape(std::string& setme, std::string_view announce_url);

protected:
    struct tracker_t
    {
        std::string_view announce_url_str;
        std::string_view scrape_url_str;
        tr_quark announce_url;
        tr_quark scrape_url;

        tracker_t(tr_quark announce_in, tr_quark scrape_in)
            : announce_url_str{ tr_quark_get_string_view(announce_in) }
            , scrape_url_str{ tr_quark_get_string_view(scrape_in) }
            , announce_url{ announce_in }
            , scrape_url{ scrape_in }
        {
        }

        bool operator<(tracker_t const& that) const
        {
            return announce_url < that.announce_url;
        }
    };

    enum class FilenameFormat
    {
        NameAndParitalHash,
        FullHash
    };

    std::string makeFilename(std::string_view dirname, FilenameFormat format, std::string_view suffix) const;

    using tier_t = std::set<tracker_t>;
    std::vector<tier_t> tiers_;
    std::vector<std::string> webseed_urls_;
    std::string name_;
    tr_sha1_digest_string_t info_hash_chars_;
    tr_sha1_digest_t info_hash_;
};

struct tr_torrent_metainfo : public tr_new_magnet_metainfo
{
public:
    ~tr_torrent_metainfo() override = default;
    // Helper funciton wrapper around parseBenc().
    // If you're looping through several files, passing in a non-nullptr
    // `buffer` can reduce the number of memory allocations needed to
    // load multiple files.
    bool parseTorrentFile(std::string_view benc_filename, std::vector<char>* buffer = nullptr, tr_error** error = nullptr);
    bool parseBenc(std::string_view benc, tr_error** error = nullptr);

    auto const& blockInfo() const
    {
        return block_info_;
    }
    auto const& comment() const
    {
        return comment_;
    }
    auto const& creator() const
    {
        return creator_;
    }
    auto const& files() const
    {
        return files_;
    }
    auto const& isPrivate() const
    {
        return is_private_;
    }
    auto const& parsedTorrentFile() const
    {
        return torrent_file_;
    }
    auto const& pieces() const
    {
        return pieces_;
    }
    auto const& source() const
    {
        return source_;
    }
    auto const& dateCreated() const
    {
        return date_created_;
    }

    tr_info* toInfo() const final;

private:
    static char* parsePath(std::string_view root, tr_variant* path, std::string& buf);
    static std::string fixWebseedUrl(tr_torrent_metainfo const& tm, std::string_view url);
    static std::string_view parseFiles(tr_torrent_metainfo& setme, tr_variant* info_dict, uint64_t* setme_total_size);
    static std::string_view parseImpl(tr_torrent_metainfo& setme, tr_variant* meta, std::string_view benc);
    static void parseAnnounce(tr_torrent_metainfo& setme, tr_variant* meta);
    static void parseWebseeds(tr_torrent_metainfo& setme, tr_variant* meta);

    struct file_t
    {
    public:
        std::string const& path() const
        {
            return path_;
        }
        uint64_t length() const
        {
            return length_;
        }

        file_t(std::string_view path, uint64_t length)
            : path_{ path }
            , length_{ length }
        {
        }

    private:
        std::string path_;
        uint64_t length_ = 0;
    };

    tr_block_info block_info_ = tr_block_info{ 0, 0 };

    std::vector<tr_sha1_digest_t> pieces_;
    std::vector<file_t> files_;

    std::string comment_;
    std::string creator_;
    std::string source_;

    // empty unless `parseTorrentFile()` was used
    std::string torrent_file_;

    time_t date_created_ = 0;

    // Location of the bencoded info dict in the entire bencoded torrent data.
    // Used when loading pieces of it to sent to magnet peers.
    // See http://bittorrent.org/beps/bep_0009.html
    uint64_t info_dict_size_ = 0;
    uint64_t info_dict_offset_ = 0;

    // Location of the bencoded 'pieces' checksums in the entire bencoded
    // torrent data. Used when loading piece checksums on demand.
    uint64_t pieces_offset_ = 0;

    bool is_private_ = false;
};

// FIXME(ckerr): move the rest of this file to a private header OR REMOVE
#if 0

void tr_metainfoRemoveSaved(tr_session const* session, tr_info const* info);

/** @brief Private function that's exposed here only for unit tests */
bool tr_metainfoAppendSanitizedPathComponent(std::string& out, std::string_view in, bool* is_adjusted);

// FIXME(ckerr): remove
struct tr_metainfo_parsed
{
    tr_info info = {};
    uint64_t info_dict_length = 0;
    std::vector<tr_sha1_digest_t> pieces;

    tr_metainfo_parsed() = default;

    tr_metainfo_parsed(tr_metainfo_parsed&& that) noexcept
    {
        std::swap(this->info, that.info);
        std::swap(this->pieces, that.pieces);
        std::swap(this->info_dict_length, that.info_dict_length);
    }

    tr_metainfo_parsed(tr_metainfo_parsed const&) = delete;

    tr_metainfo_parsed& operator=(tr_metainfo_parsed const&) = delete;

    ~tr_metainfo_parsed()
    {
        tr_metainfoFree(&info);
    }
};

// FIXME(ckerr): remove
std::optional<tr_metainfo_parsed> tr_metainfoParse(tr_session const* session, tr_variant const* variant, tr_error** error);

void tr_metainfoMigrateFile(
    tr_session const* session,
    tr_info const* info,
    tr_torrent_metainfo::FilenameFormat old_format,
    tr_torrent_metainfo::FilenameFormat new_format);
#endif
