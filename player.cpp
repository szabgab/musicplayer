#include "player.hpp"

#include "chipplayer.h"
#include "chipplugin.h"

#include <audioplayer/audioplayer.h>
#include <chrono>
#include <coreutils/log.h>
#include <coreutils/utils.h>

#include <csignal>
#include <readerwriterqueue.h>

#include "resampler.h"

#include <atomic>
#include <thread>

class Player : public MusicPlayer
{

    std::shared_ptr<musix::ChipPlayer> player;
    std::string pluginName;
    std::vector<Info> infoList;

    Resampler<32768> fifo{44100};
    AudioPlayer audioPlayer{44100};
    int _current_song{};

public:
    std::shared_ptr<musix::ChipPlayer> get_player() { return player; }

    Player()
    {
        using musix::ChipPlayer;
        using musix::ChipPlugin;

        logging::setLevel(logging::Level::Warning);

        auto xd = utils::get_exe_dir();
        auto searchPath = std::vector{fs::absolute(xd / ".." / "data"),
                                      fs::absolute(xd / ".." / ".." / "data"),
                                      fs::path("/usr/share/musix"),
                                      fs::path("/usr/local/share/musix")};
        fs::path dataPath;
        for (auto&& p : searchPath) {
            if (fs::exists(p)) {
                dataPath = p;
                break;
            }
        }
        if (dataPath.empty()) {
            throw musix::player_exception("Could not find data directory");
        }
        ChipPlugin::createPlugins(dataPath.string());

        audioPlayer.play([&](int16_t* ptr, int size) {
            auto count = fifo.read(ptr, size);
            if (count <= 0) { memset(ptr, 0, size * 2); }
        });
    }

    void play(fs::path const& name) override
    {
        player = nullptr;
        for (const auto& plugin : musix::ChipPlugin::getPlugins()) {
            if (plugin->canHandle(name)) {
                if (auto* ptr = plugin->fromFile(name)) {
                    player = std::shared_ptr<musix::ChipPlayer>(ptr);
                    pluginName = plugin->name();
                    break;
                }
            }
        }
        if (!player) { printf("No plugin could handle file\n"); }

        player->onMeta([this](auto&& meta_list, auto* player) {
            for (auto&& name : meta_list) {
                auto&& val = player->meta(name);
                infoList.emplace_back(name, val);
            }
        });

        _current_song = player->getMetaInt("startSong");
    }

    std::vector<Info> get_info() override
    {
        auto result = infoList;
        infoList.clear();
        return result;
    }

    void next() override { player->seekTo(++_current_song); }

    void prev() override { player->seekTo(--_current_song); }

    void update() override
    {
        if (player == nullptr) { return; }
        std::array<int16_t, 1024 * 16> temp{};
        fifo.setHz(player->getHZ());
        auto rc =
            player->getSamples(temp.data(), static_cast<int>(temp.size()));
        if (rc > 0) { fifo.write(&temp[0], &temp[1], rc); }
    }
};

class ThreadedPlayer : public MusicPlayer
{
    Player player;
    std::thread playThread;
    std::atomic<bool> quit{false};

    struct Next
    {};
    struct Prev
    {};
    struct Play
    {
        fs::path name;
    };

    using Command = std::variant<Next, Prev, Play>;

    moodycamel::ReaderWriterQueue<Command> commands;
    moodycamel::ReaderWriterQueue<std::vector<Info>> infos;

    void handle_cmd(Next const&) { player.next(); }
    void handle_cmd(Prev const&) { player.prev(); }
    void handle_cmd(Play const& cmd) { player.play(cmd.name); }

    void update() override
    {
        Command cmd;
        if (commands.try_dequeue(cmd)) {
            std::visit([&](auto&& cmd) { handle_cmd(cmd); }, cmd);
        }
        auto&& info = player.get_info();
        if (!info.empty()) { infos.emplace(info); }
    }

    void run()
    {
        playThread = std::thread([this]() {
            while (!quit) {
                player.update();
                update();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

public:
    void play(fs::path const& name) override { commands.emplace(Play{name}); }
    void next() override { commands.emplace(Next{}); }
    void prev() override { commands.emplace(Prev{}); }

    std::vector<Info> get_info() override
    {
        std::vector<Info> info;
        if (infos.try_dequeue(info)) { return info; }
        return {};
    }

    ThreadedPlayer() { run(); }
    ~ThreadedPlayer() override
    {
        quit = true;
        if (playThread.joinable()) { playThread.join(); }
    }
};

class PipePlayer : public MusicPlayer
{
    int childPid = -1;
public:
    PipePlayer()
    {
        pid_t pid = fork();
        if (pid < 0) { exit(EXIT_FAILURE); }
        if (pid > 0) {
            childPid = pid;
            return;
            // exit(EXIT_SUCCESS);
        }
        // Child starting
        umask(0);
        auto sid = setsid();
        if (sid < 0) { exit(EXIT_FAILURE); }

        auto fifo_path = utils::get_home_dir() / ".musix_fifo";
        mkfifo(fifo_path.c_str(), 0777);
        //close(STDIN_FILENO);
        //close(STDOUT_FILENO);
        //close(STDERR_FILENO);
        ThreadedPlayer player;
        std::ifstream myfile;
        myfile.open(fifo_path.string());
        std::string l;
        while (true) {
            if(std::getline(myfile, l)) {
                if (l[0] == '>') {
                    player.play(l.substr(1));
                } else if(l[0] == 'n') {
                    player.next();
                } else if(l[0] == 'p') {
                    player.prev();
                }
            }
        }
    }

    ~PipePlayer() override
    {
        if (childPid > 0) {
            kill(childPid, SIGINT);
        }
    }

    void play(fs::path const& name) override
    {
        auto fifo_path = utils::get_home_dir() / ".musix_fifo";
        std::ofstream myfile;
        myfile.open(fifo_path.string());
        myfile << '>' << name.string() << "\n";
        myfile.close();
    }

    void next() override
    {
        auto fifo_path = utils::get_home_dir() / ".musix_fifo";
        std::ofstream myfile;
        myfile.open(fifo_path.string());
        myfile << "n\n";
        myfile.close();
    }

    std::vector<Info> get_info() override
    {
        return {};
    }
};

std::unique_ptr<MusicPlayer> MusicPlayer::create()
{
    return std::make_unique<PipePlayer>();
}
