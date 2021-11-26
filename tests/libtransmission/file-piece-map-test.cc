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

class FilePieceMapTest : public ::testing::Test
{
protected:
    static constexpr size_t TotalSize{ 1001 };
    static constexpr size_t PieceSize{ 100 };
    tr_block_info const block_info_{ TotalSize, PieceSize };

    static constexpr std::array<uint64_t, 17> FileSizes{
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

    void SetUp() override
    {
        EXPECT_EQ(11, block_info_.n_pieces);
        EXPECT_EQ(PieceSize, block_info_.piece_size);
        EXPECT_EQ(TotalSize, block_info_.total_size);
        EXPECT_EQ(TotalSize, std::accumulate(std::begin(FileSizes), std::end(FileSizes), uint64_t{ 0 }));
    }
};

TEST_F(FilePieceMapTest, pieceSpan)
{
    // Note to reviewers: it's easy to see a nonexistent fencepost error here.
    // Remember everything is zero-indexed, so the 11 valid pieces are [0..10]
    // and that last piece #10 has one byte in it. Piece #11 is the 'end' iterator position.
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
    EXPECT_EQ(std::size(FileSizes), std::size(ExpectedPieceSpans));

    auto const fpm = tr_file_piece_map{ block_info_, std::data(FileSizes), std::size(FileSizes) };
    tr_file_index_t const n = std::size(fpm);
    EXPECT_EQ(std::size(FileSizes), n);
    uint64_t offset = 0;
    for (tr_file_index_t file = 0; file < n; ++file)
    {
        EXPECT_EQ(ExpectedPieceSpans[file].begin, fpm.pieceSpan(file).begin);
        EXPECT_EQ(ExpectedPieceSpans[file].end, fpm.pieceSpan(file).end);
        offset += FileSizes[file];
    }
    EXPECT_EQ(TotalSize, offset);
    EXPECT_EQ(block_info_.n_pieces, fpm.pieceSpan(std::size(FileSizes) - 1).end);
}

TEST_F(FilePieceMapTest, priorities)
{
    auto const fpm = tr_file_piece_map{ block_info_, std::data(FileSizes), std::size(FileSizes) };
    auto file_priorities = tr_file_priorities(fpm);
    tr_file_index_t const n_files = std::size(FileSizes);

    // make a helper to compare file & piece priorities
    auto expected_file_priorities = std::vector<tr_priority_t>(n_files, TR_PRI_NORMAL);
    auto expected_piece_priorities = std::vector<tr_priority_t>(block_info_.n_pieces, TR_PRI_NORMAL);
    auto const compare_to_expected = [&, this]()
    {
        for (tr_file_index_t i = 0; i < n_files; ++i)
        {
            std::cerr << __FILE__ << ':' << __LINE__ << " file " << i << std::endl;
            EXPECT_EQ(int(expected_file_priorities[i]), int(file_priorities.filePriority(i)));
        }
        for (tr_piece_index_t i = 0; i < block_info_.n_pieces; ++i)
        {
            std::cerr << __FILE__ << ':' << __LINE__ << " piece " << i << std::endl;
            EXPECT_EQ(int(expected_piece_priorities[i]), int(file_priorities.piecePriority(i)));
        }
    };

    // check default priority is normal
    std::cerr << __FILE__ << ':' << __LINE__ << std::endl;
    compare_to_expected();

    // set the first file as high priority.
    // since this begins and ends on a piece boundary,
    // this shouldn't affect any other files' pieces
    auto pri = TR_PRI_HIGH;
    file_priorities.set(0, pri);
    expected_file_priorities[0] = pri;
    for (size_t i = 0; i < 5; ++i)
    {
        expected_piece_priorities[i] = pri;
    }
    std::cerr << __FILE__ << ':' << __LINE__ << std::endl;
    compare_to_expected();

    // This file shares a piece with another file.
    // If _either_ is set to high, the piece's priority should be high.
    // file #5: byte [500..550) piece [5, 6)
    // file #6: byte [550..650) piece [5, 7)
    //
    // first test setting file #5...
    pri = TR_PRI_HIGH;
    file_priorities.set(5, pri);
    expected_file_priorities[5] = pri;
    expected_piece_priorities[5] = pri;
    std::cerr << __FILE__ << ':' << __LINE__ << std::endl;
    compare_to_expected();
    // ...and that shared piece should still be the same when both are high...
    file_priorities.set(6, pri);
    expected_file_priorities[6] = pri;
    expected_piece_priorities[5] = pri;
    expected_piece_priorities[6] = pri;
    std::cerr << __FILE__ << ':' << __LINE__ << std::endl;
    compare_to_expected();
    // ...and that shared piece should still be the same when only 6 is high...
    pri = TR_PRI_NORMAL;
    file_priorities.set(5, pri);
    expected_file_priorities[5] = pri;
    std::cerr << __FILE__ << ':' << __LINE__ << std::endl;
    compare_to_expected();

    // setup for the next test: set all files to low priority
    pri = TR_PRI_LOW;
    for (tr_file_index_t i = 0; i < n_files; ++i)
    {
        file_priorities.set(i, pri);
    }
    std::fill(std::begin(expected_file_priorities), std::end(expected_file_priorities), pri);
    std::fill(std::begin(expected_piece_priorities), std::end(expected_piece_priorities), pri);
    std::cerr << __FILE__ << ':' << __LINE__ << std::endl;
    compare_to_expected();

    // Raise the priority of a small 1-piece file.
    // Since it's the highest priority in the piece, piecePriority() should return its value.
    // file #8: byte [650, 659) piece [6, 7)
    pri = TR_PRI_NORMAL;
    file_priorities.set(8, pri);
    expected_file_priorities[8] = pri;
    expected_piece_priorities[6] = pri;
    std::cerr << __FILE__ << ':' << __LINE__ << std::endl;
    compare_to_expected();
    // Raise the priority of another small 1-piece file in the same piece.
    // Since _it_ now has the highest priority in the piece, piecePriority should return _its_ value.
    // file #9: byte [659, 667) piece [6, 7)
    pri = TR_PRI_HIGH;
    file_priorities.set(9, pri);
    expected_file_priorities[9] = pri;
    expected_piece_priorities[6] = pri;
    std::cerr << __FILE__ << ':' << __LINE__ << std::endl;
    compare_to_expected();

    // Prep for the next test: set all files to normal priority
    pri = TR_PRI_NORMAL;
    for (tr_file_index_t i = 0; i < n_files; ++i)
    {
        file_priorities.set(i, pri);
    }
    std::fill(std::begin(expected_file_priorities), std::end(expected_file_priorities), pri);
    std::fill(std::begin(expected_piece_priorities), std::end(expected_piece_priorities), pri);
    std::cerr << __FILE__ << ':' << __LINE__ << std::endl;
    compare_to_expected();

    // *Sigh* OK what happens to piece priorities if you set the priority
    // of a zero-byte file. Arguably nothing should happen since you can't
    // download an empty file. But that would complicate the code for a
    // pretty stupid use case, and treating 0-sized files the same as any
    // other does no real harm. Let's KISS.
    //
    // Check that even zero-sized files can change a piece's priority
    // file #1: byte [500, 500) piece [5, 6)
    pri = TR_PRI_HIGH;
    file_priorities.set(1, pri);
    expected_file_priorities[1] = pri;
    expected_piece_priorities[5] = pri;
    std::cerr << __FILE__ << ':' << __LINE__ << std::endl;
    compare_to_expected();
    // Check that zero-sized files at the end of a torrent change the last piece's priority.
    // file #16 byte [1001, 1001) piece [10, 11)
    file_priorities.set(16, pri);
    expected_file_priorities[16] = pri;
    expected_piece_priorities[10] = pri;
    std::cerr << __FILE__ << ':' << __LINE__ << std::endl;
    compare_to_expected();
}

#if 0
    static constexpr std::array<uint64_t, 17> FileSizes {
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
}


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
