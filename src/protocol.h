#pragma once

#include <string>
#include <vector>
#include "picojson/picojson.h"

namespace proto {

struct Site {
    int id;
};

struct River {
    int source;
    int target;
};

struct Map {
    std::vector<Site> sites;
    std::vector<River> rivers;
    std::vector<int> mines;
};

struct Setup {
    int punter;
    int punters;
    Map map;
};

enum MoveType {CLAIM, PASS};

struct Move {
    Move() {}
    Move(int p, int s, int t): move_type(CLAIM), punter(p), source(s), target(t) {}
    Move(int p): move_type(PASS), punter(p) {}
    MoveType move_type;
    int punter;
    int source;
    int target;
};

typedef std::vector<Move> Moves;


//**********************************************

namespace json = picojson;

std::string read_handshake(const std::string& raw);

std::string write_handshake(const std::string& handshake);

Setup read_setup(const json::value::object& root);

std::string write_punter_ready(int punter, const std::string& state);

void read_moves(const json::value::object& root, Moves* game);

std::string write_move(const proto::Move& move, const std::string& state);

}
