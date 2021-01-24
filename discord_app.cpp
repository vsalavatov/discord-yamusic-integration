#define _CRT_SECURE_NO_WARNINGS

#include <array>
#include <cassert>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>

#include "discord.h"

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/thread.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

using namespace boost::asio;
using ip::tcp;

class DiscordHandler {
public:
    DiscordHandler() = default;
    ~DiscordHandler() = default;

    discord::Core* getInstance() {
        if (core == nullptr) {
            discord::Core* instance{};
            auto result = discord::Core::Create(799915793156145152, DiscordCreateFlags_Default, &instance);
            core.reset(instance);
            if (!core) {
                std::cout << "Failed to instantiate discord core! (err " << static_cast<int>(result)
                          << ")\n";
                std::exit(-1);
            }
        }
        return core.get();
    }

    void free() {
        core.reset(nullptr);
    }

    bool isActive() const {
        return core != nullptr;
    }

private:
    std::unique_ptr<discord::Core> core;
};

struct YaMusicState {
    std::string trackCoverURL;
    std::string trackName;
    std::string trackURL;
    std::string artistName;
    std::string artistURL;
    double songStateNow;
    double songStateTotal;

    bool haveTimeInfo() const {
        return songStateNow >= 0 && songStateTotal > 0;
    }

    static YaMusicState fromJSON(const std::string& raw_json) {
        std::stringstream json_stream;
        json_stream << raw_json;
        boost::property_tree::ptree pt;
        boost::property_tree::read_json(json_stream, pt);

        YaMusicState result;
        result.trackCoverURL = pt.get<std::string>("track_cover_url");
        result.trackName = pt.get<std::string>("track_name");
        result.trackURL = pt.get<std::string>("track_url");
        result.artistName = pt.get<std::string>("artist_name");
        result.artistURL = pt.get<std::string>("artist_url");
        try {
            result.songStateNow = parseTextDurationToSeconds(pt.get<std::string>("song_state_now"));
            result.songStateTotal = parseTextDurationToSeconds(pt.get<std::string>("song_state_total"));
        } catch (...) {
            result.songStateNow = -1;
            result.songStateTotal = -1;
        }
        return result;
    }

    bool operator==(const YaMusicState& other) const {
        return trackCoverURL == other.trackCoverURL &&
                trackName == other.trackName &&
                trackURL == other.trackURL &&
                artistName == other.artistName &&
                artistURL == other.artistURL &&
                fabs(songStateNow - other.songStateNow) < 1e-6 &&
                fabs(songStateTotal - other.songStateTotal) < 1e-6;
    }

private:
    static double parseTextDurationToSeconds(const std::string& data) {
        int h, m, s;
        int parts = sscanf(data.c_str(), "%d:%d:%d", &h, &m, &s);
        if (parts == 2)
            return h * 60 + m;
        if (parts == 3)
            return h * 60 * 60 + m * 60 + s;
        if (parts == 1)
            return h;
        return -1;
    }
};

class DiscordActivityController {
public:
    explicit DiscordActivityController(DiscordHandler &discordState)
    : discordHandler{discordState},
      previousYaMusicState{},
      lastUpdateTimestamp{std::chrono::time_point<std::chrono::system_clock>()},
      activity{}
    {
    };
    ~DiscordActivityController() = default;

