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
    bool has_futures;
    bool has_splurges;
    bool has_options;
    Map map;
};

struct Future {
    Future(int s, int t): source(s),target(t) {}
    int source;
    int target;
};

enum MoveType {CLAIM, PASS, SPLURGE, OPTION};

struct Move {
    Move() {}
    Move(MoveType tpe, int p, int s, int t): move_type(tpe), punter(p), source(s), target(t) {}
    static inline Move claim(int p, int s, int t) { return Move(CLAIM, p, s, t); }
    static inline Move pass(int p) { return Move(PASS, p, 0, 0); }
    static inline Move option(int p, int s, int t) { return Move(OPTION, p, s, t); }

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

std::string write_punter_ready(int punter, const std::vector<Future>& futures, const std::string& state);

void read_moves(const json::value::object& root, Moves* game);

std::string write_move(const proto::Move& move, const std::string& state);

}
