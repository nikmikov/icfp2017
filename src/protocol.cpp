#include "protocol.h"

#include <string>
#include <assert.h>

#include "picojson/picojson.h"

namespace proto {

std::string
read_handshake(const std::string& raw)
{
    std::string handshake;
    json::value jsn;
    std::string err = json::parse(jsn, raw);
    assert(err.empty());

    const auto& root = jsn.get<json::object>();

    return root.at("you").get<std::string>();
}

std::string
write_handshake(const std::string& handshake)
{
    json::value v;
    v.set<json::object>(json::object());
    v.get<json::object>()["me"] =  json::value(handshake);
    return v.serialize();
}


Setup
read_setup(const json::value::object& root)
{
    std::cerr << "Reading setup" << std::endl;
    Setup result;
    result.punter = static_cast<int>(root.at("punter").get<double>());
    result.punters = static_cast<int>(root.at("punters").get<double>());

    const auto& map = root.at("map").get<json::object>();
    const auto& sites = map.at("sites").get<json::array>();
    const auto& rivers = map.at("rivers").get<json::array>();
    const auto& mines = map.at("mines").get<json::array>();

    result.map.sites.reserve(sites.size());
    result.map.rivers.reserve(rivers.size());
    result.map.mines.reserve(mines.size());

    for (const auto& site: sites) {
        int site_id = static_cast<int>(site.get<json::object>().at("id").get<double>());
        result.map.sites.push_back( {site_id} );
    }
    for (const auto& elem: rivers) {
        const auto& river = elem.get<json::object>();
        int source = static_cast<int>(river.at("source").get<double>());
        int target = static_cast<int>(river.at("target").get<double>());
        result.map.rivers.push_back( {source, target} );
    }
    for (const auto& elem: mines) {
        int site_id = static_cast<int>(elem.get<double>());
        result.map.mines.push_back( {site_id} );
    }
    return result;
}

std::string
write_punter_ready(int punter, const std::string& state)
{
    json::value v;
    v.set<json::object>(json::object());
    v.get<json::object>()["ready"] = json::value(static_cast<double>(punter));
    v.get<json::object>()["state"] = json::value(state);
    return v.serialize();
}

void
read_moves(const json::value::object& root, Moves* moves)
{
    moves->resize(0);
    const auto& moves_root = root.at("move").get<json::object>();
    for(const auto& elem: moves_root.at("moves").get<json::array>()) {
        const auto& m = elem.get<json::object>();
        if (m.find("pass") != m.end()) {
            const auto& pass = m.at("pass").get<json::object>();
            int punter = static_cast<int>(pass.at("punter").get<double>());
            moves->emplace_back(punter);
        } else if (m.find("claim") != m.end()) {
            const auto& claim = m.at("claim").get<json::object>();
            int punter = static_cast<int>(claim.at("punter").get<double>());
            int source = static_cast<int>(claim.at("source").get<double>());
            int target = static_cast<int>(claim.at("target").get<double>());
            moves->emplace_back(punter, source, target);
        } else {
            std::cerr << "Unknown move type: " << elem << std::endl;
            exit(1);
        }
    }
}

std::string
write_move(const proto::Move& move, const std::string& state)
{
    json::value v;
    v.set<json::object>(json::object());
    json::value::object m;
    m["punter"] = json::value(static_cast<double>(move.punter));
    switch(move.move_type){
    case CLAIM: {
        m["source"] = json::value(static_cast<double>(move.source));
        m["target"] = json::value(static_cast<double>(move.target));
        v.get<json::object>()["claim"] = json::value(m);
        break;
    }
    case PASS:
        v.get<json::object>()["pass"] = json::value(m);
        break;
    }
    v.get<json::object>()["state"] = json::value(state);
    return v.serialize();
}

}
