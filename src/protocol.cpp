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
    result.has_futures = result.has_splurges = result.has_options = false;
    if (root.find("settings") != root.end()) {
        auto settings = root.at("settings").get<json::object>();
        result.has_futures = settings.find("futures") != settings.end()
            && settings.at("futures").get<bool>();
        result.has_splurges = settings.find("splurges") != settings.end()
            && settings.at("splurges").get<bool>();
        result.has_options = settings.find("options") != settings.end()
            && settings.at("options").get<bool>();
    }

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
write_punter_ready(int punter, const std::vector<Future>& futures, const std::string& state)
{
    json::value v;
    v.set<json::object>(json::object());
    v.get<json::object>()["ready"] = json::value(static_cast<double>(punter));
    v.get<json::object>()["state"] = json::value(state);
    if (!futures.empty()) {
        json::value::array futures_val;
        for (const auto& f: futures) {
            json::value::object m;
            m["source"] = json::value(static_cast<double>(f.source));
            m["target"] = json::value(static_cast<double>(f.target));
            futures_val.push_back(json::value(m));
        }
        v.get<json::object>()["futures"] = json::value(futures_val);
    }
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
            moves->push_back(Move::pass(punter));
        } else if (m.find("claim") != m.end()) {
            const auto& claim = m.at("claim").get<json::object>();
            int punter = static_cast<int>(claim.at("punter").get<double>());
            int source = static_cast<int>(claim.at("source").get<double>());
            int target = static_cast<int>(claim.at("target").get<double>());
            moves->push_back(Move::claim(punter, source, target));
        } else if (m.find("splurge") != m.end()) {
            // convert splurge to series of claim
            const auto& splurge = m.at("splurge").get<json::object>();
            int punter = static_cast<int>(splurge.at("punter").get<double>());
            const auto& route = splurge.at("route").get<json::array>();
            int prev = -1;
            for (const auto& elem: route) {
                int site_id = static_cast<int>(elem.get<double>());
                if (prev != -1) {
                    moves->push_back(Move::claim(punter, prev, site_id));
                }
                prev = site_id;
            }
        } else if (m.find("option") != m.end()) {
            const auto& option = m.at("option").get<json::object>();
            int punter = static_cast<int>(option.at("punter").get<double>());
            int source = static_cast<int>(option.at("source").get<double>());
            int target = static_cast<int>(option.at("target").get<double>());
            moves->push_back(Move::option(punter, source, target));
        } else {
            std::cerr << "Unknown move type: " << elem << std::endl;
            assert(false);
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
    case PASS:{
        v.get<json::object>()["pass"] = json::value(m);
        break;
    }
    case SPLURGE: {
        v.get<json::object>()["pass"] = json::value(m);
        break;
    }
    case OPTION: {
        m["source"] = json::value(static_cast<double>(move.source));
        m["target"] = json::value(static_cast<double>(move.target));
        v.get<json::object>()["option"] = json::value(m);
        break;
    }
    }
    v.get<json::object>()["state"] = json::value(state);
    return v.serialize();
}

}
