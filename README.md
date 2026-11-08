# Hyperlink-Hustlers: WikiRace Pathfinder

WikiRace is a game where you start on one Wikipedia article and try to reach a target article by clicking only the links on each page. WikiRace Pathfinder models Wikipedia as a directed graph (articles are vertices, hyperlinks are edges) and compares graph search algorithms on the same task: BFS, bidirectional BFS, DFS, Dijkstra, and Bellman-Ford.

Everything is written from scratch in C++. The project uses no graph, map, or priority-queue libraries.

**Team:** Ryan Xi (data and graph), Vinh Nguyen (search algorithms and Dijkstra), Daniel Gao (Bellman-Ford, menu, and benchmarks).

## Requirements

- A C++17 compiler. This was tested with MSYS2 MinGW-w64 `g++` 15.2 on Windows.
- CMake 3.16 or newer.
- About 600 MB of free RAM to hold the full graph.

## Getting the data

The project uses the [wiki-topcats](https://snap.stanford.edu/data/wiki-topcats.html) graph from the Stanford SNAP collection. To download and decompress it into `data/`, run:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/download_data.ps1
```

| File | Size | Contents |
|---|---|---|
| `data/wiki-topcats.txt` | 422 MB | One link per line: `source_id target_id` |
| `data/wiki-topcats-page-names.txt` | 47 MB | One article per line: `article_id title` |

`data/` is gitignored because the edge list is far over GitHub's 100 MB file limit. Each teammate runs the script once. It skips files that already exist.

## Building and testing

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
```

The tests run on a small hand-written graph in `tests/fixtures/`, so they don't need the downloaded data.

## Running

`wikirace` is the main program: a menu to find paths, compare algorithms, and run batch benchmarks. Run it from the repo root (it loads `data/wiki-topcats.txt`):

```powershell
.\build\wikirace.exe
```

- **1** finds a path with BFS (unweighted, fewest clicks) or Dijkstra (weighted).
- **2** runs BFS, bidirectional BFS, Dijkstra, Bellman-Ford, and DFS on the same query and prints a timing table plus the shortest path.
- **3** benchmarks every algorithm on `N` random pairs (default 100), writes `benchmark_results.csv` and `benchmark_summary.csv`, and prints average rows in the terminal. Memory is the process working set (MB) after each run on Windows.
- **4** toggles weighted mode.

After a batch run, generate charts for the report:

```powershell
pip install matplotlib
python scripts/summarize_benchmark.py
```

That reads `benchmark_results.csv`, refreshes `benchmark_summary.csv`, and saves bar charts under `output/`.

`graph_stats` loads the graph and prints its size, load time, memory, degree extremes, and weights:

```powershell
.\build\graph_stats.exe data\wiki-topcats.txt data\wiki-topcats-page-names.txt
```

| Flag | What it does |
|---|---|
| `--lookup TITLE` | Prints an article's id, out-degree, in-degree, and weight. Titles are case-sensitive. The flag can be repeated. |
| `--max-vertices N` | Also builds and reports a connected subgraph of `N` vertices (the fallback below). |
| `--seed ID` | Sets the starting vertex for the subgraph. The default is the vertex with the most outgoing links. |

Sample output (abridged):

```
Loaded edges in 1,396 ms
Loaded titles in 443 ms

Full graph
  Vertices:            1,791,489
  Edges:               28,511,807
  Max in-degree:       238,040 (id 279122 "United States")
  Estimated memory:    519 MB
  Vertex weights (1 + log2(maxInDegree / inDegree)):
    Min / mean / max:  1.00 / 16.82 / 18.86
  Lookup "Pizza": id 1651404, out-degree 77, in-degree 315, weight 10.56
```

## Data format

| Field | Type | Source file | Meaning |
|---|---|---|---|
| `source_id` | int | `wiki-topcats.txt` | Article the link is on |
| `target_id` | int | `wiki-topcats.txt` | Article the link points to |
| `article_id` | int | `wiki-topcats-page-names.txt` | Unique id per article, from 0 to 1,791,488 |
| `title` | string | `wiki-topcats-page-names.txt` | Article title; can contain spaces |

The edge file separates ids with a space, not a comma. The loader accepts a space, a tab, or a comma, and skips lines that start with `#` or `%`. In the titles file, everything after the first space is the title.

The full graph has 1,791,489 articles and 28,511,807 links. Every article has at least one incoming and one outgoing link. Some quirks to know about:

- **Self-loops:** 3,666 links point from an article to itself. They are kept as they appear in the data, so searches should skip them.
- **Duplicate titles:** 892 titles appear on more than one article. `idForTitle` returns the lowest id.

## Weighted mode

In weighted mode, a link costs more when its destination is obscure. This models how human racers prefer well-known hub pages. Obscurity is measured by in-degree, the number of articles that link to a page:

```
weight(v) = 1 + log2(maxInDegree / inDegree(v))
```

Here `maxInDegree` is 238,040 (United States). An in-degree of 0, which can occur in subgraphs and tests, is treated as 1.

| Article | In-degree | Weight |
|---|---|---|
| United States | 238,040 | 1.00 |
| Florida | 12,847 | 5.21 |
| University of Florida | 2,507 | 7.57 |
| Pizza | 315 | 10.56 |
| Any article with one in-link | 1 | 18.86 |

Every weight is at least 1, which keeps Dijkstra correct. The log scale stops one obscure page from outweighing a whole path. Because the cost depends only on the destination, weights are stored once per vertex (about 14 MB) instead of once per edge.

Unweighted mode is the default, and it costs every link 1. In that mode Dijkstra and Bellman-Ford should return the same path lengths as BFS, which makes a quick correctness check.

## Using the Graph API

All algorithms code against `include/graph/Graph.h`:

```cpp
#include "graph/Graph.h"

Graph graph;
if (!graph.loadEdges("data/wiki-topcats.txt") ||
    !graph.loadTitles("data/wiki-topcats-page-names.txt")) {
    std::cerr << graph.lastError() << "\n";
    return 1;
}

int start = graph.idForTitle("University of Florida");  // -1 if not found
int target = graph.idForTitle("Pizza");

for (int next : graph.outNeighbors(start)) { /* forward links */ }
for (int prev : graph.inNeighbors(target)) { /* reverse links, for bidirectional BFS */ }

graph.setWeighted(true);                          // toggle from the menu
double cost = graph.edgeWeight(start, target);    // 1.0 when unweighted
std::string name = graph.titleForId(target);
```

## Connected subgraph fallback

The full graph fits in about 520 MB, so the fallback is optional. `inducedConnectedSubgraph(seed, n)` grows a weakly connected subgraph of `n` vertices from `seed` by following links in both directions. It keeps every link between the collected vertices and renumbers them from 0, with the seed as 0. Weights are copied from the full graph rather than recomputed, so an article stays as obscure as it is on real Wikipedia.

With `n = 100,000`, it builds in under 200 ms and keeps 2,715,886 links.

## Project layout

```
include/graph/       Graph.h (graph API), TitleMap.h (title-to-id hash map)
src/graph/           Loader, adjacency lists, weights, subgraph, and hash map
src/tools/           wikirace menu program and graph_stats loader tool
tests/               graph_tests and small fixture files
scripts/             download_data.ps1
```
