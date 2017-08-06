#pragma once

#include <string>
#include <stdint.h>
#include <cassert>
#include "protocol.h"


#define UNDEFINED 0xffffffff


struct Header {
    uint32_t punters_sz; // total number of punters
    uint32_t punter_id;  // our id
    uint32_t move_seq;   // current move 0-based
    uint32_t nodes;      // total number of sites
    uint32_t edges;      // total number of rivers
    uint32_t mines;      // total number of mines
};


struct Node {
//    Node(const proto::Site& s): id(s.id) {}
//    uint32_t id;
    uint32_t first_edge_ref = UNDEFINED;
};

struct EdgeRef {
    uint32_t edge_id;
};

struct Edge {
    Edge(const proto::River& r): source(r.source), target(r.target) {}
    uint32_t source;
    uint32_t target;
    uint32_t claimed_by = UNDEFINED; // if claimed by punter or 0xffffffff if not claimed

    bool is_claimed() const { return claimed_by !=  UNDEFINED; }
};

struct Mine {
    uint32_t site_id;
};

class State {
public:
    State(const proto::Setup& setup);
    State(const std::string& base64);

    std::string serialize() const;

    void update(const std::vector< proto::Move >& moves);

    uint32_t num_edges() { return get_header()->edges;  }

    Header* get_header() { return header; }
    Node* get_nodes() { return nodes; }
    Edge* get_edges() { return edges; }
    Mine* get_mines() { return mines; }

    int whoami() { return static_cast<int>(get_header()->punter_id); }

    Edge* get_edge_by_ref(uint32_t edge_ref) { return get_edge(edge_refs[edge_ref].edge_id); }

    Edge* get_edge(uint32_t edge_id) { return &edges[edge_id]; }

    Edge*
    find_edge(uint32_t source, uint32_t target)
    {
        assert(source < header->nodes - 1);
        assert(target < header->nodes - 1);
        uint32_t from_ref = nodes[source].first_edge_ref;
        uint32_t to_ref = nodes[source + 1].first_edge_ref;
        assert(from_ref <= to_ref);
        for(;from_ref < to_ref; ++from_ref) {
            Edge* e = get_edge_by_ref(from_ref);
            if (e->target == target) return e;
        }
        return nullptr;
    }

private:
    std::vector<char> data;

    Header* header;
    Node* nodes;
    EdgeRef* edge_refs;
    Edge* edges;
    Mine* mines;

    char* sentinel;

    void update_pointers();
};
