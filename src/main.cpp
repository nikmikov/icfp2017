#include <iostream>
#include <string>
#include <cassert>
#include <chrono>
#include "io.h"
#include "protocol.h"
#include "state.h"
#include "game.h"
#include "picojson/picojson.h"

const char* PUNTER_NAME = "poopybutthole";

namespace json = picojson;

void
handshake()
{
    io::send(proto::write_handshake(PUNTER_NAME));
    std::string reply = io::receive();
    assert(proto::read_handshake(reply) == PUNTER_NAME);
}

void
setup(const json::value::object& root )
{
    // start timer for setup
    // const auto start = std::chrono::high_resolution_clock::now();
    auto setup = proto::read_setup(root);
    State state(setup);

    auto futures = state.init_execution_plan();

    io::send(proto::write_punter_ready(setup.punter, futures, state.serialize()));
}

void
gameplay(const json::value::object& root )
{
    proto::Moves moves;
    read_moves(root, &moves);
    State game_state(root.at("state").get<std::string>());
    std::cerr << "MOVES LEFT: " << game_state.moves_left() << std::endl;
    game_state.update(moves);

    proto::Move move;
    make_move(&game_state, &move);
    io::send(proto::write_move(move, game_state.serialize()));
}

void
scoring(const json::value::object& root )
{
    // print scores
    std::cerr << "Final scores:" << std::endl;
    for (const auto& elem: root.at("stop").get<json::object>().at("scores").get<json::array>()) {
        const auto& score = elem.get<json::object>();
        std::cerr << " - punter: " << static_cast<int>(score.at("punter").get<double>())
                  << " , score: " << static_cast<int>(score.at("score").get<double>())
                  << std::endl;
    }
}

int
main()
{
    std::cerr << "===BEGIN===" << std::endl;
    handshake();
    auto start_time = std::chrono::high_resolution_clock::now();

    auto raw = io::receive();
    json::value jsn;
    std::string err = json::parse(jsn, raw);
    if(!err.empty()) {
        std::cerr << raw << std::endl;
        std::cerr << err << std::endl;
        assert(err.empty());
    }
    const auto& root = jsn.get<json::object>();
    if (root.find("map") != root.end()) {
        setup(root);
    } else if (root.find("move") != root.end()) {
        gameplay(root);
    } else if (root.find("stop") != root.end()) {
        scoring(root);
    } else if (root.find("timeout") != root.end()) {
        std::cerr << "Timeout: "<< root.at("timeout").get<double>() << std::endl;
    } else {
        std::cerr << "Unknown game state: " << raw << std::endl;
        exit(1);
    }
    auto current_time = std::chrono::high_resolution_clock::now();
    std::cerr << "Elapsed: " << std::chrono::duration_cast<std::chrono::microseconds>(current_time - start_time).count() << " microseconds" << std::endl;
    std::cerr << "=== END ===" << std::endl;
    return 0;
}
