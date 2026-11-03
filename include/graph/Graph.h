#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "graph/TitleMap.h"

// Directed Wikipedia link graph: articles are vertices, hyperlinks are edges.
// Vertex ids are contiguous in [0, vertexCount()).
class Graph {
public:
    // Edge list: one "source target" pair per line, separated by spaces, tabs, or a comma.
    // Lines starting with '#' or '%' are skipped. Replaces any previously loaded edges.
    bool loadEdges(const std::string& path);

    // Title list: one "id title" pair per line; the title is the rest of the line.
    bool loadTitles(const std::string& path);

    int vertexCount() const { return static_cast<int>(out_.size()); }
    long long edgeCount() const { return edgeCount_; }

    const std::vector<int>& outNeighbors(int id) const { return out_[id]; }
    const std::vector<int>& inNeighbors(int id) const { return in_[id]; }
    int outDegree(int id) const { return static_cast<int>(out_[id].size()); }
    int inDegree(int id) const { return static_cast<int>(in_[id].size()); }

    // Weighted mode prices each link by how obscure its destination is:
    // vertexWeight(v) = 1 + log2(maxInDegree / inDegree(v)), with inDegree 0 treated as 1.
    // Unweighted mode (the default) prices every link at 1.
    void setWeighted(bool weighted) { weighted_ = weighted; }
    bool isWeighted() const { return weighted_; }
    double edgeWeight(int /*from*/, int to) const { return weighted_ ? vertexWeight_[to] : 1.0; }
    double vertexWeight(int id) const { return vertexWeight_[id]; }

    // Returns -1 if no article has this exact title.
    int idForTitle(const std::string& title) const;
    // Returns an empty string for ids without a title.
    const std::string& titleForId(int id) const;
    bool hasTitles() const { return !titles_.empty(); }
    std::size_t duplicateTitleCount() const { return duplicateTitles_; }

    // Weakly connected subgraph of up to minVertices vertices grown from seed by following
    // links in both directions. Ids are remapped to [0, n) in discovery order, so seed becomes 0.
    // Returns fewer vertices only if seed's component is smaller than minVertices.
    // Vertex weights and the weighted flag are copied from this graph, not recomputed.
    Graph inducedConnectedSubgraph(int seed, int minVertices) const;

    std::size_t memoryBytes() const;
    const std::string& lastError() const { return lastError_; }

private:
    void ensureVertexCount(int count);
    void indexTitles();
    void computeWeights();

    std::vector<std::vector<int>> out_;
    std::vector<std::vector<int>> in_;
    long long edgeCount_ = 0;

    std::vector<double> vertexWeight_;
    bool weighted_ = false;

    std::vector<std::string> titles_;
    TitleMap titleToId_;
    std::size_t duplicateTitles_ = 0;

    std::string lastError_;
};
