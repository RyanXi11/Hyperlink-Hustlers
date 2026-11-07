#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>
 
#include "graph/Algorithms.h"
#include "graph/Graph.h"
#include "graph/MinHeap.h"
#include "graph/TitleMap.h"

namespace {

int failures = 0;

#define CHECK(condition)                                                              \
    do {                                                                              \
        if (!(condition)) {                                                           \
            ++failures;                                                               \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #condition \
                      << "\n";                                                        \
        }                                                                             \
    } while (0)

const std::string kFixtures = FIXTURE_DIR;

bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

std::string writeTemp(const std::string& name, const std::string& contents) {
    std::filesystem::path path = std::filesystem::temp_directory_path() / ("wikirace_" + name);
    std::ofstream file(path, std::ios::binary);
    file << contents;
    return path.string();
}

Graph loadTiny() {
    Graph graph;
    CHECK(graph.loadEdges(kFixtures + "/tiny_edges.txt"));
    CHECK(graph.loadTitles(kFixtures + "/tiny_names.txt"));
    return graph;
}

bool reverseListsMatch(const Graph& graph) {
    long long inTotal = 0;
    for (int v = 0; v < graph.vertexCount(); ++v) {
        inTotal += graph.inDegree(v);
        for (int u : graph.outNeighbors(v)) {
            bool found = false;
            for (int w : graph.inNeighbors(u)) {
                if (w == v) found = true;
            }
            if (!found) return false;
        }
    }
    return inTotal == graph.edgeCount();
}

bool weaklyConnected(const Graph& graph) {
    if (graph.vertexCount() == 0) return true;
    std::vector<bool> seen(graph.vertexCount(), false);
    std::vector<int> stack = {0};
    seen[0] = true;
    int reached = 1;
    while (!stack.empty()) {
        int v = stack.back();
        stack.pop_back();
        for (const std::vector<int>* neighbors : {&graph.outNeighbors(v), &graph.inNeighbors(v)}) {
            for (int u : *neighbors) {
                if (!seen[u]) {
                    seen[u] = true;
                    ++reached;
                    stack.push_back(u);
                }
            }
        }
    }
    return reached == graph.vertexCount();
}

void testAdjacencyLists() {
    Graph graph;
    CHECK(graph.loadEdges(kFixtures + "/tiny_edges.txt"));
    CHECK(graph.vertexCount() == 6);
    CHECK(graph.edgeCount() == 8);
    CHECK(graph.outNeighbors(0) == (std::vector<int>{1, 2}));
    CHECK(graph.outNeighbors(2) == (std::vector<int>{0, 3}));
    CHECK(graph.inNeighbors(2) == (std::vector<int>{0, 1}));
    CHECK(graph.inNeighbors(3) == (std::vector<int>{2, 5}));
    CHECK(graph.outDegree(4) == 1);
    CHECK(graph.inDegree(0) == 1);
    CHECK(reverseListsMatch(graph));
    CHECK(!graph.hasTitles());
}

void testTitles() {
    Graph graph = loadTiny();
    CHECK(graph.vertexCount() == 7);
    CHECK(graph.outDegree(6) == 0 && graph.inDegree(6) == 0);
    CHECK(graph.idForTitle("Gators") == 0);
    CHECK(graph.idForTitle("United States") == 2);
    CHECK(graph.idForTitle("New York City") == 5);
    CHECK(graph.idForTitle("Isolated Page") == 6);
    CHECK(graph.idForTitle("united states") == -1);
    CHECK(graph.idForTitle("Not An Article") == -1);
    CHECK(graph.idForTitle("") == -1);
    CHECK(graph.titleForId(4) == "Pizza");
    CHECK(graph.titleForId(99).empty());
    CHECK(graph.titleForId(-1).empty());
    CHECK(graph.duplicateTitleCount() == 0);
}

void testFlexibleEdgeFormat() {
    std::string path = writeTemp("flex_edges.txt", "0,1\r\n1\t2\r\n  # comment\r\n\r\n% also a comment\n2 0");
    Graph graph;
    CHECK(graph.loadEdges(path));
    CHECK(graph.vertexCount() == 3);
    CHECK(graph.edgeCount() == 3);
    CHECK(graph.outNeighbors(1) == (std::vector<int>{2}));
    CHECK(graph.outNeighbors(2) == (std::vector<int>{0}));
}

void testMalformedEdgesKeepExistingGraph() {
    Graph graph;
    CHECK(graph.loadEdges(kFixtures + "/tiny_edges.txt"));
    std::string path = writeTemp("bad_edges.txt", "0 1\n2 x\n");
    CHECK(!graph.loadEdges(path));
    CHECK(graph.lastError().find(":2:") != std::string::npos);
    CHECK(graph.vertexCount() == 6);
    CHECK(graph.edgeCount() == 8);
    CHECK(!graph.loadEdges(writeTemp("missing_target.txt", "5\n")));
    CHECK(!graph.loadEdges(writeTemp("trailing_text.txt", "1 2 3\n")));
    CHECK(!graph.loadEdges("does/not/exist.txt"));
}

void testDuplicateTitlesKeepFirstId() {
    Graph graph;
    CHECK(graph.loadTitles(writeTemp("dup_names.txt", "0 Same\n1 Same\n2 Other\r\n")));
    CHECK(graph.idForTitle("Same") == 0);
    CHECK(graph.idForTitle("Other") == 2);
    CHECK(graph.duplicateTitleCount() == 1);
}

void testSubgraph() {
    Graph graph = loadTiny();

    Graph sub = graph.inducedConnectedSubgraph(2, 4);
    CHECK(sub.vertexCount() == 4);
    CHECK(sub.edgeCount() == 5);
    CHECK(sub.titleForId(0) == "United States");
    CHECK(sub.idForTitle("Gators") != -1);
    CHECK(sub.idForTitle("Pizza") == -1);
    CHECK(reverseListsMatch(sub));
    CHECK(weaklyConnected(sub));

    Graph whole = graph.inducedConnectedSubgraph(0, 100);
    CHECK(whole.vertexCount() == 6);
    CHECK(whole.edgeCount() == 8);
    CHECK(whole.idForTitle("Isolated Page") == -1);
    CHECK(weaklyConnected(whole));

    CHECK(graph.inducedConnectedSubgraph(-1, 4).vertexCount() == 0);
    CHECK(graph.inducedConnectedSubgraph(0, 0).vertexCount() == 0);
}

void testVertexWeights() {
    Graph graph = loadTiny();
    // In-degrees: 2 and 3 have 2 (the max), 0, 1, 4, 5 have 1, isolated 6 has 0.
    CHECK(near(graph.vertexWeight(2), 1.0));
    CHECK(near(graph.vertexWeight(3), 1.0));
    CHECK(near(graph.vertexWeight(0), 2.0));
    CHECK(near(graph.vertexWeight(4), 2.0));
    CHECK(near(graph.vertexWeight(6), 2.0));

    Graph wide;
    CHECK(wide.loadEdges(writeTemp("wide_edges.txt", "1 0\n2 0\n3 0\n4 0\n0 1\n")));
    CHECK(near(wide.vertexWeight(0), 1.0));
    CHECK(near(wide.vertexWeight(1), 3.0));
}

void testWeightedToggle() {
    Graph graph = loadTiny();
    CHECK(!graph.isWeighted());
    CHECK(near(graph.edgeWeight(2, 0), 1.0));
    CHECK(near(graph.edgeWeight(0, 2), 1.0));

    graph.setWeighted(true);
    CHECK(graph.isWeighted());
    CHECK(near(graph.edgeWeight(2, 0), 2.0));
    CHECK(near(graph.edgeWeight(0, 2), 1.0));

    graph.setWeighted(false);
    CHECK(near(graph.edgeWeight(2, 0), 1.0));
}

void testSubgraphKeepsOriginalWeights() {
    Graph graph = loadTiny();
    graph.setWeighted(true);
    Graph sub = graph.inducedConnectedSubgraph(2, 4);
    CHECK(sub.isWeighted());
    // Sub ids 0..3 are original 2, 0, 3, 1. Original 3 keeps weight 1.0 even though
    // its in-link from 5 is dropped in the subgraph.
    CHECK(near(sub.vertexWeight(0), 1.0));
    CHECK(near(sub.vertexWeight(1), 2.0));
    CHECK(near(sub.vertexWeight(2), 1.0));
    CHECK(sub.inDegree(2) == 1);
    CHECK(near(sub.vertexWeight(3), 2.0));
    CHECK(near(sub.edgeWeight(0, 1), 2.0));
}

void testTitleMapGrowth() {
    TitleMap map;
    const int count = 100000;
    for (int i = 0; i < count; ++i) {
        CHECK(map.insert("Article " + std::to_string(i), i));
    }
    CHECK(map.size() == static_cast<std::size_t>(count));
    CHECK(map.capacity() >= map.size() * 2);
    CHECK(!map.insert("Article 42", -5));
    bool allFound = true;
    for (int i = 0; i < count; ++i) {
        if (map.find("Article " + std::to_string(i)) != i) allFound = false;
    }
    CHECK(allFound);
    CHECK(map.find("Article 100000") == -1);
    map.clear();
    CHECK(map.size() == 0 && map.find("Article 1") == -1);
}

Graph graphFromEdges(const std::string& name, const std::vector<std::pair<int, int>>& edges) {
    std::string contents;
    for (const auto& [a, b] : edges) contents += std::to_string(a) + " " + std::to_string(b) + "\n";
    Graph graph;
    CHECK(graph.loadEdges(writeTemp("search_" + name, contents)));
    return graph;
}
 
bool validPath(const Graph& graph, const PathResult& r, int start, int target) {
    if (r.path.empty() || r.path.front() != start || r.path.back() != target) return false;
    for (std::size_t i = 0; i + 1 < r.path.size(); ++i) {
        const std::vector<int>& out = graph.outNeighbors(r.path[i]);
        if (std::find(out.begin(), out.end(), r.path[i + 1]) == out.end()) return false;
    }
    return true;
}
 
void testHeap() {
    MinHeap<double, int> heap;
    CHECK(heap.empty());
    std::mt19937 rng(7);
    for (int i = 0; i < 2000; ++i) heap.push(static_cast<double>(rng() % 500), i);
    CHECK(heap.size() == 2000);
    double previous = -1;
    bool sorted = true;
    while (!heap.empty()) {
        double key = heap.pop().key;
        if (key < previous) sorted = false;
        previous = key;
    }
    CHECK(sorted);
}
 
void testSearchesOnTinyFixture() {
    Graph graph;
    CHECK(graph.loadEdges(kFixtures + "/tiny_edges.txt"));
    CHECK(graph.loadTitles(kFixtures + "/tiny_names.txt"));
    int gators = graph.idForTitle("Gators");
    int pizza = graph.idForTitle("Pizza");
    int nyc = graph.idForTitle("New York City");
    int isolated = graph.idForTitle("Isolated Page");
 
    for (PathResult r : {bfs(graph, gators, pizza), bidirectionalBfs(graph, gators, pizza),
                         dfs(graph, gators, pizza), dijkstra(graph, gators, pizza)}) {
        CHECK(validPath(graph, r, gators, pizza));
        CHECK(r.nodes_explored > 0);
    }
    CHECK(near(bfs(graph, gators, pizza).path_length, 3));
    CHECK(near(bidirectionalBfs(graph, gators, pizza).path_length, 3));
    CHECK(near(dijkstra(graph, gators, pizza).path_length, 3));
    CHECK(near(bfs(graph, gators, nyc).path_length, 4));
 
    for (PathResult r : {bfs(graph, gators, isolated), bidirectionalBfs(graph, gators, isolated),
                         dfs(graph, gators, isolated), dijkstra(graph, gators, isolated)}) {
        CHECK(r.path.empty() && std::isinf(r.path_length));
    }
    CHECK(bfs(graph, pizza, gators).path.empty());  // links only go "downhill" into the 3-4-5 cycle
    CHECK(near(bfs(graph, 2, 2).path_length, 0) && bfs(graph, 2, 2).path.size() == 1);
    CHECK(near(bidirectionalBfs(graph, 2, 2).path_length, 0));
    CHECK(bfs(graph, -1, 2).path.empty() && dijkstra(graph, 0, 99).path.empty());
    CHECK(dfs(graph, 0, 99).path.empty() && bidirectionalBfs(graph, 99, 0).path.empty());
 
    graph.setWeighted(true);
    PathResult w = dijkstra(graph, gators, pizza);
    CHECK(near(w.path_length, 4.0) && validPath(graph, w, gators, pizza));
    CHECK(near(bfs(graph, gators, pizza).path_length, 3));
}
 
void testWeightedPrefersHub() {
    std::vector<std::pair<int, int>> e = {{0, 1}, {1, 4}, {0, 2}, {2, 3}, {3, 4}};
    for (int h = 5; h <= 9; ++h) {
        e.push_back({h, 2});
        e.push_back({h, 3});
        e.push_back({h, 4});
        e.push_back({0, h});  // keep helpers reachable
    }
    Graph graph = graphFromEdges("hub.txt", e);
    graph.setWeighted(false);
    PathResult flat = dijkstra(graph, 0, 4);
    CHECK(flat.path.size() == 3);  // the 2-click route (0 -> h -> 4 or 0 -> 1 -> 4)
    graph.setWeighted(true);
    PathResult weighted = dijkstra(graph, 0, 4);
    CHECK(validPath(graph, weighted, 0, 4));
    // with weights, the cost is the sum of destination weights and must match brute force.
    double cost = 0;
    for (std::size_t i = 0; i + 1 < weighted.path.size(); ++i)
        cost += graph.edgeWeight(weighted.path[i], weighted.path[i + 1]);
    CHECK(near(cost, weighted.path_length));
}
 
void testDfsIsNotShortest() {
    Graph graph = graphFromEdges("dfs.txt", {{0, 1}, {0, 2}, {2, 3}, {3, 4}, {4, 1}});
    PathResult b = bfs(graph, 0, 1);
    PathResult d = dfs(graph, 0, 1);
    CHECK(near(b.path_length, 1));
    CHECK(validPath(graph, d, 0, 1));
    CHECK(d.path_length >= b.path_length);
}
 
void testSelfLoopsIgnored() {
    Graph graph = graphFromEdges("loops.txt", {{0, 0}, {0, 1}, {1, 1}, {1, 2}});
    CHECK(near(bfs(graph, 0, 2).path_length, 2));
    CHECK(near(bidirectionalBfs(graph, 0, 2).path_length, 2));
    CHECK(near(dijkstra(graph, 0, 2).path_length, 2));
    CHECK(validPath(graph, dfs(graph, 0, 2), 0, 2));
}
 
// all algorithms must agree with each other
void testRandomGraphs() {
    std::mt19937 rng(12345);
    for (int trial = 0; trial < 300; ++trial) {
        int n = 4 + static_cast<int>(rng() % 40);
        std::vector<std::pair<int, int>> edges;
        int m = n * (1 + static_cast<int>(rng() % 4));
        for (int i = 0; i < m; ++i) edges.push_back({static_cast<int>(rng() % n), static_cast<int>(rng() % n)});
        edges.push_back({n - 1, 0});  // make sure vertex ids 0..n-1 all exist
        Graph graph = graphFromEdges("rand.txt", edges);
        int s = static_cast<int>(rng() % graph.vertexCount());
        int t = static_cast<int>(rng() % graph.vertexCount());
 
        PathResult b = bfs(graph, s, t), bi = bidirectionalBfs(graph, s, t), d = dfs(graph, s, t);
        graph.setWeighted(false);
        PathResult dj = dijkstra(graph, s, t), bf = bellmanFord(graph, s, t);
        bool reachable = !b.path.empty();
        CHECK(reachable == !bi.path.empty() && reachable == !d.path.empty() && reachable == !dj.path.empty());
        if (reachable) {
            CHECK(near(b.path_length, bi.path_length));
            CHECK(near(b.path_length, dj.path_length));
            CHECK(near(b.path_length, bf.path_length));
            CHECK(d.path_length >= b.path_length);
            CHECK(validPath(graph, b, s, t) && validPath(graph, bi, s, t));
            CHECK(validPath(graph, d, s, t) && validPath(graph, dj, s, t));
        }
        graph.setWeighted(true);
        PathResult wd = dijkstra(graph, s, t), wb = bellmanFord(graph, s, t);
        CHECK(wd.path.empty() == !reachable);
        if (reachable) {
            CHECK(near(wd.path_length, wb.path_length));
            CHECK(validPath(graph, wd, s, t));
        }
    }
}
 
}  // namespace close

int main() {
    testAdjacencyLists();
    testTitles();
    testFlexibleEdgeFormat();
    testMalformedEdgesKeepExistingGraph();
    testDuplicateTitlesKeepFirstId();
    testSubgraph();
    testVertexWeights();
    testWeightedToggle();
    testSubgraphKeepsOriginalWeights();
    testTitleMapGrowth();

    if (failures > 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "All graph tests passed\n";
    return 0;
}
