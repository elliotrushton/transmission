/*
 * This file Copyright (C) Mnemosyne LLC
 *
 * It may be used under the GNU GPL versions 2 or 3
 * or any future license endorsed by Mnemosyne LLC.
 *
 */

#include <algorithm>
#include <iterator>
#include <vector>

#include "transmission.h"

#include "block-info.h"
#include "file-piece-map.h"

#include <iostream>

tr_file_piece_map::tr_file_piece_map(tr_block_info const& block_info, std::vector<uint64_t> const& file_sizes)
{
    tr_file_index_t const n = std::size(file_sizes);
    files_.resize(n);

    uint64_t offset = 0;
    for (tr_file_index_t i = 0; i < n; ++i)
    {
        auto const file_size = file_sizes[i];
        std::cerr << __FILE__ << ':' << __LINE__ << " file " << i << " offset " << offset << " size " << file_size << std::endl;
        auto const begin_piece = block_info.pieceOf(offset);
        std::cerr << __FILE__ << ':' << __LINE__ << " ...begin piece " << begin_piece << std::endl;
        tr_piece_index_t end_piece = 0;
        if (file_size != 0)
        {
            auto const last_byte = offset + file_size - 1;
            std::cerr << __FILE__ << ':' << __LINE__ << " ...final_byte " << last_byte << std::endl;
            auto const final_piece = block_info.pieceOf(last_byte);
            std::cerr << __FILE__ << ':' << __LINE__ << " ...final_piece " << final_piece << std::endl;
            end_piece = final_piece + 1;
        }
        else
        {
            end_piece = begin_piece + 1;
            std::cerr << __FILE__ << ':' << __LINE__ << " ...zero sized file" << std::endl;
        }
        std::cerr << __FILE__ << ':' << __LINE__ << " ...end piece " << end_piece << std::endl;
        files_[i] = piece_span_t{ begin_piece, end_piece };
        offset += file_size;
    }
}

tr_file_piece_map::piece_span_t tr_file_piece_map::pieceSpan(tr_file_index_t file) const
{
    return files_[file];
}

tr_file_piece_map::file_span_t tr_file_piece_map::fileSpan(tr_piece_index_t piece) const
{
    struct Compare
    {
        int compare(tr_piece_index_t piece, piece_span_t span) const // <=>
        {
            if (piece < span.begin)
            {
                return -1;
            }

            if (piece >= span.end)
            {
                return 1;
            }

            return 0;
        }

        bool operator()(tr_piece_index_t piece, piece_span_t span) const // <
        {
            return compare(piece, span) < 0;
        }

        int compare(piece_span_t span, tr_piece_index_t piece) const // <=>
        {
            return -compare(piece, span);
        }

        bool operator()(piece_span_t span, tr_piece_index_t piece) const // <
        {
            return compare(span, piece) < 0;
        }
    };

    auto const begin = std::begin(files_);
    auto const pair = std::equal_range(begin, std::end(files_), piece, Compare{});
    return { tr_piece_index_t(std::distance(begin, pair.first)), tr_piece_index_t(std::distance(begin, pair.second)) };
}

/***
****
***/

tr_file_priorities::tr_file_priorities(tr_file_piece_map const& fpm)
    : fpm_{ fpm }
{
    auto const n = std::size(fpm);
    priorities_.resize(n);
    priorities_.shrink_to_fit();
    std::fill_n(std::begin(priorities_), n, TR_PRI_NORMAL);
}

void tr_file_priorities::set(tr_file_index_t const* files, size_t n, tr_priority_t priority)
{
    for (size_t i = 0; i < n; ++i)
    {
        priorities_[files[i]] = priority;
    }
}

tr_priority_t tr_file_priorities::filePriority(tr_file_index_t file) const
{
    return priorities_[file];
}

tr_priority_t tr_file_priorities::piecePriority(tr_piece_index_t piece) const
{
    auto const [begin_idx, end_idx] = fpm_.fileSpan(piece);
    auto const begin = std::begin(priorities_) + begin_idx;
    auto const end = std::begin(priorities_) + end_idx;
    auto const it = std::max_element(begin, end);
    return it != end ? *it : TR_PRI_NORMAL;
}

/***
****
***/

tr_file_wanted::tr_file_wanted(tr_file_piece_map const& fpm)
    : fpm_{ fpm }
    , wanted_{ std::size(fpm) }
{
    wanted_.setHasAll(); // by default we want all files
}

void tr_file_wanted::set(tr_file_index_t const* files, size_t n, bool wanted)
{
    for (size_t i = 0; i < n; ++i)
    {
        wanted_.set(files[i], wanted);
    }
}

tr_priority_t tr_file_wanted::fileWanted(tr_file_index_t file) const
{
    return wanted_.test(file);
}

tr_priority_t tr_file_wanted::pieceWanted(tr_piece_index_t piece) const
{
    auto const [begin, end] = fpm_.fileSpan(piece);
    return wanted_.count(begin, end) != 0;
}
