
#include "panel.hpp"
#include "player.hpp"

#include "colors.hpp"

#include <ansi/console.h>
#include <ansi/unix_terminal.h>
#include <coreutils/log.h>

#include <fmt/format.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <functional>
#include <locale>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <sol/sol.hpp>

using namespace std::string_literals;
using namespace std::chrono_literals;

int main(int argc, const char** argv)
{
    logging::setLevel(logging::Level::Info);

    sol::state lua;

    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table,
                       sol::lib::io);

    std::string songFile;
    int startSong = -1;
    bool verbose = false;
    bool output = true;
    bool bg = false;
    bool clear = true;
    bool writeOut = false;
    bool quitPlayer = false;
    bool useColors = false;
    std::string command;

    std::vector<fs::path> songFiles;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            auto opt = std::string_view(&argv[i][1]);
            if (opt == "song" || opt == "s") {
                startSong = std::stoi(argv[++i]);
            } else if (opt == "color" || opt == "c") {
                useColors = true;
            } else if (opt == "v") {
                verbose = true;
                output = false;
            } else if (opt == "d") {
                bg = true;
            } else if (opt == "a") {
                clear = false;
            } else if (opt == "o") {
                writeOut = true;
            } else if (opt == "q") {
                quitPlayer = true;
            } else if (opt == "n") {
                command = "next";
                bg = true;
            } else if (opt == "p") {
                command = "prev";
                bg = true;
            }

        } else {
            songFile = argv[i];
            songFiles.emplace_back(songFile);
        }
    }

    if (isatty(fileno(stdin)) == 0) {
        bg = true;
        std::string line;
        while (std::getline(std::cin, line)) {
            songFiles.emplace_back(line);
        }
        fmt::print("{} files\n", songFiles.size());
    }

    auto music_player =
        writeOut ? MusicPlayer::createWriter() : MusicPlayer::create();

    if (quitPlayer) {
        music_player->clear();
        music_player = nullptr;
        return 0;
    }

    if (startSong > 0) { music_player->set_song(startSong - 1); }
    if (!songFiles.empty()) {
        if (clear) { music_player->clear(); }
        for (auto&& sf : songFiles) {
            music_player->add(sf);
        }
    }

    if (writeOut) { return 0; }

    if (command == "next") { music_player->next(); }
    if (command == "prev") { music_player->prev(); }

    if (bg) {
        music_player->detach();
        return 0;
    }

    std::unique_ptr<bbs::Terminal> term =
        std::make_unique<bbs::LocalTerminal>();
    term->open();
    auto con = std::make_shared<bbs::Console>(std::move(term));
    con->set_xy(0, 0);

    auto panel = Panel{con};
    panel.useColors = useColors;
    std::unordered_map<std::string, Meta> meta;

    bool theme_set = false;
    sol::function update_fn;
    std::unordered_map<int, std::function<void()>> mapping;

    for (auto&& [name, color] : html_colors) {
        lua[utils::toUpper(name)] = (color << 8) | 0xff;
    }
    lua["DEFAULT"] = 0x12345600;

    for (int i = KEY_F1; i <= KEY_F8; i++) {
        lua[fmt::format("KEY_F{}", i - KEY_F1 + 1)] = i;
    }

    lua["get_meta"] = [&] {
        sol::table t = lua.create_table();
        for (auto [name, val] : meta) {
            std::visit([&, n = name](auto&& v) { t[n] = v; }, val);
        }
        return t;
    };
    lua.set_function("var_color",
                     sol::overload(
                         [&](std::string const& var, uint32_t color) {
                             if (auto* target = panel.get_var(var)) {
                                 target->fg = color;
                             }
                         },
                         [&](std::string const& var, uint32_t fg, uint32_t bg) {
                             if (auto* target = panel.get_var(var)) {
                                 target->fg = fg;
                                 target->bg = bg;
                             }
                         }));
    lua.set_function("map",
                     sol::overload(
                         [&](std::string key, std::function<void()> const& fn) {
                             mapping[static_cast<int>(key[0])] = fn;
                         },
                         [&](int key, std::function<void()> const& fn) {
                             mapping[key] = fn;
                         }));

    lua.set_function(
        "colorize",
        sol::overload(
            [&](std::string const& pattern, uint32_t color) {
                auto [x, y] = con->find(pattern);
                if (x >= 0) {
                    con->set_color(color);
                    con->colorize(x, y, pattern.size(), 1);
                }
            },
            [&](std::string const& pattern, uint32_t fg, uint32_t bg) {
                auto [x, y] = con->find(pattern);
                if (x >= 0) {
                    con->set_color(fg, bg);
                    con->colorize(x, y, pattern.size(), 1);
                }
            },
            [&](int x, int y, int len, uint32_t color) {
                con->set_color(color);
                con->colorize(x, y, len, 1);
            },
            [&](int x, int y, int len, uint32_t fg, uint32_t bg) {
                con->set_color(fg, bg);
                con->colorize(x, y, len, 1);
            }));

    lua["set_theme"] = [&](sol::table args) {
        theme_set = true;
        std::string panelText = args["panel"];
        auto panel_bg =
            args.get_or<uint32_t>("panel_bg", bbs::Console::DefaultColor);
        auto panel_fg =
            args.get_or<uint32_t>("panel_fg", bbs::Console::DefaultColor);
        auto var_bg =
            args.get_or<uint32_t>("var_bg", bbs::Console::DefaultColor);
        auto var_fg =
            args.get_or<uint32_t>("var_fg", bbs::Console::DefaultColor);
        con->set_color(panel_fg, panel_bg);
        panel.set_color(var_fg, var_bg);
        panel.set_panel(panelText);
        sol::function v = args["init_fn"];
        if (v.valid()) { v(); }
        con->set_color(panel_fg, panel_bg);
        update_fn = args["update_fn"];
    };

    auto dataPath = MusicPlayer::findDataPath("init.lua");

    if (!dataPath.empty() && fs::exists(dataPath)) {
        auto res = lua.script_file(dataPath.string());
        if (!res.valid()) { fmt::print("ERROR\n"); }
    }
    if (!theme_set) { panel.set_panel(); }
    con->flush();

    uint32_t song = 0;

    auto clear_meta = [&] {
        meta.clear();
        meta["title"] = ""s;
        meta["sub_title"] = ""s;
        meta["game"] = ""s;
        meta["composer"] = ""s;
        meta["filename"] = ""s;
        meta["song"] = static_cast<uint32_t>(0);
        meta["songs"] = static_cast<uint32_t>(0);
        meta["length"] = static_cast<uint32_t>(0);
        meta["seconds"] = static_cast<uint32_t>(0);
        song = 0;
    };
    clear_meta();

    static std::atomic<bool> quit{false};

    std::signal(SIGINT, [](int) { quit = true; });

    while (!quit) {
        auto&& info = music_player->get_info();
        for (auto&& [name, val] : info) {
            if (name == "init") {
                clear_meta();
                continue;
            }
            if (name == "song") { song = std::get<uint32_t>(val); }
            meta[name] = val;
        }
        if (output) {
            if (!info.empty()) {

                panel.update(meta);
                if (update_fn.valid()) {
                    sol::table t = lua.create_table();
                    for (auto [name, val] : meta) {
                        std::visit([&, n = name](auto&& v) { t[n] = v; }, val);
                    }
                    update_fn(t);
                    for (auto&& [key, val] : t) {
                        if (val.get_type() == sol::type::number) {
                            meta[key.as<std::string>()] = val.as<uint32_t>();
                        } else {
                            meta[key.as<std::string>()] = val.as<std::string>();
                        }
                    }
                }

                panel.render(meta);
                con->flush();
            }
        }

        auto key = con->read_key();
        if (key == KEY_NONE) { std::this_thread::sleep_for(100ms); }
        auto it = mapping.find(key);
        if (it != mapping.end()) { it->second(); }
        if (key == KEY_ESCAPE) {
            music_player->detach();
            quit = true;
        }
        if (key == 'q') { quit = true; }
        if (key == KEY_ENTER || key == 'n') { music_player->next(); }
        if (key == KEY_BACKSPACE || key == 'p') { music_player->prev(); }
        if (key == KEY_RIGHT || key == ']') {
            music_player->set_song(song + 1);
        }
        if (key == KEY_LEFT || key == '[') { music_player->set_song(song - 1); }
    }
    music_player = nullptr;
    return 0;
}
