// ProsperoPuzzles - Shared play screen for the native logic puzzles.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/tween.hpp"
#include "games/game_scene.hpp"
#include "ui/menu.hpp"
#include "ui/theme.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ppz::kit
{

// The look every native puzzle shares (2048 and Tenfold's palette).
namespace look
{
inline const gfx::Color kBoard = gfx::Color::rgb(0xe6e3dc);   // board card
inline const gfx::Color kWell = gfx::Color::rgb(0xd9d4ca);    // empty cell / slot
inline const gfx::Color kTile = gfx::Color::rgb(0xfffefa);    // raised tile
inline const gfx::Color kInk = gfx::Color::rgb(0x28334f);     // text and lines
inline const gfx::Color kInkSoft = gfx::Color::rgb(0x8a8fa3); // secondary marks
inline const gfx::Color kTileShadow = gfx::Color::rgb(0x394259, 0.18f);
inline const gfx::Color kError = gfx::Color::rgb(0xe0605e);
// Distinct hues for pieces, regions and colours, in a fixed order.
constexpr std::uint32_t kHues[] = {0x469fca, 0xed836d, 0x7eac59, 0xae75bb, 0xdf9a43, 0x64b8a6,
                                   0xd97799, 0x7775c5, 0x3c9290, 0xe0605e, 0xf0c555, 0x8a6f5a};
inline gfx::Color hue(int index)
{
    constexpr int count = static_cast<int>(sizeof(kHues) / sizeof(kHues[0]));
    return gfx::Color::rgb(kHues[((index % count) + count) % count]);
}
// A soft wash of a hue for region backgrounds.
gfx::Color wash(int index, float amount = 0.32f);
} // namespace look

// Where the board sits: centred between the title column and the stats panel.
constexpr gfx::Rect kBoardArea{580.0f, 236.0f, 760.0f, 720.0f};

// Square cells fitted into kBoardArea (or a custom area).
struct Grid
{
    int cols = 1;
    int rows = 1;
    float x = 0.0f;    // first cell's left edge
    float y = 0.0f;    // first cell's top edge
    float cell = 1.0f; // cell size
    float gap = 0.0f;  // space between cells
    gfx::Rect card{};  // the board card behind the cells

    gfx::Rect cell_rect(int col, int row) const
    {
        return {x + static_cast<float>(col) * (cell + gap),
                y + static_cast<float>(row) * (cell + gap), cell, cell};
    }
    // Fractional positions, for springs.
    gfx::Rect cell_rect(float col, float row) const
    {
        return {x + col * (cell + gap), y + row * (cell + gap), cell, cell};
    }
};
Grid fit_grid(int cols, int rows, float gap_fraction = 0.1f, gfx::Rect area = kBoardArea);

// Draws a raised rounded tile with its soft drop shadow (the Tenfold tile).
void draw_tile(gfx::DrawList &list, const gfx::Rect &r, gfx::Color fill,
               float radius_fraction = 0.16f);

struct PuzzleStats
{
    std::uint32_t played = 0;
    std::uint32_t solved = 0;
    std::array<std::uint32_t, 3> best_seconds{}; // per size; 0 = none yet
};

// Base class for the native puzzles: header, stats panel, board card, cursor
// ring, hint bar, pause and solved menus, sizes, undo/redo, timer and saves.
// A game supplies its rules and drawing through the hooks below.
class PuzzleScene : public games::GameScene
{
  public:
    struct Info
    {
        std::string id;
        std::string title;
        std::string subtitle; // one line under the title
        std::array<const char *, 3> sizes{"Small", "Medium", "Large"};
        int default_size = 0;
    };

    PuzzleScene(const ui::Fonts &fonts, Info info);

    void start(const std::string &save, const std::string &stats) override;
    games::SceneExit update(const InputFrame &input, float dt,
                            std::vector<audio::Cue> &cues) override;
    void draw(gfx::DrawList &list) const override;
    std::string save() override;
    std::string stats() override;
    bool in_progress() override;
    const std::string &id() const override
    {
        return info_.id;
    }

    // Test hooks.
    bool menu_open() const
    {
        return menu_.is_open();
    }
    bool is_solved() const
    {
        return solved_;
    }
    int size() const
    {
        return size_;
    }
    int cursor_col() const
    {
        return cursor_col_;
    }
    int cursor_row() const
    {
        return cursor_row_;
    }
    // Starts a puzzle from a fixed seed (tests, previews).
    void new_puzzle(std::uint64_t seed, int size);
    // Draws only the board card and its contents (library previews), and
    // where that card sits in 1080p coordinates.
    void draw_preview(gfx::DrawList &list) const;
    gfx::Rect preview_bounds() const
    {
        return grid().card;
    }

  protected:
    // ---- rules ----
    // Builds a fresh puzzle for size 0..2 from seed.
    virtual void generate(std::uint64_t seed, int size) = 0;
    // Clears the player's progress back to the generated puzzle.
    virtual void restart() = 0;
    // The whole game state (puzzle and progress) as bytes, and back.
    virtual std::string serialize() const = 0;
    virtual bool deserialize(std::string_view data) = 0;
    virtual bool solved() const = 0;
    // Gameplay input after the kit handled menus, undo and the cursor.
    virtual void play(const InputFrame &input, std::vector<audio::Cue> &cues) = 0;
    virtual void animate(float dt)
    {
        (void)dt;
    }

    // ---- presentation ----
    // Cells the cursor moves over (and the default ring geometry).
    virtual int grid_cols() const = 0;
    virtual int grid_rows() const = 0;
    // The board grid; defaults to fit_grid(grid_cols(), grid_rows()).
    virtual Grid grid() const;
    // Draws the board card's contents (the kit draws the card, then this, then the ring).
    virtual void draw_board(gfx::DrawList &list) const = 0;
    // Draws above the ring (for example a digit picker).
    virtual void draw_overlay(gfx::DrawList &list) const
    {
        (void)list;
    }
    // The ring around the cursor; defaults to the cursor's cell.
    virtual gfx::Rect cursor_rect(float col, float row) const;
    virtual bool show_cursor() const
    {
        return true;
    }
    // When false the kit leaves the D-pad to play() (for example while dragging).
    virtual bool kit_moves_cursor() const
    {
        return true;
    }
    // The primary hints on the left of the hint bar.
    virtual std::vector<ui::Hint> hints() const = 0;
    // An optional fourth stats box (label, value), for example "CROWNS 3/8".
    virtual bool extra_stat(std::string *label, std::string *value) const
    {
        (void)label;
        (void)value;
        return false;
    }
    // Whether Circle is used by play() (then it no longer opens the pause menu).
    virtual bool wants_back() const
    {
        return false;
    }

    // ---- helpers for games ----
    // Call before changing the state: records an undo step and counts a move.
    void remember();
    // Call when an action is rejected: shakes the board.
    void reject(std::vector<audio::Cue> &cues);
    void set_cursor(int col, int row);
    float time() const
    {
        return time_;
    }
    // 0..1 progress of the solved flourish (games can ripple their tiles).
    float solved_progress() const
    {
        return solved_timer_.running ? solved_timer_.progress() : (solved_ ? 1.0f : 0.0f);
    }
    std::uint32_t moves() const
    {
        return moves_;
    }
    const ui::Fonts &fonts() const
    {
        return fonts_;
    }
    int size_ = 0;

  private:
    enum MenuId
    {
        kResume,
        kNewPuzzle,
        kRestart,
        kSize,
        kHowTo,
        kLibrary,
        kSizeSmall = 100, // + size index
    };

    void open_pause();
    void open_solved();
    void undo(std::vector<audio::Cue> &cues);
    void redo(std::vector<audio::Cue> &cues);
    void move_cursor(Direction direction, std::vector<audio::Cue> &cues);
    void check_solved(std::vector<audio::Cue> &cues);
    void snap_ring();

    Info info_;
    ui::Fonts fonts_;
    PuzzleStats stats_;
    ui::Menu menu_;
    std::vector<std::string> undo_;
    std::vector<std::string> redo_;
    std::uint32_t moves_ = 0;
    float seconds_ = 0.0f;
    bool solved_ = false;
    bool counted_ = false; // played++ happens on the first move
    int cursor_col_ = 0;
    int cursor_row_ = 0;
    tween::Spring ring_x_;
    tween::Spring ring_y_;
    tween::Spring ring_w_;
    tween::Spring ring_h_;
    tween::Timer shake_;
    tween::Timer solved_timer_;
    tween::Timer intro_;
    float time_ = 0.0f;
};

// Seconds as m:ss.
std::string format_time(std::uint32_t seconds);
// A fresh random seed.
std::uint64_t fresh_seed();

// A small deterministic generator for puzzle construction.
class Rng
{
  public:
    explicit Rng(std::uint64_t seed) : state_(seed ? seed : 0x9e3779b97f4a7c15ULL)
    {
    }
    std::uint64_t next()
    {
        // splitmix64
        std::uint64_t z = (state_ += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }
    // Uniform in [0, bound).
    int below(int bound)
    {
        return bound <= 1 ? 0 : static_cast<int>(next() % static_cast<std::uint64_t>(bound));
    }
    template <typename T> void shuffle(std::vector<T> &items)
    {
        for (int i = static_cast<int>(items.size()) - 1; i > 0; --i)
            std::swap(items[static_cast<std::size_t>(i)],
                      items[static_cast<std::size_t>(below(i + 1))]);
    }

  private:
    std::uint64_t state_;
};

} // namespace ppz::kit
