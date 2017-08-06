#include "state.h"

#include <cassert>
#include <algorithm>
#include "base64/base64.h"


State::State(const proto::Setup& setup):data(0)
{
    data.resize(sizeof(Header));
    header = reinterpret_cast<Header*>(data.data());
    header->punters_sz = setup.punters;
    header->punter_id = setup.punter;
    header->move_seq = 0;

    // find maximum node_id (assume that id's are contiguous and not random)
    int max_node_id = 0;
    for (const auto& a: setup.map.sites) {
        max_node_id = std::max(a.id, max_node_id);
    }

    header->nodes = max_node_id + 2; // + sentinel
    header->edges = setup.map.rivers.size();
    header->mines = setup.map.mines.size();

    update_pointers();
    data.resize(sentinel - data.data());
    update_pointers();

    std::vector<std::vector<uint32_t>> edges_of_nodes(header->nodes);
    for (size_t idx = 0; idx < setup.map.rivers.size(); ++idx) {
        edges[idx] = Edge(setup.map.rivers[idx]);
        assert(edges[idx].source < header->nodes);
        assert(edges[idx].target < header->nodes);
        edges_of_nodes[edges[idx].source].push_back(idx);
        edges_of_nodes[edges[idx].target].push_back(idx);
    }
    assert(edges_of_nodes.back().empty());


    for (uint32_t idx = 0, edge_iref = 0; idx < edges_of_nodes.size(); ++idx) {
        nodes[idx].first_edge_ref = edge_iref;
        for (uint32_t e_id: edges_of_nodes[idx]) {
            assert(edge_iref < header->edges * 2);
            edge_refs[edge_iref++].edge_id = e_id;
        }
    }

    for (size_t idx = 0; idx < setup.map.mines.size(); ++idx) {
        mines[idx].site_id = setup.map.mines[idx];
    }
}

State::State(const std::string& base64):data(0)
{
    Base64::Decode(base64, &data);
    update_pointers();
}

void
State::update_pointers()
{
    header = reinterpret_cast<Header*>(data.data());

    size_t nodes_offset = sizeof(Header);
    size_t edge_refs_offset = nodes_offset + sizeof(Node) * header->nodes;
    size_t edges_offset = edge_refs_offset + sizeof(EdgeRef) * header->edges * 2;
    size_t mines_offset = edges_offset + sizeof(Edge) * header->edges;

    sentinel = data.data() + mines_offset + sizeof(Mine) * header->mines;

    nodes = reinterpret_cast<Node*>(data.data() + nodes_offset);
    edge_refs = reinterpret_cast<EdgeRef*>(data.data() + edge_refs_offset);
    edges = reinterpret_cast<Edge*>(data.data() + edges_offset);
    mines = reinterpret_cast<Mine*>(data.data() + mines_offset);
}

std::string
State::serialize() const
{
    std::string result;
    Base64::Encode(data, &result);
//    std::cerr << "Encoded: " << data.size() << " bytes to " << result << std::endl;
    return result;
}


void
State::update(const std::vector< proto::Move >& moves)
{
    for(const auto& m: moves) {
        if (m.move_type == proto::CLAIM) {
            Edge* e = find_edge(m.source, m.target);
            assert(e != nullptr && e->claimed_by == UNDEFINED);
            e->claimed_by = m.punter;
        }
    }
    get_header()->move_seq++;
}
