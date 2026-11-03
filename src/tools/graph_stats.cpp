// Loads the wiki-topcats graph and reports its size, load time, memory, and title lookups.
//
// Usage: graph_stats <edges.txt> [titles.txt] [--max-vertices N] [--seed ID] [--lookup TITLE]...

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "graph/Graph.h"

namespace {

using Clock = std::chrono::steady_clock;

long long elapsedMs(Clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
}

std::string withCommas(long long value) {
    std::string digits = std::to_string(value < 0 ? -value : value);
    std::string out;
    for (std::size_t i = 0; i < digits.size(); ++i) {
        if (i > 0 && (digits.size() - i) % 3 == 0) {
            out += ',';
        }
        out += digits[i];
    }
    return value < 0 ? "-" + out : out;
}

std::string describe(const Graph& graph, int id) {
    const std::string& title = graph.titleForId(id);
    return "id " + std::to_string(id) + (title.empty() ? "" : " \"" + title + "\"");
}

int maxOutDegreeVertex(const Graph& graph) {
    int best = 0;
    for (int v = 1; v < graph.vertexCount(); ++v) {
        if (graph.outDegree(v) > graph.outDegree(best)) best = v;
    }
    return best;
}

int maxInDegreeVertex(const Graph& graph) {
    int best = 0;
    for (int v = 1; v < graph.vertexCount(); ++v) {
        if (graph.inDegree(v) > graph.inDegree(best)) best = v;
    }
    return best;
}

void printStats(const Graph& graph) {
    if (graph.vertexCount() == 0) {
        std::cout << "  Graph is empty\n";
        return;
    }
    long long selfLoops = 0;
    int noOut = 0;
    int noIn = 0;
    for (int v = 0; v < graph.vertexCount(); ++v) {
        for (int u : graph.outNeighbors(v)) {
            if (u == v) ++selfLoops;
        }
        if (graph.outDegree(v) == 0) ++noOut;
        if (graph.inDegree(v) == 0) ++noIn;
    }
    int maxOut = maxOutDegreeVertex(graph);
    int maxIn = maxInDegreeVertex(graph);

    std::cout << "  Vertices:            " << withCommas(graph.vertexCount()) << "\n"
              << "  Edges:               " << withCommas(graph.edgeCount()) << "\n"
              << "  Self-loops:          " << withCommas(selfLoops) << "\n"
              << "  No outgoing links:   " << withCommas(noOut) << "\n"
              << "  No incoming links:   " << withCommas(noIn) << "\n"
              << "  Max out-degree:      " << withCommas(graph.outDegree(maxOut)) << " ("
              << describe(graph, maxOut) << ")\n"
              << "  Max in-degree:       " << withCommas(graph.inDegree(maxIn)) << " ("
              << describe(graph, maxIn) << ")\n"
              << "  Duplicate titles:    " << withCommas(static_cast<long long>(graph.duplicateTitleCount()))
              << "\n"
              << "  Estimated memory:    " << withCommas(static_cast<long long>(graph.memoryBytes() >> 20))
              << " MB\n";
}

void printRoundTrips(const Graph& graph, const std::vector<int>& ids) {
    if (!graph.hasTitles()) {
        return;
    }
    std::cout << "  Title round trips (id -> title -> id):\n";
    for (int id : ids) {
        const std::string& title = graph.titleForId(id);
        int back = graph.idForTitle(title);
        std::cout << "    " << describe(graph, id) << " -> " << back << (back == id ? "  ok" : "  MISMATCH")
                  << "\n";
    }
}

int usage() {
    std::cerr << "Usage: graph_stats <edges.txt> [titles.txt] [--max-vertices N] [--seed ID] "
                 "[--lookup TITLE]...\n";
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> positional;
    std::vector<std::string> lookups;
    int maxVertices = 0;
    int seed = -1;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--max-vertices" || arg == "--seed" || arg == "--lookup") && i + 1 >= argc) {
            return usage();
        }
        if (arg == "--max-vertices") {
            maxVertices = std::atoi(argv[++i]);
        } else if (arg == "--seed") {
            seed = std::atoi(argv[++i]);
        } else if (arg == "--lookup") {
            lookups.push_back(argv[++i]);
        } else if (!arg.empty() && arg[0] == '-') {
            return usage();
        } else {
            positional.push_back(arg);
        }
    }
    if (positional.empty() || positional.size() > 2) {
        return usage();
    }

    Graph graph;
    auto start = Clock::now();
    if (!graph.loadEdges(positional[0])) {
        std::cerr << "Failed to load edges: " << graph.lastError() << "\n";
        return 1;
    }
    std::cout << "Loaded edges in " << withCommas(elapsedMs(start)) << " ms\n";

    if (positional.size() == 2) {
        start = Clock::now();
        if (!graph.loadTitles(positional[1])) {
            std::cerr << "Failed to load titles: " << graph.lastError() << "\n";
            return 1;
        }
        std::cout << "Loaded titles in " << withCommas(elapsedMs(start)) << " ms\n";
    }

    std::cout << "\nFull graph\n";
    printStats(graph);
    if (graph.vertexCount() > 0) {
        printRoundTrips(graph, {0, maxOutDegreeVertex(graph), maxInDegreeVertex(graph), graph.vertexCount() - 1});
    }

    for (const std::string& title : lookups) {
        int id = graph.idForTitle(title);
        std::cout << "  Lookup \"" << title << "\": "
                  << (id == -1 ? "not found" : "id " + std::to_string(id) + ", out-degree " +
                                                   std::to_string(graph.outDegree(id)) + ", in-degree " +
                                                   std::to_string(graph.inDegree(id)))
                  << "\n";
    }

    if (maxVertices > 0) {
        if (seed < 0) {
            seed = maxOutDegreeVertex(graph);
        }
        if (seed >= graph.vertexCount()) {
            std::cerr << "Seed " << seed << " is not a vertex\n";
            return 1;
        }
        start = Clock::now();
        Graph sub = graph.inducedConnectedSubgraph(seed, maxVertices);
        std::cout << "\nConnected subgraph from " << describe(graph, seed) << " in "
                  << withCommas(elapsedMs(start)) << " ms\n";
        printStats(sub);
        if (sub.vertexCount() < maxVertices) {
            std::cout << "  Warning: the seed's component has only " << withCommas(sub.vertexCount())
                      << " vertices\n";
        }
        if (sub.vertexCount() > 0) {
            printRoundTrips(sub, {0, sub.vertexCount() - 1});
        }
    }
    return 0;
}
