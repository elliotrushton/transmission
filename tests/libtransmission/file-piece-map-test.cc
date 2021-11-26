/*
 * This file Copyright (C) Mnemosyne LLC
 *
 * It may be used under the GNU GPL versions 2 or 3
 * or any future license endorsed by Mnemosyne LLC.
 *
 */

#include <array>
#include <numeric>
#include <cstdint>

#include "transmission.h"

#include "block-info.h"
#include "file-piece-map.h"

#include "gtest/gtest.h"

using FilePieceMapTest = ::testing::Test;

// 0..99 is piece 0
// 100.. is piece 1
// 1000.. is piece 10
TEST_F(FilePieceMapTest, pieceSpan)
{
    auto constexpr TotalSize = size_t{ 1001 };
    auto constexpr PieceSize = size_t{ 100 };
    auto const block_info = tr_block_info{ TotalSize, PieceSize };
    EXPECT_EQ(11, block_info.n_pieces);
    EXPECT_EQ(PieceSize, block_info.piece_size);
    EXPECT_EQ(TotalSize, block_info.total_size);

    auto file_sizes = std::vector<uint64_t>{
        500, // [offset 0] begins and ends on a piece boundary
        0, // [offset 500] zero-sized files
        0,   0, 0,
        50, // [offset 500] begins on a piece boundary
        100, // [offset 550] neither begins nor ends on a piece boundary, spans >1 piece
        10, // [offset 650] small files all contained in a single piece
        9,   8, 7, 6,
        311, // [offset 690] ends end-of-torrent
        0, // [offset 1001] zero-sized files at the end-of-torrent
        0,   0, 0,
        // sum is 1001 == TotalSize
    };
    EXPECT_EQ(TotalSize, std::accumulate(std::begin(file_sizes), std::end(file_sizes), uint64_t{ 0 }));

    auto constexpr ExpectedPieceSpans = std::array<tr_file_piece_map::piece_span_t, 17>{ {
        { 0, 5 },
        { 5, 6 },
        { 5, 6 },
        { 5, 6 },
        { 5, 6 },
        { 5, 6 },
        { 5, 7 },
        { 6, 7 },
        { 6, 7 },
        { 6, 7 },
        { 6, 7 },
        { 6, 7 },
        { 6, 11 },
        { 10, 11 },
        { 10, 11 },
        { 10, 11 },
        { 10, 11 },
    } };

    EXPECT_EQ(std::size(file_sizes), std::size(ExpectedPieceSpans));

    auto const fpm = tr_file_piece_map{ block_info, file_sizes };
    tr_file_index_t const n = std::size(fpm);
    EXPECT_EQ(std::size(file_sizes), n);
    uint64_t offset = 0;
    for (tr_file_index_t file = 0; file < n; ++file)
    {
        std::cerr << "offset " << offset << " size " << file_sizes[file] << " file " << file << std::endl;
        EXPECT_EQ(ExpectedPieceSpans[file].begin, fpm.pieceSpan(file).begin);
        EXPECT_EQ(ExpectedPieceSpans[file].end, fpm.pieceSpan(file).end);
        offset += file_sizes[file];
    }
    EXPECT_EQ(TotalSize, offset);
    EXPECT_EQ(block_info.n_pieces, fpm.pieceSpan(std::size(file_sizes) - 1).end);
}

#if 0
class tr_file_piece_map
{
public:
    template<typename T> struct index_span_t { T begin; T end; };
    using file_span_t = index_span_t<tr_file_index_t>;
    using piece_span_t = index_span_t<tr_piece_index_t>;

    tr_file_piece_map(tr_block_info const& block_info, std::vector<uint64_t> const& file_sizes);
    [[nodiscard]] piece_span_t pieceSpan(tr_file_index_t file) const;
    [[nodiscard]] file_span_t fileSpan(tr_piece_index_t piece) const;
    [[nodiscard]] size_t size() const { return std::size(files_); }

private:
    std::vector<piece_span_t> files_;
};

class tr_file_priorities
{
public:
    explicit tr_file_priorities(tr_file_piece_map const& fpm);
    void set(tr_file_index_t const* files, size_t n, tr_priority_t priority);

    [[nodiscard]] tr_priority_t filePriority(tr_file_index_t file) const;
    [[nodiscard]] tr_priority_t piecePriority(tr_piece_index_t piece) const;

private:
    tr_file_piece_map const& fpm_;
    std::vector<tr_priority_t> priorities_;
};

class tr_file_wanted
{
public:
    explicit tr_file_wanted(tr_file_piece_map const& fpm);
    void set(tr_file_index_t const* files, size_t n, bool wanted);

    [[nodiscard]] tr_priority_t fileWanted(tr_file_index_t file) const;
    [[nodiscard]] tr_priority_t pieceWanted(tr_piece_index_t piece) const;

private:
    tr_file_piece_map const& fpm_;
    tr_bitfield wanted_;
};
#endif
