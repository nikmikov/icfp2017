#include "game.h"


void
make_move(State* state, proto::Move* move)
{
    for (uint32_t i = 0; i < state->num_edges(); ++i) {
        Edge* e = state->get_edge(i);
        if (!e->is_claimed()) {
            // claim this edge
            *move = proto::Move(state->whoami(), e->source, e->target);

        } else {
            std::cerr << "CLAIMED: " << e->source << " ==> " << e->target << ", by: " << e->claimed_by
                      << std::endl;
        }
    }
}
