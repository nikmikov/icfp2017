#include "state.h"

#include <cassert>
#include <cmath>
#include <algorithm>
#include <random>
#include <queue>
#include <unordered_map>
#include "base64/base64.h"


namespace {


// return path
void
unpack_path(State* state, uint32_t from, uint32_t to,
            const std::unordered_map<uint32_t, uint32_t>& visited,
            std::vector<Edge*>* result)
{
    if (from == to) return;
    uint32_t cur = to;
    do {
        const auto it = visited.at(cur);
        Edge* e = state->get_edge_by_ref(it);
        result->push_back(e);
        uint32_t prev = e->target != cur ? e->target: e->source;
        cur = prev;
    } while(cur != from);
    std::reverse(result->begin(), result->end());
}

bool
nearest_mine_path(State* state, uint32_t root, std::vector<Edge*>* path)
{
    std::queue<uint32_t> queue;
    std::unordered_map<uint32_t, uint32_t> visited;

    queue.push(root);
    visited.emplace(root, UNDEFINED);

    while(!queue.empty()) {
        uint32_t node = queue.front();
        queue.pop();
        auto iter = state->get_edges_iter(node);
        for (auto i = iter.first; i < iter.second; ++i) {
            Edge* e = state->get_edge_by_ref(i);
            // can travel
            uint32_t t = e->source == node ? e->target: e->source;
            if (visited.find(t) != visited.end()) continue; // already visited
            queue.push(t);
            visited.emplace(t, i);
            if (state->is_mine(t)) {
                // unpack path
                path->clear();
                unpack_path(state, root, t, visited, path);
                return true;
            }
        }
    }
    return false;
}

bool
longest_breadcrumb_path(State* state, uint32_t root, std::vector<Edge*>* path)
{
    std::queue<uint32_t> queue;
    std::unordered_map<uint32_t, uint32_t> visited;

    queue.push(root);
    visited.emplace(root, UNDEFINED);

    while(!queue.empty()) {
        uint32_t node = queue.front();
        queue.pop();
        auto iter = state->get_edges_iter(node);
        for (auto i = iter.first; i < iter.second; ++i) {
            Edge* e = state->get_edge_by_ref(i);
            if (!e->is_breadcrumb()) continue;
            // can travel
            uint32_t t = e->source == node ? e->target: e->source;
            if (visited.find(t) != visited.end()) continue; // already visited
            queue.push(t);
            visited.emplace(t, i);
        }
        if (queue.empty() && root != node) {
            unpack_path(state, root, node, visited, path);
            return true;
        }
    }
    return false;

}

void
setup_execution_plan(State* state, std::vector<proto::Future>* futures,  std::vector<Target>* targets)
{
    std::vector<std::vector<Edge*>> mine_paths(state->num_mines());
    // for each mine find shorest path to another main
    std::vector<std::pair<int, int>> ordered_by_shortest;
    for (uint32_t i = 0; i < state->num_mines(); ++i) {
        bool res = nearest_mine_path(state, state->get_mine(i)->site_id, &mine_paths[i]);
        assert(res);
        if (res) {
            ordered_by_shortest.emplace_back(mine_paths[i].size(), i);
        }
    }

    double moves_buffer = state->get_header()->has_futures ? 0.1 : 0.05;
    std::sort(ordered_by_shortest.begin(), ordered_by_shortest.end());
    int moves_total_adj = static_cast<int>(ceil( (1.0 - moves_buffer) * state->moves_total() ));
    int total_len = 0;
    for (const auto& p: ordered_by_shortest) {
        uint32_t mine_id = p.second;
        total_len += p.first;
        if (total_len >= moves_total_adj) break;
        for (Edge* e: mine_paths[mine_id]) {
            e->breadcrumb = 1;
        }
        Edge* last_edge = mine_paths[mine_id].back();
        uint32_t mine_target = state->is_mine(last_edge->target) ? last_edge->target : last_edge->source;
        assert(state->is_mine(mine_target));
        uint32_t m = state->get_mine(mine_id)->site_id;
        targets->emplace_back(std::min(m,mine_target), std::max(m,mine_target));
#ifdef DEBUG
        std::cerr << "Moves used: " << total_len << " of " << moves_total_adj;
        std::cerr << ".Path: " <<  state->get_mine(mine_id)->site_id << " to "
                  << mine_paths[mine_id].back()->target << ", len: " << mine_paths[mine_id].size()
                  << std::endl;

#endif
    }
    std::reverse(targets->begin(), targets->end());

    if (state->get_header()->has_futures) {
        int moves_left = moves_total_adj - total_len;
        // calculate futures
        // find longest breadcrumb path
        size_t NFUT = (size_t)ceil( (state->get_header()->options_avail > 0 ? 0.3: 0.1) * state->num_mines());
        std::vector<Edge*> longest, work;
        for (uint32_t i = 0; i < NFUT ; ++i) {
            //
            work.resize(0);
            uint32_t mine_id = state->get_mine(i)->site_id;
            longest_breadcrumb_path(state, mine_id, &work);
#ifdef DEBUG
            std::cerr << "Longest for " << mine_id << ": " << work.size() << std::endl;
#endif
            if (work.size() <= 1) continue;
//            if (work.size() > longest.size()) {
//                longest.swap(work);
//            } else {
            Edge* last = work.back();
            uint32_t lfrom = last->source, lto = last->target;
            uint32_t n = state->is_mine(lto) ? lfrom : lto;
            assert(!state->is_mine(n));
            assert(mine_id != n);
            futures->emplace_back(mine_id, n);
//            targets->emplace_back(lfrom, lto);
#ifdef DEBUG
            std::cerr << "FUTURE: " << mine_id << "->" << n << ", target: " << lfrom << "->" << lto << std::endl;
#endif
//            }
        }

    }


    //

    for (const auto& f: *futures) {
        targets->emplace_back(f.source, f.target);
    }
    std::reverse(targets->begin(), targets->end());
#ifdef  DEBUG
    std::cerr << "Targets: " <<  targets->size() << " ====> ";
    for (const Target& t: *targets) {
        std::cerr << "(" << t.source << "," << t.target << ") ";
    }
    std::cerr << std::endl;
#endif



}

}

