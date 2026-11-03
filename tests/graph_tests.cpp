#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "graph/Graph.h"
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

}  // namespace

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
