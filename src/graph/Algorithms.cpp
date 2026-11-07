#include "graph/Algorithms.h"
#include <limits>
#include <algorithm>
#include <queue>
#include <vector>
#include "graph/MinHeap.h"

using namespace std;
const double INF = numeric_limits<double>::infinity();

namespace {
 
bool validIds(const Graph& graph, int start_id, int target_id) {
    int n = graph.vertexCount();
    return start_id >= 0 && start_id < n && target_id >= 0 && target_id < n;
}
 
PathResult notFound(int explored) {
    PathResult result;
    result.path_length = INF;
    result.nodes_explored = explored;
    return result;
}
 
PathResult trivialPath(int v) {
    PathResult result;
    result.path_length = 0;
    result.nodes_explored = 0;
    result.path = {v};
    return result;
}
 
vector<int> pathFromParents(const vector<int>& parent, int end) {
    vector<int> path;
    for (int v = end; v != -1; v = parent[v]) path.push_back(v);
    reverse(path.begin(), path.end());
    return path;
}
 
PathResult hopResult(vector<int> path, int explored) {
    PathResult result;
    result.path_length = static_cast<double>(path.size() - 1);
    result.nodes_explored = explored;
    result.path = move(path);
    return result;
}
 
}

PathResult bfs(const Graph& graph, int start_id, int target_id) {
    if (!validIds(graph, start_id, target_id)) return notFound(0);
    if (start_id == target_id) return trivialPath(start_id);
 
    const int kUnseen = -2;
    vector<int> parent(graph.vertexCount(), kUnseen);
    queue<int> frontier;
    parent[start_id] = -1;
    frontier.push(start_id);
    int explored = 0;
 
    while (!frontier.empty()) {
        int u = frontier.front();
        frontier.pop();
        ++explored;
        for (int v : graph.outNeighbors(u)) {
            if (v == u || parent[v] != kUnseen) continue;
            parent[v] = u;
            if (v == target_id) return hopResult(pathFromParents(parent, v), explored);
            frontier.push(v);
        }
    }
    return notFound(explored);
}

PathResult bidirectionalBfs(const Graph& graph, int start_id, int target_id) {
    if (!validIds(graph, start_id, target_id)) return notFound(0);
    if (start_id == target_id) return trivialPath(start_id);
 
    const int n = graph.vertexCount();
    vector<int> distF(n, -1), distB(n, -1), parentF(n, -1), parentB(n, -1);
    vector<int> frontF = {start_id};
    vector<int> frontB = {target_id};
    distF[start_id] = 0;
    distB[target_id] = 0;
    int explored = 0;
 
    while (!frontF.empty() && !frontB.empty()) {
        const bool forward = frontF.size() <= frontB.size();
        vector<int>& front = forward ? frontF : frontB;
        vector<int>& dist = forward ? distF : distB;
        vector<int>& parent = forward ? parentF : parentB;
        const vector<int>& otherDist = forward ? distB : distF;
 
        vector<int> next;
        int bestTotal = -1;
        int meet = -1;
        for (int u : front) {
            ++explored;
            const vector<int>& neighbors = forward ? graph.outNeighbors(u) : graph.inNeighbors(u);
            for (int v : neighbors) {
                if (v == u || dist[v] != -1) continue;
                dist[v] = dist[u] + 1;
                parent[v] = u;
                next.push_back(v);
                if (otherDist[v] != -1) {
                    int total = dist[v] + otherDist[v];
                    if (bestTotal == -1 || total < bestTotal) {
                        bestTotal = total;
                        meet = v;
                    }
                }
            }
        }
        if (meet != -1) {
            vector<int> path = pathFromParents(parentF, meet);   // start..meet
            vector<int> tail = pathFromParents(parentB, meet);   // target..meet
            for (int i = static_cast<int>(tail.size()) - 2; i >= 0; --i) path.push_back(tail[i]);
            return hopResult(move(path), explored);
        }
        front = move(next);
    }
    return notFound(explored);
}
 
PathResult dfs(const Graph& graph, int start_id, int target_id) {
    if (!validIds(graph, start_id, target_id)) return notFound(0);
    if (start_id == target_id) return trivialPath(start_id);
 
    const int kUnseen = -2;
    vector<int> parent(graph.vertexCount(), kUnseen);
    vector<int> stack = {start_id};
    parent[start_id] = -1;
    int explored = 0;
 
    while (!stack.empty()) {
        int u = stack.back();
        stack.pop_back();
        ++explored;
        for (int v : graph.outNeighbors(u)) {
            if (v == u || parent[v] != kUnseen) continue;
            parent[v] = u;
            if (v == target_id) return hopResult(pathFromParents(parent, v), explored);
            stack.push_back(v);
        }
    }
    return notFound(explored);
}

PathResult bellmanFord(const Graph& graph, int start_id, int target_id) {
    int num_vertices = graph.vertexCount();

    if (start_id < 0 || start_id >= num_vertices || target_id < 0 || target_id >= num_vertices) {
        PathResult invalid;
        invalid.path_length = INF;
        invalid.nodes_explored = 0;
        return invalid;
    }

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
