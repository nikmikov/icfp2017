#include "game.h"

#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <random>

/** return number of unclaimed edges from node with given id */
std::vector<Edge*>
unclaimed_edges_from(State* state, uint32_t root)
{
    std::vector<Edge*> res;
    res.reserve(128);
    std::vector<uint32_t> queue;
    std::unordered_set<uint32_t> visited;
    queue.reserve(128);
    queue.push_back(root);
    visited.emplace(root);

    while(!queue.empty()) {
        uint32_t node = queue.back();

        queue.pop_back();
        auto iter = state->get_edges_iter(node);
        for (auto i = iter.first; i < iter.second; ++i) {
            Edge* e = state->get_edge_by_ref(i);
            if (e->claimed_by_me()) {
                // claimed by me
                uint32_t t = e->source == node ? e->target: e->source;
                if (visited.find(t) == visited.end()){
                    queue.push_back(t);
                    visited.emplace(t);
                }

            } else if(e->is_claimed()) {
                // claimed by others
                // can't travel
            } else {
                // free
                res.push_back(e);
            }
        }
        if (res.size() > 100) break;
    }

    return res;
}

// return path
std::vector<Edge*>
unpack_path(State* state, uint32_t from, uint32_t to, const std::unordered_map<uint32_t, uint32_t>& visited)
{
    std::vector<Edge*> result;
    uint32_t cur = to;
    do {
        const auto it = visited.at(cur);
        Edge* e = state->get_edge_by_ref(it);
        result.push_back(e);
        uint32_t prev = e->target != cur ? e->target: e->source;
        cur = prev;
    } while(cur != from);

    return result;
}

// return first unclaimed edge ref or nullptr
Edge*
path_to_nearest_unconnected_mine(State* state, uint32_t root)
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
            if (e->claimed_by_me() || !e->is_claimed()) {
                // can travel
                uint32_t t = e->source == node ? e->target: e->source;
                if (visited.find(t) != visited.end()) continue; // already visited
                queue.push(t);
                visited.emplace(t, i);
                if (state->is_mine(t)) {
                    // unpack path
                    auto path = unpack_path(state, root, t, visited);
                    std::cerr << "Path found:" << root << " -> " << t << " ===>  ";
                    for (auto it = path.rbegin(); it != path.rend(); ++it) {
                        Edge* eee = *it;
                        std::cerr << "(" << eee->source << "," << eee->target << "," << eee->claimed << eee->option <<  eee->me << ")";
                    }
                    std::cerr << std::endl;
                    for (auto it = path.rbegin(); it != path.rend(); ++it) {
                        if ((*it)->is_unclaimed()) {
                            return *it;
                        }
                    }
                }
            }
        }
    }
    return nullptr;
}

// return first unclaimed edge ref or nullptr
Edge*
shortest_path(State* state, uint32_t from, uint32_t to, bool use_options)
{
    std::queue<uint32_t> queue;
    std::unordered_map<uint32_t, uint32_t> visited;

    uint32_t opt_num = use_options ? state->get_header()->options_avail : 0;
    std::cerr << "OPTIONS LEFT: " << opt_num << std::endl;
    queue.push(from);
    visited.emplace(from, UNDEFINED);
    while(!queue.empty()) {
        uint32_t node = queue.front();
        queue.pop();
        auto iter = state->get_edges_iter(node);
        for (auto i = iter.first; i < iter.second; ++i) {
            Edge* e = state->get_edge_by_ref(i);
            if (e->can_pass() || (e->can_exec_opt() && opt_num >  0)) {
                // can travel
                uint32_t t = e->source == node ? e->target: e->source;
                if (visited.find(t) != visited.end()) continue; // already visited

                if(!e->can_pass()) {
                    std::cerr << "Dec opt: " << opt_num << std::endl;
                    --opt_num; // exec option
                }
                queue.push(t);
                visited.emplace(t, i);
                if (t == to) {
                    // target reached
                    // unpack path
                    auto path = unpack_path(state, from, to, visited);
#ifdef DEBUG
                    std::cerr << "Path found:" << from << " -> " << to << " ===>  ";
                    for (auto it = path.rbegin(); it != path.rend(); ++it) {
                        Edge* eee = *it;
                        std::cerr << "(" << eee->source << "," << eee->target << "," << eee->claimed << eee->option <<  eee->me << ")";
                    }
                    std::cerr << std::endl;
#endif
                    for (auto it = path.rbegin(); it != path.rend(); ++it) {
                        if (!(*it)->claimed_by_me()) {
                            return *it;
                        }
                    }
                    return *path.rbegin();
                }

            }
        }
    }
    return nullptr;

}


