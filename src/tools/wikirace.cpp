// Menu-driven WikiRace Pathfinder: path search, algorithm comparison, and batch benchmarks.

#include <cmath>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "graph/Algorithms.h"
#include "graph/Graph.h"
#include "tools/ProcessMemory.h"

namespace {

using Clock = std::chrono::steady_clock;
using AlgoFn = PathResult (*)(const Graph&, int, int);

struct TimedPath {
    PathResult result;
    long long time_ms = 0;
    long long memory_mb = -1;  // process working set after the run; -1 if unavailable
};

struct BatchTotals {
    long long runs = 0;
    long long reachable = 0;
    long long sum_nodes = 0;
    long long sum_time_ms = 0;
    long long sum_memory_mb = 0;
    long long memory_samples = 0;
    double sum_path_length = 0;
};

struct AlgorithmSpec {
    const char* name;
    AlgoFn run;
};

const std::vector<AlgorithmSpec> kAllAlgorithms = {
    {"BFS", bfs},
    {"Bidirectional", bidirectionalBfs},
    {"Dijkstra", dijkstra},
    {"Bellman-Ford", bellmanFord},
    {"DFS", dfs},
};

std::string withCommas(long long value);

std::string formatMemoryMb(long long memory_mb) {
    if (memory_mb < 0) {
        return "n/a";
    }
    return std::to_string(memory_mb);
}

TimedPath timeAlgorithm(const Graph& graph, int start_id, int target_id, AlgoFn fn) {
    auto t0 = Clock::now();
    PathResult result = fn(graph, start_id, target_id);
    auto t1 = Clock::now();
    TimedPath timed;
    timed.result = std::move(result);
    timed.time_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    const std::size_t bytes = processWorkingSetBytes();
    if (bytes > 0) {
        timed.memory_mb = static_cast<long long>((bytes + 512ULL * 1024ULL) / (1024ULL * 1024ULL));
    }
    return timed;
}

void recordBatchRow(BatchTotals& bucket, const TimedPath& timed) {
    ++bucket.runs;
    bucket.sum_nodes += timed.result.nodes_explored;
    bucket.sum_time_ms += timed.time_ms;
    if (std::isfinite(timed.result.path_length)) {
        ++bucket.reachable;
        bucket.sum_path_length += timed.result.path_length;
    }
    if (timed.memory_mb >= 0) {
        bucket.sum_memory_mb += timed.memory_mb;
        ++bucket.memory_samples;
    }
}

void printBatchSummary(const std::unordered_map<std::string, BatchTotals>& totals) {
    if (totals.empty()) {
        return;
    }
    std::cout << "\nBatch averages\n"
              << "Algorithm          Avg path   Avg nodes   Avg ms   Avg mem MB\n"
              << std::string(60, '-') << "\n";
    for (const AlgorithmSpec& spec : kAllAlgorithms) {
        auto it = totals.find(spec.name);
        if (it == totals.end() || it->second.runs == 0) {
            continue;
        }
        const BatchTotals& row = it->second;
        std::ostringstream avg_path;
        if (row.reachable > 0) {
            avg_path << std::fixed << std::setprecision(2)
                     << (row.sum_path_length / static_cast<double>(row.reachable));
        } else {
            avg_path << "N/A";
        }
        std::string avg_mem = "n/a";
        if (row.memory_samples > 0) {
            avg_mem = std::to_string(row.sum_memory_mb / row.memory_samples);
        }
        std::cout << std::left << std::setw(19) << spec.name << std::right << std::setw(10)
                  << avg_path.str() << std::setw(12) << withCommas(row.sum_nodes / row.runs)
                  << std::setw(9) << withCommas(row.sum_time_ms / row.runs) << std::setw(11)
                  << avg_mem << "\n";
    }
    std::cout << std::string(60, '-') << "\n";
}

void writeBatchSummaryCsv(const std::unordered_map<std::string, BatchTotals>& totals) {
    std::ofstream csv("benchmark_summary.csv");
    if (!csv) {
        std::cout << "Could not write benchmark_summary.csv\n";
        return;
    }
    csv << "Algorithm,Runs,Reachable,AvgPathLength,AvgNodesExplored,AvgTimeMS,AvgMemoryMB\n";
    for (const AlgorithmSpec& spec : kAllAlgorithms) {
        auto it = totals.find(spec.name);
        if (it == totals.end()) {
            continue;
        }
        const BatchTotals& row = it->second;
        csv << spec.name << ',' << row.runs << ',' << row.reachable << ',';
        if (row.reachable > 0) {
            csv << (row.sum_path_length / static_cast<double>(row.reachable));
        }
        csv << ',' << (row.runs > 0 ? static_cast<double>(row.sum_nodes) / row.runs : 0) << ','
            << (row.runs > 0 ? static_cast<double>(row.sum_time_ms) / row.runs : 0) << ',';
        if (row.memory_samples > 0) {
            csv << (static_cast<double>(row.sum_memory_mb) / row.memory_samples);
        }
        csv << '\n';
    }
    std::cout << "Wrote benchmark_summary.csv\n"
              << "Charts: python scripts/summarize_benchmark.py\n\n";
}

bool readTitles(int& start_id, int& target_id, const Graph& graph) {
    std::string start_title;
    std::string target_title;
    std::cout << "Start article: ";
    std::cin >> std::ws;
    std::getline(std::cin, start_title);
    std::cout << "Target article: ";
    std::getline(std::cin, target_title);

    start_id = graph.idForTitle(start_title);
    target_id = graph.idForTitle(target_title);
    if (start_id == -1 || target_id == -1) {
        std::cout << "Error: Article title not found in dataset.\n";
        return false;
    }
    return true;
}

std::string formatLength(double length) {
    if (!std::isfinite(length)) {
        return "N/A";
    }
    std::ostringstream out;
    double rounded = std::round(length * 100.0) / 100.0;
    if (std::fabs(rounded - std::round(rounded)) < 1e-9) {
        out << static_cast<long long>(std::llround(rounded));
    } else {
        out << std::fixed << std::setprecision(2) << rounded;
    }
    return out.str();
}

std::string articleLabel(const Graph& graph, int id) {
    if (id < 0 || id >= graph.vertexCount()) {
        return "(invalid article)";
    }
    const std::string& title = graph.titleForId(id);
    return title.empty() ? ("id " + std::to_string(id)) : title;
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

// Compact chain for short paths (proposal-style one line).
void printPathChain(const Graph& graph, const std::vector<int>& path) {
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (i > 0) {
            std::cout << " -> ";
        }
        std::cout << articleLabel(graph, path[i]);
    }
    std::cout << "\n";
}

// Numbered list; long paths show the first and last few hops only.
void printPathRoute(const Graph& graph, const std::vector<int>& path, int edge_limit = 8) {
    if (path.empty()) {
        return;
    }
    const int clicks = static_cast<int>(path.size()) - 1;
    std::cout << "  " << clicks << " click" << (clicks == 1 ? "" : "s") << ", "
              << path.size() << " article" << (path.size() == 1 ? "" : "s") << "\n";

    if (path.size() <= static_cast<std::size_t>(edge_limit) + 1) {
        for (std::size_t i = 0; i < path.size(); ++i) {
            std::cout << "    " << std::setw(3) << (i + 1) << ". " << articleLabel(graph, path[i])
                      << "\n";
        }
        return;
    }

    const std::size_t head = static_cast<std::size_t>(edge_limit);
    const std::size_t tail = static_cast<std::size_t>(edge_limit);
    for (std::size_t i = 0; i < head; ++i) {
        std::cout << "    " << std::setw(3) << (i + 1) << ". " << articleLabel(graph, path[i])
                  << "\n";
    }
    const int skipped = static_cast<int>(path.size()) - static_cast<int>(head + tail);
    if (skipped > 0) {
        std::cout << "    ... " << skipped << " more hop" << (skipped == 1 ? "" : "s")
                  << " ...\n";
    }
    for (std::size_t i = path.size() - tail; i < path.size(); ++i) {
        std::cout << "    " << std::setw(3) << (i + 1) << ". " << articleLabel(graph, path[i])
                  << "\n";
    }
}

void printSinglePath(const Graph& graph, int start_id, int target_id) {
    const bool weighted = graph.isWeighted();
    const char* algo = weighted ? "Dijkstra" : "BFS";
    std::cout << "Running " << algo << "..."
              << (weighted ? " (weighted)" : " (unweighted, fewest clicks)") << "\n";

    TimedPath timed = weighted ? timeAlgorithm(graph, start_id, target_id, dijkstra)
                               : timeAlgorithm(graph, start_id, target_id, bfs);

    std::cout << "\n  Path length:      " << formatLength(timed.result.path_length) << "\n"
              << "  Nodes explored:   " << withCommas(timed.result.nodes_explored) << "\n"
              << "  Time (ms):        " << withCommas(timed.time_ms) << "\n"
              << "  Memory (MB):      " << formatMemoryMb(timed.memory_mb)
              << "  (process working set after run)\n";

    if (timed.result.path.empty()) {
        std::cout << "\n  No path found.\n";
        return;
    }
    std::cout << "\n";
    printPathRoute(graph, timed.result.path);
    if (timed.result.path.size() <= 12) {
        std::cout << "  Chain: ";
        printPathChain(graph, timed.result.path);
    }
}

void printComparison(const Graph& graph, int start_id, int target_id) {
    std::cout << "\n";
    std::cout << "Query\n"
              << "  From: " << articleLabel(graph, start_id) << "\n"
              << "  To:   " << articleLabel(graph, target_id) << "\n"
              << "  Mode: " << (graph.isWeighted() ? "weighted" : "unweighted") << "\n"
              << "  Graph estimate:   ~" << withCommas(static_cast<long long>(graph.memoryBytes() >> 20))
              << " MB loaded structure\n";

    std::cout << "\nAlgorithm          Path length   Nodes explored    Time (ms)  Memory (MB)\n"
              << std::string(72, '-') << "\n";

    PathResult path_to_show;

    for (const AlgorithmSpec& spec : kAllAlgorithms) {
        TimedPath timed = timeAlgorithm(graph, start_id, target_id, spec.run);
        std::cout << std::left << std::setw(19) << spec.name << std::right << std::setw(14)
                  << formatLength(timed.result.path_length) << std::setw(18)
                  << withCommas(timed.result.nodes_explored) << std::setw(13)
                  << withCommas(timed.time_ms) << std::setw(13) << formatMemoryMb(timed.memory_mb)
                  << "\n";

        if (!graph.isWeighted() && spec.run == bfs && !timed.result.path.empty()) {
            path_to_show = timed.result;
        }
        if (graph.isWeighted() && spec.run == dijkstra && !timed.result.path.empty()) {
            path_to_show = timed.result;
        }
    }

    std::cout << std::string(72, '-') << "\n";
    if (!path_to_show.path.empty()) {
        std::cout << "\n"
                  << (graph.isWeighted() ? "Shortest path (Dijkstra):\n"
                                         : "Shortest path (BFS):\n");
        printPathRoute(graph, path_to_show.path);
        if (path_to_show.path.size() <= 12) {
            std::cout << "  Chain: ";
            printPathChain(graph, path_to_show.path);
        }
    } else {
        std::cout << "\nNo path found.\n";
    }
    std::cout << "\n";
}

void runBatchBenchmark(const Graph& graph, int num_pairs) {
    if (num_pairs <= 0) {
        std::cout << "Need at least one pair.\n";
        return;
    }

    std::cout << "Benchmarking " << num_pairs << " random pair(s) with every algorithm.\n"
              << "Bellman-Ford on the full graph can take several seconds per pair.\n\n";

    std::ofstream csv("benchmark_results.csv");
    if (!csv) {
        std::cout << "Could not open benchmark_results.csv for writing.\n";
        return;
    }
    csv << "Algorithm,Weighted,StartID,TargetID,PathLength,NodesExplored,TimeMS,MemoryMB\n";

    std::unordered_map<std::string, BatchTotals> totals;
    for (const AlgorithmSpec& spec : kAllAlgorithms) {
        totals.emplace(spec.name, BatchTotals{});
    }

    const int vertices = graph.vertexCount();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> pick(0, vertices - 1);

    for (int pair = 0; pair < num_pairs; ++pair) {
        int start_id = pick(gen);
        int target_id = pick(gen);
        for (const AlgorithmSpec& spec : kAllAlgorithms) {
            TimedPath timed = timeAlgorithm(graph, start_id, target_id, spec.run);
            recordBatchRow(totals[spec.name], timed);
            csv << spec.name << ',' << (graph.isWeighted() ? "true" : "false") << ','
                << start_id << ',' << target_id << ','
                << (std::isfinite(timed.result.path_length) ? std::to_string(timed.result.path_length)
                                                            : "inf")
                << ',' << timed.result.nodes_explored << ',' << timed.time_ms << ','
                << (timed.memory_mb >= 0 ? std::to_string(timed.memory_mb) : "") << '\n';
        }
        if ((pair + 1) % 10 == 0 || pair + 1 == num_pairs) {
            std::cout << "  Finished " << (pair + 1) << " / " << num_pairs << " pairs\r"
                      << std::flush;
        }
    }
    std::cout << "\nBenchmark complete. Results saved to benchmark_results.csv\n";
    printBatchSummary(totals);
    writeBatchSummaryCsv(totals);
}

int readPairCount() {
    std::cout << "Number of random pairs (default 100): ";
    std::string line;
    std::getline(std::cin >> std::ws, line);
    if (line.empty()) {
        return 100;
    }
    try {
        int value = std::stoi(line);
        return value > 0 ? value : 100;
    } catch (...) {
        return 100;
    }
}

void runMenu(Graph& graph) {
    while (true) {
        std::cout << "\n=========== WikiRace Pathfinder ===========\n"
                  << "1. Find a path between two articles\n"
                  << "2. Compare all algorithms on one query\n"
                  << "3. Run batch benchmark (N random pairs)\n"
                  << "4. Toggle edge weights (currently: " << (graph.isWeighted() ? "ON" : "OFF")
                  << ")\n"
                  << "5. Exit\n"
                  << "===========================================\n"
                  << "Choice: ";

        int choice = 0;
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (choice == 1 || choice == 2) {
            int start_id = 0;
            int target_id = 0;
            if (!readTitles(start_id, target_id, graph)) {
                continue;
            }
            if (choice == 1) {
                printSinglePath(graph, start_id, target_id);
            } else {
                printComparison(graph, start_id, target_id);
            }
        } else if (choice == 3) {
            runBatchBenchmark(graph, readPairCount());
        } else if (choice == 4) {
            graph.setWeighted(!graph.isWeighted());
        } else if (choice == 5) {
            break;
        } else {
            std::cout << "Invalid choice. Please try again.\n";
        }
    }
}

}  // namespace

int main() {
    Graph graph;
    std::cout << "Loading WikiRace graph...\n";

    if (!graph.loadEdges("data/wiki-topcats.txt") ||
        !graph.loadTitles("data/wiki-topcats-page-names.txt")) {
        std::cerr << "Failed to load graph: " << graph.lastError() << "\n";
        return 1;
    }

    std::cout << "Graph loaded! Vertices: " << graph.vertexCount() << "\n";
    runMenu(graph);
    return 0;
}
