// ProsperoPuzzles - One running Tatham puzzle: midend, drawing and input bridge.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "games/sgt/sgt_catalog.hpp"
#include "games/sgt/sgt_c.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace ppz::sgt
{

// Receives the Tatham drawing API calls. The PS5 implementation draws into a
// persistent GL canvas; tests use recording implementations. Colours are
// indices into Session::colours().
class Renderer
{
  public:
    virtual ~Renderer() = default;

    virtual void begin_draw()
    {
    }
    virtual void end_draw()
    {
    }
    virtual void rect(int x, int y, int w, int h, int colour) = 0;
    virtual void line(float x1, float y1, float x2, float y2, float thickness, int colour) = 0;
    // coords holds npoints (x, y) pairs; outline is always a valid colour,
    // fill may be -1 for an unfilled polygon.
    virtual void polygon(const int *coords, int npoints, int fill, int outline) = 0;
    virtual void circle(int cx, int cy, int radius, int fill, int outline) = 0;
    virtual void text(int x, int y, int font_type, int font_size, int align, int colour,
                      const char *text) = 0;
    virtual void clip(int x, int y, int w, int h) = 0;
    virtual void unclip() = 0;
    virtual void update(int x, int y, int w, int h)
    {
        (void)x;
        (void)y;
        (void)w;
        (void)h;
    }
    // Blitters save and restore canvas regions; ids are allocated here.
    virtual int blitter_new(int w, int h) = 0;
    virtual void blitter_free(int id) = 0;
    virtual void blitter_save(int id, int x, int y) = 0;
    virtual void blitter_load(int id, int x, int y) = 0;
};

// What a key press did, for sound and UI feedback.
enum class Action
{
    none, // nothing happened
    ui,   // cursor or other UI-only change
    move, // a move was executed (see KeyOutcome::moves)
    undo,
    redo,
    solve,
    no_effect, // the game understood the key but it changed nothing
    unused,    // the game has no use for the key
};

struct KeyOutcome
{
    Action action = Action::none;
    std::vector<std::string> moves; // executed move strings, in order
    int status_before = 0;
    int status_after = 0;
};

struct Preset
{
    std::string title; // includes the submenu path, e.g. "Tricky 9x9"
    int id = 0;
    game_params *params = nullptr; // owned by the midend's preset menu
};

struct KeyLabel
{
    std::string label;
    int button = 0;
};

class Session
{
  public:
    explicit Session(const GameEntry &entry);
    ~Session();
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;

    const GameEntry &entry() const
    {
        return entry_;
    }

    // Generates a new game for the current parameters (can be slow for hard
    // presets; the shell runs it on a worker session).
    void new_game();
    // Loads "params:desc" or "params#seed"; returns an error or "" on success.
    std::string load_game_id(const std::string &id);
    std::string game_id();
    std::string random_seed_id();

    std::vector<Preset> presets();
    int current_preset();
    void set_params(const game_params *params);
    std::string encoded_params();

    KeyOutcome key(int x, int y, int button);
    KeyOutcome undo();
    KeyOutcome redo();
    std::string solve();
    void restart();
    std::vector<KeyLabel> request_keys();

    // Drawing: the renderer may change between frames; size fits the puzzle
    // into the given area and returns the canvas size actually used.
    void set_renderer(Renderer *renderer)
    {
        renderer_ = renderer;
    }
    void resize(int max_width, int max_height, int *width, int *height);
    void redraw();
    void force_redraw();
    void tick(float seconds);
    bool timer_active() const
    {
        return timer_active_;
    }
    std::vector<float> colours(); // RGB triples, index = Tatham colour

    int status();
    bool can_undo();
    bool can_redo();
    bool can_solve() const;
    bool wants_status_bar();
    const std::string &status_text() const
    {
        return status_text_;
    }
    bool cursor_location(int *x, int *y, int *w, int *h);

    std::string serialise();
    // Replaces the running game with a serialised one; returns an error or "".
    std::string deserialise(std::string_view data);

    // Tatham front-end callbacks (see sgt_session.cpp).
    void set_timer_active(bool active)
    {
        timer_active_ = active;
    }
    void set_status_text(const char *text)
    {
        status_text_ = text != nullptr ? text : "";
    }
    Renderer *renderer() const
    {
        return renderer_;
    }
    void record_move(const char *move);
    const ::game *original_game() const
    {
        return entry_.game;
    }

    // Background colour Tatham derives each game's palette from.
    static void default_colour(float *rgb);

  private:
    class Active;

    const GameEntry &entry_;
    ::game wrapped_;
    frontend frontend_;
    midend *midend_ = nullptr;
    Renderer *renderer_ = nullptr;
    bool timer_active_ = false;
    bool recording_ = false;
    std::vector<std::string> recorded_;
    std::string status_text_;
};

} // namespace ppz::sgt