    void makeListening(const YaMusicState &yaMusicState) {
        std::lock_guard<std::mutex> lg(lock);
        lastUpdateTimestamp = std::chrono::system_clock::now();
        isStalledChecker.addData({yaMusicState.songStateNow, yaMusicState.songStateTotal});

        auto setTimestamps = [&]() {
            if (yaMusicState.haveTimeInfo()) {
                auto startTimestamp = std::chrono::system_clock::now()
                                 - std::chrono::seconds(static_cast<long>(yaMusicState.songStateNow));
                auto endTimestamp = startTimestamp + std::chrono::seconds(static_cast<long>(yaMusicState.songStateTotal));
                activity.GetTimestamps().SetStart(secondsSinceEpoch(startTimestamp));
                // activity.GetTimestamps().SetEnd(secondsSinceEpoch(endTimestamp));
            }
        };

        bool doUpdate = true;
        if (yaMusicState.artistName == previousYaMusicState.artistName &&
            yaMusicState.trackName == previousYaMusicState.trackName &&
            yaMusicState.trackURL == previousYaMusicState.trackURL) {
            if (yaMusicState.haveTimeInfo() && isStalledChecker.isStalled()) {
                if (activity.GetTimestamps().GetStart() != 0) {
                    activity.GetTimestamps().SetStart(0);
                    activity.GetTimestamps().SetEnd(0);
                } else {
                    doUpdate = false;
                }
            } else if (yaMusicState.haveTimeInfo() && activity.GetTimestamps().GetStart() == 0){
                setTimestamps();
            } else {
                auto startWas = activity.GetTimestamps().GetStart();
                setTimestamps();
                if (abs(startWas - activity.GetTimestamps().GetStart()) <= 1)
                    doUpdate = false;
            }
        } else {
            std::string upperLine = yaMusicState.artistName;
            std::string midLine = yaMusicState.trackName;

            activity.SetType(discord::ActivityType::Listening);
            activity.SetDetails(upperLine.c_str());
            activity.SetState(midLine.c_str());
            setTimestamps();
        }
        if (doUpdate) {
            try {
                discordHandler.getInstance()->ActivityManager().UpdateActivity(activity, [this](discord::Result result) {
                    std::cout << ((result == discord::Result::Ok) ? "Succeeded" : "Failed")
                              << " updating activity!\n";
                });
            } catch (...) {
                std::cerr << "Error: Activity update failed!" << std::endl;
            }
        }
        previousYaMusicState = yaMusicState;
    }

    void clearIfStalled() {
        std::lock_guard<std::mutex> lg(lock);
        if (lastUpdateTimestamp > std::chrono::system_clock::now() - std::chrono::seconds(10))
            return;
        discordHandler.free();
    }

private:
    template<class T>
    static constexpr long secondsSinceEpoch(const T& timepoint) {
        return std::chrono::duration_cast<std::chrono::seconds>(timepoint.time_since_epoch()).count();
    }

    class IsStalledChecker {
    public:
        IsStalledChecker() = default;
        ~IsStalledChecker() = default;

        bool isStalled() const {
            if (timestamps.size() < windowSize)
                return false;
            return std::all_of(timestamps.begin(), timestamps.end(), [e=timestamps[0]](const auto& t) {
                return e == t;
            });
        }

        void addData(std::pair<long, long> timestamp) {
            timestamps.push_back(timestamp);
            if (timestamps.size() > windowSize) {
                timestamps.erase(timestamps.begin());
            }
        }

    private:
        std::vector<std::pair<long, long>> timestamps;
        const int windowSize = 4;
    };

    DiscordHandler &discordHandler;
    YaMusicState previousYaMusicState;
    IsStalledChecker isStalledChecker;
    std::chrono::time_point<std::chrono::system_clock> lastUpdateTimestamp;
    discord::Activity activity;

    std::mutex lock;
};

namespace {
    volatile bool interrupted{false};
}

int main(int, char**)
{
    std::signal(SIGINT, [](int) { interrupted = true; });

    DiscordHandler discordHandler{};
    DiscordActivityController discordActivityController(discordHandler);

    io_service io_svc;
    tcp::acceptor acceptor(io_svc);
    acceptor.open(tcp::v4());
    acceptor.set_option(tcp::acceptor::reuse_address(true));
    acceptor.bind({{}, 13239});
    acceptor.listen(5);

    spawn(io_svc, [&acceptor, &discordActivityController](yield_context yield){
#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
       while (true) {
           tcp::socket sock(acceptor.get_io_service());
           std::cout << "Listening..." << std::endl;
           acceptor.async_accept(sock, yield);
           std::cout << "Accepted!" << std::endl;
           spawn(yield, [sock=std::move(sock), &discordActivityController](yield_context yield) mutable {
               char buf[8096];
               size_t bytes = sock.async_read_some(buffer(buf), yield);
               std::string req(buf, buf + bytes);
               size_t headers_end = req.find("\r\n\r\n") + 4;
               std::string raw_json = req.substr(headers_end);
               std::cout << raw_json << std::endl;

               auto state = YaMusicState::fromJSON(raw_json);
               discordActivityController.makeListening(state);
           });
       }
#pragma clang diagnostic pop
    });

    boost::thread io_service_thread([&io_svc] { io_svc.run(); });

    do {
        discordActivityController.clearIfStalled();

        if (discordHandler.isActive())
            discordHandler.getInstance()->RunCallbacks();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (!interrupted);

    io_service_thread.interrupt();
    io_service_thread.join();

    return 0;
}
