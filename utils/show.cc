/*
 * This file Copyright (C) 2012-2014 Mnemosyne LLC
 *
 * It may be used under the GNU GPL versions 2 or 3
 * or any future license endorsed by Mnemosyne LLC.
 *
 */

#include <array>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <string_view>
#include <algorithm>

#include <curl/curl.h>

#include <event2/buffer.h>

#include <libtransmission/transmission.h>

#include <libtransmission/error.h>
#include <libtransmission/torrent-metainfo.h>
#include <libtransmission/tr-getopt.h>
#include <libtransmission/tr-macros.h>
#include <libtransmission/utils.h>
#include <libtransmission/version.h>
#include <libtransmission/web-utils.h>

#include "units.h"

#define MY_NAME "transmission-show"
#define TIMEOUT_SECS 30

namespace
{

auto options = std::array<tr_option, 5>{
    { { 'm', "magnet", "Give a magnet link for the specified torrent", "m", false, nullptr },
      { 's', "scrape", "Ask the torrent's trackers how many peers are in the torrent's swarm", "s", false, nullptr },
      { 'u', "unsorted", "Do not sort files by name", "u", false, nullptr },
      { 'V', "version", "Show version number and exit", "V", false, nullptr },
      { 0, nullptr, nullptr, nullptr, false, nullptr } }
};

char const* getUsage()
{
    return "Usage: " MY_NAME " [options] <.torrent file>";
}

auto filename_opt = std::string_view{};
auto magnet_opt = bool{ false };
auto scrape_opt = bool{ false };
auto show_version_opt = bool{ false };
auto unsorted_opt = bool{ false };

int parseCommandLine(int argc, char const* const* argv)
{
    int c;
    char const* optarg;

    while ((c = tr_getopt(getUsage(), argc, argv, std::data(options), &optarg)) != TR_OPT_DONE)
    {
        switch (c)
        {
        case 'm':
            magnet_opt = true;
            break;

        case 's':
            scrape_opt = true;
            break;

        case 'u':
            unsorted_opt = true;
            break;

        case 'V':
            show_version_opt = true;
            break;

        case TR_OPT_UNK:
            filename_opt = optarg;
            break;

        default:
            return 1;
        }
    }

    return 0;
}

auto toString(time_t timestamp)
{
    if (timestamp == 0)
    {
        return std::string{ "Unknown" };
    }

    struct tm tm;
    tr_localtime_r(&timestamp, &tm);
    auto buf = std::array<char, 64>{};
    strftime(std::data(buf), std::size(buf), "%a %b %d %T %Y%n", &tm); /* ctime equiv */
    return std::string{ std::data(buf) };
}

void showInfo(tr_torrent_metainfo const& metainfo)
{
    auto buf = std::array<char, 128>{};

    /**
    ***  General Info
    **/

    printf("GENERAL\n\n");
    printf("  Name: %s\n", metainfo.name().c_str());
    printf("  Hash: %" TR_PRIsv "\n", TR_PRIsv_ARG(metainfo.infoHashString()));
    printf("  Created by: %s\n", std::empty(metainfo.creator()) ? "Unknown" : metainfo.creator().c_str());
    printf("  Created on: %s\n", toString(metainfo.dateCreated()).c_str());

    if (!std::empty(metainfo.source()))
    {
        printf("  Comment: %s\n", metainfo.source().c_str());
    }

    if (!std::empty(metainfo.comment()))
    {
        printf("  Comment: %s\n", metainfo.comment().c_str());
    }

    printf("  Piece Count: %" PRIu64 "\n", metainfo.blockInfo().n_pieces);
    printf("  Piece Size: %s\n", tr_formatter_mem_B(std::data(buf), metainfo.blockInfo().piece_size, std::size(buf)));
    printf("  Total Size: %s\n", tr_formatter_size_B(std::data(buf), metainfo.blockInfo().total_size, std::size(buf)));
    printf("  Privacy: %s\n", metainfo.isPrivate() ? "Private torrent" : "Public torrent");

    /**
    ***  Trackers
    **/

    printf("\nTRACKERS\n");

    int tier_number = 1;
    for (auto const& tier : metainfo.tiers())
    {
        printf("\n  Tier #%d\n", tier_number);
        ++tier_number;

        for (auto const& tracker : tier)
        {
            printf("  %" TR_PRIsv "\n", TR_PRIsv_ARG(tracker.announce_url_str));
        }
    }

    /**
    ***
    **/

    auto const& webseeds = metainfo.webseeds();
    if (!std::empty(webseeds))
    {
        printf("\nWEBSEEDS\n\n");

        for (auto const& webseed : webseeds)
        {
            printf("  %s\n", webseed.c_str());
        }
    }

    /**
    ***  Files
    **/

    printf("\nFILES\n\n");

    auto filenames = std::vector<std::string>{};
    for (auto const& file : metainfo.files())
    {
        std::string filename = file.path();
        filename += " (";
        filename += tr_formatter_size_B(std::data(buf), file.length(), std::size(buf));
        filename += ')';
        filenames.emplace_back(filename);
    }

    if (!unsorted_opt)
    {
        std::sort(std::begin(filenames), std::end(filenames));
    }

    for (auto const& filename : filenames)
    {
        printf("  %s\n", filename.c_str());
    }
}

#if 0
size_t writeFunc(void* ptr, size_t size, size_t nmemb, void* vbuf)
{
    auto* buf = static_cast<evbuffer*>(vbuf);
    size_t const byteCount = size * nmemb;
    evbuffer_add(buf, ptr, byteCount);
    return byteCount;
}

CURL* tr_curl_easy_init(struct evbuffer* writebuf)
{
    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_USERAGENT, MY_NAME "/" LONG_VERSION_STRING);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunc);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, writebuf);
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, tr_env_key_exists("TR_CURL_VERBOSE"));
    curl_easy_setopt(curl, CURLOPT_ENCODING, "");
    return curl;
}
#endif

