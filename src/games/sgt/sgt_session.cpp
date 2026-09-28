// ProsperoPuzzles - One running Tatham puzzle: midend, drawing and input bridge.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Implements the Tatham front-end contract (drawing_api, timers, random seed,
// fatal) and observes executed moves by giving the midend a copy of the game
// vtable whose execute_move records the move string before forwarding. The
// vendored upstream code stays unmodified.

#include "games/sgt/sgt_session.hpp"

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

struct blitter
{
    int id;
};

namespace ppz::sgt
{

namespace
{

// The session whose midend is being driven on this thread. execute_move has
// no context argument, so the trampoline finds its session here.
thread_local Session *t_active = nullptr;

Session *session_of(drawing *dr)
{
    return GET_HANDLE_AS_TYPE(dr, Session);
}

Renderer *renderer_of(drawing *dr)
{
    return session_of(dr)->renderer();
}

void api_draw_text(drawing *dr, int x, int y, int font_type, int font_size, int align, int colour,
                   const char *text)
{
    if (Renderer *r = renderer_of(dr))
        r->text(x, y, font_type, font_size, align, colour, text);
}

void api_draw_rect(drawing *dr, int x, int y, int w, int h, int colour)
{
    if (Renderer *r = renderer_of(dr))
        r->rect(x, y, w, h, colour);
}

void api_draw_line(drawing *dr, int x1, int y1, int x2, int y2, int colour)
{
    if (Renderer *r = renderer_of(dr))
        r->line(static_cast<float>(x1), static_cast<float>(y1), static_cast<float>(x2),
                static_cast<float>(y2), 1.0f, colour);
}

void api_draw_polygon(drawing *dr, const int *coords, int npoints, int fill, int outline)
{
    if (Renderer *r = renderer_of(dr))
        r->polygon(coords, npoints, fill, outline);
}

void api_draw_circle(drawing *dr, int cx, int cy, int radius, int fill, int outline)
{
    if (Renderer *r = renderer_of(dr))
        r->circle(cx, cy, radius, fill, outline);
}

void api_draw_update(drawing *dr, int x, int y, int w, int h)
{
    if (Renderer *r = renderer_of(dr))
        r->update(x, y, w, h);
}

void api_clip(drawing *dr, int x, int y, int w, int h)
{
    if (Renderer *r = renderer_of(dr))
        r->clip(x, y, w, h);
}

void api_unclip(drawing *dr)
{
    if (Renderer *r = renderer_of(dr))
        r->unclip();
}

void api_start_draw(drawing *dr)
{
    if (Renderer *r = renderer_of(dr))
        r->begin_draw();
}

void api_end_draw(drawing *dr)
{
    if (Renderer *r = renderer_of(dr))
        r->end_draw();
}

void api_status_bar(drawing *dr, const char *text)
{
    session_of(dr)->set_status_text(text);
}

blitter *api_blitter_new(drawing *dr, int w, int h)
{
    auto *result = snew(blitter);
    Renderer *r = renderer_of(dr);
    result->id = r != nullptr ? r->blitter_new(w, h) : -1;
    return result;
}

void api_blitter_free(drawing *dr, blitter *bl)
{
    if (Renderer *r = renderer_of(dr); r != nullptr && bl->id >= 0)
        r->blitter_free(bl->id);
    sfree(bl);
}

void api_blitter_save(drawing *dr, blitter *bl, int x, int y)
{
    if (Renderer *r = renderer_of(dr); r != nullptr && bl->id >= 0)
        r->blitter_save(bl->id, x, y);
}

void api_blitter_load(drawing *dr, blitter *bl, int x, int y)
{
    if (Renderer *r = renderer_of(dr); r != nullptr && bl->id >= 0)
        r->blitter_load(bl->id, x, y);
}

// Our fonts cover ASCII: prefer the first plain-ASCII alternative.
char *api_text_fallback(drawing *dr, const char *const *strings, int nstrings)
{
    (void)dr;
    for (int i = 0; i < nstrings; ++i)
    {
        bool ascii = true;
        for (const char *p = strings[i]; *p != '\0'; ++p)
        {
            if (static_cast<unsigned char>(*p) >= 0x80)
                ascii = false;
        }
        if (ascii)
            return dupstr(strings[i]);
    }
    return dupstr(strings[nstrings - 1]);
}

void api_draw_thick_line(drawing *dr, float thickness, float x1, float y1, float x2, float y2,
                         int colour)
{
    if (Renderer *r = renderer_of(dr))
        r->line(x1, y1, x2, y2, thickness, colour);
}

const drawing_api kDrawingApi = {
    1, // API version
    api_draw_text,
    api_draw_rect,
    api_draw_line,
    api_draw_polygon,
    api_draw_circle,
    api_draw_update,
    api_clip,
    api_unclip,
    api_start_draw,
    api_end_draw,
    api_status_bar,
    api_blitter_new,
    api_blitter_free,
    api_blitter_save,
    api_blitter_load,
    nullptr, // begin_doc (printing is not supported)
    nullptr, // begin_page
    nullptr, // begin_puzzle
    nullptr, // end_puzzle
    nullptr, // end_page
    nullptr, // end_doc
    nullptr, // line_width
    nullptr, // line_dotted
    api_text_fallback,
    api_draw_thick_line,
};

game_state *execute_move_trampoline(const game_state *state, const char *move)
{
    Session *session = t_active;
    if (session == nullptr)
    {
        fatal("execute_move called outside an active session");
        return nullptr;
    }
    game_state *next = session->original_game()->execute_move(state, move);
    if (next != nullptr)
        session->record_move(move);
    return next;
}

void flatten_presets(const preset_menu *menu, const std::string &prefix, std::vector<Preset> &out)
{
    for (int i = 0; i < menu->n_entries; ++i)
    {
        const preset_menu_entry &entry = menu->entries[i];
        std::string title = prefix.empty() ? entry.title : prefix + " " + entry.title;
        if (entry.submenu != nullptr)
            flatten_presets(entry.submenu, title, out);
        else
            out.push_back(Preset{title, entry.id, entry.params});
    }
}

} // namespace

// Marks this session active on the calling thread for the duration of a
// midend call, restoring any outer session afterwards.
class Session::Active
{
  public:
    explicit Active(Session &session) : previous_(t_active)
    {
        t_active = &session;
    }
    ~Active()
    {
        t_active = previous_;
    }
    Active(const Active &) = delete;
    Active &operator=(const Active &) = delete;

