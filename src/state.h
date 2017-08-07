#pragma once

#include <string>
#include <stdint.h>
#include <cassert>
#include "protocol.h"


#define UNDEFINED 0x3fffffff


struct Header {
    uint32_t punters_sz; // total number of punters
    uint32_t punter_id;  // our id2
    uint32_t move_seq;   // current move 0-based
    uint32_t nodes;      // total number of sites
    uint32_t edges;      // total number of rivers
    uint32_t mines;      // total number of mines
    uint32_t targets;
    uint32_t options_avail;
    uint8_t  has_futures;
    uint8_t  has_splurges;
};


struct Node {
    Node(): first_edge_ref(UNDEFINED), is_mine(0) {}
//    Node(const proto::Site& s): id(s.id) {}
//    uint32_t id;
    uint32_t first_edge_ref : 31;
    uint32_t is_mine : 1;
};

struct EdgeRef {
    uint32_t edge_id;
};

struct Edge {
    Edge(const proto::River& r): source(r.source), claimed(0), option(0),
                                 target(r.target), me(0), breadcrumb(0) {}
    uint32_t source:30;
    uint32_t claimed:1;    // 0: free, 1: claimed
    uint32_t option:1;     // 0: option available, 1: option executed

    uint32_t target:30;
    uint32_t me:1;         // 1: if I claimed or executed option on the link
    uint32_t breadcrumb:1; // secret flag

    bool is_claimed() const { return claimed != 0; }
    bool is_unclaimed() const { return claimed == 0; }
    bool can_pass() const { return is_unclaimed() || claimed_by_me(); }
    bool can_exec_opt() const { return option == 0; }
    bool claimed_by_me() const { return me != 0; }
    bool is_breadcrumb() const { return breadcrumb != 0; }
};

struct Target {
    Target(uint32_t s, uint32_t t): source(s), target(t), reached(0) {}
    uint32_t source;
    uint32_t target:31;
    uint32_t reached:1;

    bool is_reached() const {return reached != 0; }
};

static_assert(sizeof(Edge) == 8, "8 bytes expected");

struct Mine {
    uint32_t site_id;
};

class State {
public:
    State(proto::Setup& setup);
    State(const std::string& base64);

    std::string serialize() const;

    void update(const std::vector< proto::Move >& moves);

    uint32_t num_nodes() { return get_header()->nodes - 1; }
    uint32_t num_edges() { return get_header()->edges; }
    uint32_t num_mines() { return get_header()->mines; }
    uint32_t num_targets() { return get_header()->targets; }

    Header* get_header() { return header; }
    Node* get_nodes() { return nodes; }
    Edge* get_edges() { return edges; }
    Mine* get_mines() { return mines; }

    int whoami() { return static_cast<int>(get_header()->punter_id); }

    int moves_total(){ return num_edges() / get_header()->punters_sz;  }
    int moves_left() { return moves_total() - get_header()->move_seq;   }

    Edge* get_edge_by_ref(uint32_t edge_ref) { return get_edge(edge_refs[edge_ref].edge_id); }

    Node* get_node(uint32_t node_id) { return &nodes[node_id]; }
    Edge* get_edge(uint32_t edge_id) { return &edges[edge_id]; }
    Mine* get_mine(uint32_t mine_id) { return &mines[mine_id]; }
    Target* get_target(uint32_t t_id) { return &targets[t_id]; }

    bool is_mine(uint32_t node_id) { return get_node(node_id)->is_mine != 0; }

    proto::Move claim_edge(uint32_t source, uint32_t target) { return proto::Move::claim(whoami(), source, target); }
    proto::Move execute_option(uint32_t source, uint32_t target) {
        assert(get_header()->options_avail > 0);
        return proto::Move::option(whoami(), source, target);
    }

    std::vector<proto::Future> init_execution_plan();

//    bool claimed_by_me(Edge* e) { return (int)e->claimed_by == whoami(); }

    /** tange from..to edges reference*/
    std::pair<uint32_t, uint32_t>
    get_edges_iter(uint32_t source)
    {
        uint32_t from_ref = nodes[source].first_edge_ref;
        uint32_t to_ref = nodes[source + 1].first_edge_ref;
        assert(from_ref <= to_ref);
        return std::make_pair(from_ref, to_ref);
    }

    Edge*
    find_edge(uint32_t source, uint32_t target)
    {
        assert(source < header->nodes - 1);
        assert(target < header->nodes - 1);
        auto edges_iter = get_edges_iter(source);

        for(uint32_t ref = edges_iter.first; ref < edges_iter.second; ++ref) {
            Edge* e = get_edge_by_ref(ref);
            if (e->target == target || e->source == target) return e;
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
    Target* targets;

    char* sentinel;

    void update_pointers();
};