void doScrape(tr_torrent_metainfo const& /*metainfo*/)
{
    // FIXME
#if 0
    for (unsigned int i = 0; i < inf->trackerCount; ++i)
    {
        CURL* curl;
        CURLcode res;
        struct evbuffer* buf;
        char const* scrape = inf->trackers[i].scrape;
        char* url;
        auto escaped = std::array<SHA_DIGEST_LENGTH * 3 + 1, char>{};

        if (scrape == nullptr)
        {
            continue;
        }

        tr_http_escape_sha1(std::data(escaped), inf->hash);

        url = tr_strdup_printf("%s%cinfo_hash=%s", scrape, strchr(scrape, '?') != nullptr ? '&' : '?', escaped);

        printf("%s ... ", url);
        fflush(stdout);

        buf = evbuffer_new();
        curl = tr_curl_easy_init(buf);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT_SECS);

        if ((res = curl_easy_perform(curl)) != CURLE_OK)
        {
            printf("error: %s\n", curl_easy_strerror(res));
        }
        else
        {
            long response;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);

            if (response != 200)
            {
                printf("error: unexpected response %ld \"%s\"\n", response, tr_webGetResponseStr(response));
            }
            else /* HTTP OK */
            {
                tr_variant top;
                tr_variant* files;
                bool matched = false;
                char const* begin = (char const*)evbuffer_pullup(buf, -1);
                auto sv = std::string_view{ begin, evbuffer_get_length(buf) };
                if (tr_variantFromBuf(&top, TR_VARIANT_PARSE_BENC | TR_VARIANT_PARSE_INPLACE, sv))
                {
                    if (tr_variantDictFindDict(&top, TR_KEY_files, &files))
                    {
                        size_t child_pos = 0;
                        tr_quark key;
                        tr_variant* val;

                        while (tr_variantDictChild(files, child_pos, &key, &val))
                        {
                            if (memcmp(inf->hash, tr_quark_get_string(key), SHA_DIGEST_LENGTH) == 0)
                            {
                                int64_t seeders;
                                if (!tr_variantDictFindInt(val, TR_KEY_complete, &seeders))
                                {
                                    seeders = -1;
                                }

                                int64_t leechers;
                                if (!tr_variantDictFindInt(val, TR_KEY_incomplete, &leechers))
                                {
                                    leechers = -1;
                                }

                                printf("%d seeders, %d leechers\n", (int)seeders, (int)leechers);
                                matched = true;
                            }

                            ++child_pos;
                        }
                    }

                    tr_variantFree(&top);
                }

                if (!matched)
                {
                    printf("no match\n");
                }
            }
        }

        curl_easy_cleanup(curl);
        evbuffer_free(buf);
        tr_free(url);
    }
#endif
}

} // namespace

int tr_main(int argc, char* argv[])
{
    tr_logSetLevel(TR_LOG_ERROR);
    tr_formatter_mem_init(MEM_K, MEM_K_STR, MEM_M_STR, MEM_G_STR, MEM_T_STR);
    tr_formatter_size_init(DISK_K, DISK_K_STR, DISK_M_STR, DISK_G_STR, DISK_T_STR);
    tr_formatter_speed_init(SPEED_K, SPEED_K_STR, SPEED_M_STR, SPEED_G_STR, SPEED_T_STR);

    if (parseCommandLine(argc, (char const* const*)argv) != 0)
    {
        return EXIT_FAILURE;
    }

    if (show_version_opt)
    {
        fprintf(stderr, "%s %s\n", MY_NAME, LONG_VERSION_STRING);
        return EXIT_SUCCESS;
    }

    /* make sure the user specified a filename */
    if (std::empty(filename_opt))
    {
        fprintf(stderr, "ERROR: No .torrent file specified.\n");
        tr_getopt_usage(MY_NAME, getUsage(), std::data(options));
        fprintf(stderr, "\n");
        return EXIT_FAILURE;
    }

    /* try to parse the .torrent file */
    auto metainfo = tr_torrent_metainfo{};
    tr_error* error = nullptr;
    auto const parsed = metainfo.parseTorrentFile(filename_opt, nullptr, &error);
    if (error != nullptr)
    {
        fprintf(
            stderr,
            "Error parsing .torrent file \"%" TR_PRIsv "\": %s (%d)\n",
            TR_PRIsv_ARG(filename_opt),
            error->message,
            error->code);
        tr_error_clear(&error);
    }
    if (!parsed)
    {
        return EXIT_FAILURE;
    }

    if (magnet_opt)
    {
        printf("%s", metainfo.magnet().c_str());
    }
    else
    {
        printf("Name: %s\n", metainfo.name().c_str());
        printf("File: %" TR_PRIsv "\n", TR_PRIsv_ARG(filename_opt));
        printf("\n");
        fflush(stdout);

        if (scrape_opt)
        {
            doScrape(metainfo);
        }
        else
        {
            showInfo(metainfo);
        }
    }

    /* cleanup */
    putc('\n', stdout);
    return EXIT_SUCCESS;
}
