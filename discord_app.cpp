#define _CRT_SECURE_NO_WARNINGS

#include <array>
#include <cassert>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

#include "discord.h"

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/thread.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

using namespace boost::asio;
using ip::tcp;

struct DiscordState {
    discord::User currentUser;

    std::unique_ptr<discord::Core> core;
};

namespace {
    volatile bool interrupted{false};
}

int main(int, char**)
{
    std::signal(SIGINT, [](int) { interrupted = true; });

    DiscordState discordState{};

    discord::Core* core{};
    auto result = discord::Core::Create(799915793156145152, DiscordCreateFlags_Default, &core);
    discordState.core.reset(core);
    if (!discordState.core) {
        std::cout << "Failed to instantiate discord core! (err " << static_cast<int>(result)
                  << ")\n";
        std::exit(-1);
    }

    discordState.core->SetLogHook(
            discord::LogLevel::Debug, [](discord::LogLevel level, const char* message) {
                std::cerr << "Log(" << static_cast<uint32_t>(level) << "): " << message << "\n";
            });

    io_service io_svc;
    tcp::acceptor acceptor(io_svc);
    acceptor.open(tcp::v4());
    acceptor.set_option(tcp::acceptor::reuse_address(true));
    acceptor.bind({{}, 13239});
    acceptor.listen(5);

    spawn(io_svc, [&acceptor, &discordState](yield_context yield){
#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
       while (true) {
           tcp::socket sock(acceptor.get_io_service());
           std::cout << "Listening..." << std::endl;
           acceptor.async_accept(sock, yield);
           std::cout << "Accepted!" << std::endl;
           spawn(yield, [sock=std::move(sock), &discordState](yield_context yield) mutable {
               char buf[8096];
               size_t bytes = sock.async_read_some(buffer(buf), yield);
               std::string req(buf, buf + bytes);
               size_t headers_end = req.find("\r\n\r\n") + 4;
               std::string raw_json = req.substr(headers_end);
               std::cout << raw_json << std::endl;

               std::stringstream json_stream;
               json_stream << raw_json;
               boost::property_tree::ptree pt;
               boost::property_tree::read_json(json_stream, pt);
               auto trackCoverURL = pt.get<std::string>("track_cover_url");
               auto trackName = pt.get<std::string>("track_name");
               auto trackURL = pt.get<std::string>("track_url");
               auto artistName = pt.get<std::string>("artist_name");
               auto artistURL = pt.get<std::string>("artist_url");

               discord::Activity activity{};
               activity.SetType(discord::ActivityType::Listening);
               activity.SetDetails(artistName.c_str());
               activity.SetState(trackName.c_str());
               activity.GetAssets().SetSmallImage("the");
               activity.GetAssets().SetSmallText("i mage");
               activity.GetAssets().SetLargeImage("the");
               activity.GetAssets().SetLargeText("u mage");
               discordState.core->ActivityManager().UpdateActivity(activity, [](discord::Result result) {
                   std::cout << ((result == discord::Result::Ok) ? "Succeeded" : "Failed")
                             << " updating activity!\n";
               });
           });
       }
#pragma clang diagnostic pop
    });

    boost::thread t([&io_svc] { io_svc.run(); });

    do {
        discordState.core->RunCallbacks();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (!interrupted);

    t.interrupt();
    t.join();

    return 0;
}
