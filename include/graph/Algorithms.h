#pragma once
#include <vector>
#include "graph/Graph.h"

struct PathResult {
    double path_length;
    int nodes_explored;
    std::vector<int> path;
};

// runs the Bellman-Ford algorithm from start_id to target_id
PathResult bellmanFord(const Graph& graph, int start_id, int target_id);
