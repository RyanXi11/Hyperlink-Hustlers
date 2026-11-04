#include "graph/Algorithms.h"
#include <limits>

using namespace std;
const double INF = numeric_limits<double>::infinity();

PathResult bellmanFord(const Graph& graph, int start_id, int target_id) {
    int num_vertices = graph.vertexCount();
    vector<double> dist(num_vertices, INF);
    vector<int> pred(num_vertices, -1);
    
    dist[start_id] = 0;
    int nodes_explored = 0;

    // Bellman-Ford relaxes edges up to V-1 times
    for (int i = 0; i < num_vertices - 1; ++i) {
        bool updated = false;
        
        for (int u = 0; u < num_vertices; ++u) {
            if (dist[u] == INF) continue;
            
            nodes_explored++; 
            
            for (int v : graph.outNeighbors(u)) {
                // edgeWeight automatically handles weighted/unweighted mode
                double edge_cost = graph.edgeWeight(u, v); 
                
                if (dist[u] + edge_cost < dist[v]) {
                    dist[v] = dist[u] + edge_cost;
                    pred[v] = u;
                    updated = true;
                }
            }
        }
        
        // Early stopping: if a full pass makes no updates, we're done
        if (!updated) {
            break;
        }
    }

    PathResult result;
    result.path_length = dist[target_id];
    result.nodes_explored = nodes_explored;
    
    // Reconstruct path
    if (dist[target_id] != INF) {
        int curr = target_id;
        while (curr != -1) {
            result.path.insert(result.path.begin(), curr);
            curr = pred[curr];
        }
    }
    
    return result;
}