  private:
    Session *previous_;
};

Session::Session(const GameEntry &entry) : entry_(entry), wrapped_(*entry.game), frontend_{this}
{
    wrapped_.execute_move = execute_move_trampoline;
    Active active(*this);
    midend_ = midend_new(&frontend_, &wrapped_, &kDrawingApi, this);
}

Session::~Session()
{
    Active active(*this);
    // midend_free does not release the pre-move state held by an animation in
    // progress; finish it first (without drawing).
    renderer_ = nullptr;
    midend_stop_anim(midend_);
    midend_free(midend_);
}

void Session::record_move(const char *move)
{
    if (recording_)
        recorded_.emplace_back(move);
}

void Session::default_colour(float *rgb)
{
    // Warm paper; Tatham derives each game's highlight and lowlight from it.
    rgb[0] = 0.92f;
    rgb[1] = 0.91f;
    rgb[2] = 0.88f;
}

void Session::new_game()
{
    Active active(*this);
    midend_new_game(midend_);
}

std::string Session::load_game_id(const std::string &id)
{
    Active active(*this);
    if (const char *error = midend_game_id(midend_, id.c_str()))
        return error;
    midend_new_game(midend_);
    return {};
}

std::string Session::game_id()
{
    Active active(*this);
    char *id = midend_get_game_id(midend_);
    std::string result = id != nullptr ? id : "";
    sfree(id);
    return result;
}

std::string Session::random_seed_id()
{
    Active active(*this);
    char *id = midend_get_random_seed(midend_);
    std::string result = id != nullptr ? id : "";
    sfree(id);
    return result;
}

std::vector<Preset> Session::presets()
{
    Active active(*this);
    int id_limit = 0;
    std::vector<Preset> result;
    if (const preset_menu *menu = midend_get_presets(midend_, &id_limit))
        flatten_presets(menu, {}, result);
    return result;
}

int Session::current_preset()
{
    Active active(*this);
    return midend_which_preset(midend_);
}

void Session::set_params(const game_params *params)
{
    Active active(*this);
    midend_set_params(midend_, const_cast<game_params *>(params));
}

std::string Session::encoded_params()
{
    Active active(*this);
    game_params *params = midend_get_params(midend_);
    char *encoded = entry_.game->encode_params(params, true);
    std::string result = encoded;
    sfree(encoded);
    entry_.game->free_params(params);
    return result;
}

KeyOutcome Session::key(int x, int y, int button)
{
    Active active(*this);
    KeyOutcome outcome;
    outcome.status_before = midend_status(midend_);
    recorded_.clear();
    recording_ = true;
    const int result = midend_process_key(midend_, x, y, button);
    recording_ = false;
    outcome.status_after = midend_status(midend_);
    outcome.moves = std::move(recorded_);
    recorded_.clear();

    if (button == UI_UNDO)
        outcome.action = result == PKR_SOME_EFFECT ? Action::undo : Action::no_effect;
    else if (button == UI_REDO)
        outcome.action = result == PKR_SOME_EFFECT ? Action::redo : Action::no_effect;
    else if (button == UI_SOLVE)
        outcome.action = result == PKR_SOME_EFFECT ? Action::solve : Action::no_effect;
    else if (!outcome.moves.empty())
        outcome.action = Action::move;
    else if (result == PKR_SOME_EFFECT)
        outcome.action = Action::ui;
    else if (result == PKR_NO_EFFECT)
        outcome.action = Action::no_effect;
    else if (result == PKR_UNUSED)
        outcome.action = Action::unused;
    return outcome;
}

KeyOutcome Session::undo()
{
    return key(0, 0, UI_UNDO);
}

KeyOutcome Session::redo()
{
    return key(0, 0, UI_REDO);
}

std::string Session::solve()
{
    Active active(*this);
    const char *error = midend_solve(midend_);
    return error != nullptr ? error : "";
}

void Session::restart()
{
    Active active(*this);
    midend_restart_game(midend_);
}

std::vector<KeyLabel> Session::request_keys()
{
    Active active(*this);
    int count = 0;
    key_label *keys = midend_request_keys(midend_, &count);
    std::vector<KeyLabel> result;
    for (int i = 0; keys != nullptr && i < count; ++i)
        result.push_back(KeyLabel{keys[i].label != nullptr ? keys[i].label : "", keys[i].button});
    if (keys != nullptr)
        free_keys(keys, count);
    return result;
}

void Session::resize(int max_width, int max_height, int *width, int *height)
{
    Active active(*this);
    int x = max_width;
    int y = max_height;
    midend_size(midend_, &x, &y, true, 1.0);
    *width = x;
    *height = y;
}

void Session::redraw()
{
    Active active(*this);
    midend_redraw(midend_);
}

void Session::force_redraw()
{
    Active active(*this);
    midend_force_redraw(midend_);
}

void Session::tick(float seconds)
{
    if (!timer_active_)
        return;
    Active active(*this);
    midend_timer(midend_, seconds);
}

std::vector<float> Session::colours()
{
    Active active(*this);
    int count = 0;
    float *colours = midend_colours(midend_, &count);
    std::vector<float> result(colours, colours + 3 * count);
    sfree(colours);
    return result;
}

int Session::status()
{
    Active active(*this);
    return midend_status(midend_);
}

bool Session::can_undo()
{
    Active active(*this);
    return midend_can_undo(midend_);
}

bool Session::can_redo()
{
    Active active(*this);
    return midend_can_redo(midend_);
}

bool Session::can_solve() const
{
    return entry_.game->can_solve;
}

bool Session::wants_status_bar()
{
    Active active(*this);
    return midend_wants_statusbar(midend_);
}

bool Session::cursor_location(int *x, int *y, int *w, int *h)
{
    Active active(*this);
    return midend_get_cursor_location(midend_, x, y, w, h);
}

std::string Session::serialise()
{
    Active active(*this);
    std::string out;
    midend_serialise(
        midend_,
        [](void *context, const void *buffer, int length)
        {
            static_cast<std::string *>(context)->append(static_cast<const char *>(buffer),
                                                        static_cast<std::size_t>(length));
        },
        &out);
    return out;
}

std::string Session::deserialise(std::string_view data)
{
    struct Reader
    {
        std::string_view data;
        std::size_t position;
    } reader{data, 0};
    Active active(*this);
    const char *error = midend_deserialise(
        midend_,
        [](void *context, void *buffer, int length)
        {
            auto *r = static_cast<Reader *>(context);
            const auto size = static_cast<std::size_t>(length);
            if (length < 0 || r->data.size() - r->position < size)
                return false;
            std::memcpy(buffer, r->data.data() + r->position, size);
            r->position += size;
            return true;
        },
        &reader);
    return error != nullptr ? error : "";
}

} // namespace ppz::sgt