bool
connect_mines_move(State* state, proto::Move* move)
{
    std::mt19937 rng;
    rng.seed(std::random_device()());

    // pick start node
    // number of available starting edges <- mine_id
    std::vector< std::pair<int, uint32_t> > num_free_edges;
    for (uint32_t i = 0; i < state->num_mines(); ++i) {

        uint32_t node_id = state->get_mine(i)->site_id;
        std::cerr << "MINE: " << i << ":" << node_id << std::endl;
        assert(state->is_mine(node_id));
        auto n = unclaimed_edges_from(state, node_id);
        num_free_edges.emplace_back(n.size(), node_id);
    }

    std::sort(num_free_edges.begin(), num_free_edges.end());
    for(const auto& a: num_free_edges) {
        std::cerr << "Free: " << a.second << ":" << a.first << std::endl;
    }
    for(const auto& a: num_free_edges) {
        if (a.first <= 0) continue;
        Edge* edge = path_to_nearest_unconnected_mine(state, a.second);
        if (edge != nullptr) {
            assert(edge->is_unclaimed());
            *move = state->claim_edge(edge->source, edge->target);
            return true;
        }
    }

    for(const auto& a: num_free_edges) {
        if (a.first <= 0) continue;
        auto n = unclaimed_edges_from(state, a.second);
        std::uniform_int_distribution<std::mt19937::result_type> dist(0, n.size() - 1);
        Edge* e = n[dist(rng)];
        *move = state->claim_edge(e->source, e->target);
        return true;
    }

    return false;
}

bool
follow_breadcrumbs(State* state, proto::Move* move)
{

    for (size_t idx = 0; idx < state->num_targets(); ++idx) {
        Target* t = state->get_target(idx);
        std::cerr << idx << ": TARGETS :" << t->source << "->" << t->target << ", reached: " << t->is_reached() << std::endl;
        if (!t->is_reached()) {
            Edge* edge = shortest_path(state, t->source, t->target, true);
//            if (edge == nullptr) {
//                edge = shortest_path(state, t->source, t->target, true);
//            }
            if (edge != nullptr) {
                if ( (edge->source == t->source || edge->target == t->source) && edge->claimed_by_me() ) {
                    t->reached = 1;
                    std::cerr << "REACHED: " << t->source << "->" << t->target << std::endl;
                } else {
                    assert(!edge->claimed_by_me());
                    if (edge->is_claimed()) {
                        // execute option
                        *move = state->execute_option(edge->source, edge->target);
                    } else {
                        *move = state->claim_edge(edge->source, edge->target);
                    }
                    return true;
                }
            } else {
                // unreachable
                std::cerr << "UNREACHABLE: " << t->source << "->" << t->target << std::endl;
                t->reached = 1;
            }
        }
    }
    return false;
}

bool
random_move(State* state, proto::Move* move)
{
    for (uint32_t i = 0; i < state->num_edges(); ++i) {
        Edge* e = state->get_edge(i);
        if (!e->is_claimed()) {
            // claim this edge
            std::cerr << "Claiming random" << std::endl;
            *move = state->claim_edge(e->source, e->target);
            return true;
        }
    }
    return false;
}

bool
make_move(State* state, proto::Move* move)
{

    return follow_breadcrumbs(state, move)
        || random_move(state, move);
}