State::State(proto::Setup& setup):data(0)
{
    data.resize(sizeof(Header));
    header = reinterpret_cast<Header*>(data.data());
    header->punters_sz = setup.punters;
    header->punter_id = setup.punter;
    header->move_seq = 0;
    std::cerr << "I AM A PUNTER #" << setup.punter << std::endl;

    std::random_device rd;
    std::mt19937 g(rd());

    std::shuffle(setup.map.mines.begin(), setup.map.mines.end(), g);

    // find maximum node_id (assume that id's are contiguous and not random)
    int max_node_id = 0;
    for (const auto& a: setup.map.sites) {
        max_node_id = std::max(a.id, max_node_id);
    }

    header->nodes = max_node_id + 2; // + sentinel
    header->edges = setup.map.rivers.size();
    header->mines = setup.map.mines.size();
    header->targets = 0;

    header->has_futures = setup.has_futures;
    header->options_avail = setup.has_options ? header->mines : 0;
    header->has_splurges = setup.has_splurges;
#ifdef DEBUG
    std::cerr << "Settings: futures: " <<  (header->has_futures != 0)
              << ", splurges: " << (header->has_splurges != 0)
              << ", options: " << header->options_avail
              << std::endl;
#endif
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
        nodes[idx].is_mine = 0;
        for (uint32_t e_id: edges_of_nodes[idx]) {
            assert(edge_iref < header->edges * 2);
            edge_refs[edge_iref++].edge_id = e_id;
        }
    }

    for (size_t idx = 0; idx < setup.map.mines.size(); ++idx) {
        uint32_t site_id = setup.map.mines[idx];
        mines[idx].site_id = site_id;
        nodes[site_id].is_mine = 1;
    }

}

State::State(const std::string& base64):data(0)
{
    Base64::Decode(base64, &data);
    update_pointers();
}

std::vector<proto::Future>
State::init_execution_plan()
{
    std::vector<proto::Future> res;
    std::vector<Target> targs;
    setup_execution_plan(this, &res, &targs);

    header->targets = targs.size();
    update_pointers();
    data.resize(sentinel - data.data());
    update_pointers();
    for (size_t idx = 0; idx < targs.size(); ++idx) {
        Target*t = &targets[idx];
        *t = targs[idx];
    }


    return res;
}

void
State::update_pointers()
{
    header = reinterpret_cast<Header*>(data.data());

    size_t nodes_offset = sizeof(Header);
    size_t edge_refs_offset = nodes_offset + sizeof(Node) * header->nodes;
    size_t edges_offset = edge_refs_offset + sizeof(EdgeRef) * header->edges * 2;
    size_t mines_offset = edges_offset + sizeof(Edge) * header->edges;
    size_t targets_offset = mines_offset + sizeof(Mine) * header->mines;

    sentinel = data.data() + targets_offset + sizeof(Target) * header->targets;

    nodes = reinterpret_cast<Node*>(data.data() + nodes_offset);
    edge_refs = reinterpret_cast<EdgeRef*>(data.data() + edge_refs_offset);
    edges = reinterpret_cast<Edge*>(data.data() + edges_offset);
    mines = reinterpret_cast<Mine*>(data.data() + mines_offset);
    targets = reinterpret_cast<Target*>(data.data() + targets_offset);
}

std::string
State::serialize() const
{
    std::string result;
    Base64::Encode(data, &result);
    return result;
}


void
State::update(const std::vector< proto::Move >& moves)
{
    for(const auto& m: moves) {
        if (m.move_type == proto::CLAIM || m.move_type == proto::OPTION) {
            Edge* e = find_edge(m.source, m.target);
            assert(e != nullptr);

            bool claimed_by_me = m.punter == whoami();
            std::string punter = (claimed_by_me ? "me" : std::to_string(m.punter));
            if (e->is_unclaimed()) {
#ifdef DEBUG
                std::cerr << "Edge claimed: " << m.source << "->" << m.target << ", by: "
                          <<  punter << std::endl;
#endif
                e->claimed = 1;
                e->me = claimed_by_me;
            } else {
#ifdef DEBUG
                std::cerr << "Option executed: " << m.source << "->" << m.target << ", by: "
                          <<  punter << std::endl;
#endif

                assert(e->can_exec_opt());
                e->option = 1;
                get_header()->options_avail--;
                if (e->me == 0) e->me = claimed_by_me;
            }
        }
    }
    get_header()->move_seq++;
}