// ---- Tatham front-end functions (C linkage, declared in puzzles.h) ----

void frontend_default_colour(frontend *fe, float *output)
{
    (void)fe;
    ppz::sgt::Session::default_colour(output);
}

void activate_timer(frontend *fe)
{
    if (fe != nullptr && fe->session != nullptr)
        fe->session->set_timer_active(true);
}

void deactivate_timer(frontend *fe)
{
    if (fe != nullptr && fe->session != nullptr)
        fe->session->set_timer_active(false);
}

void get_random_seed(void **randseed, int *randseedsize)
{
    static std::atomic<std::uint64_t> counter{0x9e3779b97f4a7c15ULL};
    std::uint64_t value =
        static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()) ^
        counter.fetch_add(0x9e3779b97f4a7c15ULL);
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    value ^= value >> 31;
    auto *seed = snewn(sizeof(value), unsigned char);
    std::memcpy(seed, &value, sizeof(value));
    *randseed = seed;
    *randseedsize = static_cast<int>(sizeof(value));
}

void fatal(const char *fmt, ...)
{
    char message[256];
    va_list arguments;
    va_start(arguments, fmt);
    std::vsnprintf(message, sizeof(message), fmt, arguments);
    va_end(arguments);
    std::fprintf(stderr, "[PPZ] sgt fatal: %s\n", message);
    std::fflush(stderr);
    std::abort();
}
