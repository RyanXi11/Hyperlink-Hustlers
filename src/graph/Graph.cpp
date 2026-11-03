#include "graph/Graph.h"

#include <climits>
#include <cstdio>
#include <cstring>
#include <utility>

namespace {

constexpr std::size_t kChunkSize = std::size_t{1} << 24;

// Calls onLine(begin, end, lineNumber) for every line in the file, without the trailing
// "\n" or "\r\n". Stops early if onLine returns false.
template <typename OnLine>
bool forEachLine(const std::string& path, OnLine onLine, std::string& error) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        error = "cannot open " + path;
        return false;
    }

    std::vector<char> buffer(kChunkSize);
    std::size_t carry = 0;
    long long lineNumber = 0;
    bool ok = true;

    auto emit = [&](const char* begin, const char* end) {
        if (end > begin && end[-1] == '\r') {
            --end;
        }
        return onLine(begin, end, ++lineNumber);
    };

    while (ok) {
        std::size_t room = buffer.size() - carry;
        std::size_t got = std::fread(buffer.data() + carry, 1, room, file);
        if (std::ferror(file)) {
            error = "read error in " + path;
            ok = false;
            break;
        }
        bool atEof = got < room;

        const char* p = buffer.data();
        const char* end = p + carry + got;
        while (ok) {
            const char* newline = static_cast<const char*>(std::memchr(p, '\n', end - p));
            if (newline == nullptr) {
                break;
            }
            ok = emit(p, newline);
            p = newline + 1;
        }
        if (!ok) {
            break;
        }

        std::size_t rest = static_cast<std::size_t>(end - p);
        if (atEof) {
            if (rest > 0) {
                ok = emit(p, end);
            }
            break;
        }
        std::memmove(buffer.data(), p, rest);
        carry = rest;
        if (carry == buffer.size()) {
            buffer.resize(buffer.size() * 2);
        }
    }

    std::fclose(file);
    return ok;
}

bool isBlank(char c) { return c == ' ' || c == '\t'; }
bool isDigit(char c) { return c >= '0' && c <= '9'; }

// Advances p past leading blanks. Returns true if the rest of the line is empty or a comment.
bool skipToContent(const char*& p, const char* end) {
    while (p < end && isBlank(*p)) {
        ++p;
    }
    return p == end || *p == '#' || *p == '%';
}

bool parseId(const char*& p, const char* end, int& out) {
    if (p == end || !isDigit(*p)) {
        return false;
    }
    long long value = 0;
    while (p < end && isDigit(*p)) {
        value = value * 10 + (*p - '0');
        if (value > INT_MAX) {
            return false;
        }
        ++p;
    }
    out = static_cast<int>(value);
    return true;
}

std::string lineError(const std::string& path, long long lineNumber, const char* what) {
    return path + ":" + std::to_string(lineNumber) + ": " + what;
}

}  // namespace

bool Graph::loadEdges(const std::string& path) {
    std::vector<std::pair<int, int>> edges;
    int maxId = -1;
    std::string error;

    bool ok = forEachLine(
        path,
        [&](const char* p, const char* end, long long lineNumber) {
            if (skipToContent(p, end)) {
                return true;
            }
            int source = 0;
            int target = 0;
            if (!parseId(p, end, source)) {
                error = lineError(path, lineNumber, "expected a source id");
                return false;
            }
            const char* afterSource = p;
            while (p < end && (isBlank(*p) || *p == ',')) {
                ++p;
            }
            if (p == afterSource || !parseId(p, end, target)) {
                error = lineError(path, lineNumber, "expected a target id");
                return false;
            }
            while (p < end && isBlank(*p)) {
                ++p;
            }
            if (p != end) {
                error = lineError(path, lineNumber, "unexpected text after target id");
                return false;
            }
            edges.emplace_back(source, target);
            if (source > maxId) maxId = source;
            if (target > maxId) maxId = target;
            return true;
        },
        error);

    if (!ok) {
        lastError_ = error;
        return false;
    }

    int n = maxId + 1;
    if (static_cast<int>(titles_.size()) > n) {
        n = static_cast<int>(titles_.size());
    }

    std::vector<int> outCount(n, 0);
    std::vector<int> inCount(n, 0);
    for (const auto& [source, target] : edges) {
        ++outCount[source];
        ++inCount[target];
    }

    std::vector<std::vector<int>> out(n);
    std::vector<std::vector<int>> in(n);
    for (int v = 0; v < n; ++v) {
        out[v].reserve(outCount[v]);
        in[v].reserve(inCount[v]);
    }
    for (const auto& [source, target] : edges) {
        out[source].push_back(target);
        in[target].push_back(source);
    }

    out_ = std::move(out);
    in_ = std::move(in);
    edgeCount_ = static_cast<long long>(edges.size());
    lastError_.clear();
    return true;
}

bool Graph::loadTitles(const std::string& path) {
    std::vector<std::string> titles;
    std::string error;

    bool ok = forEachLine(
        path,
        [&](const char* p, const char* end, long long lineNumber) {
            if (skipToContent(p, end)) {
                return true;
            }
            int id = 0;
            if (!parseId(p, end, id)) {
                error = lineError(path, lineNumber, "expected an article id");
                return false;
            }
            if (p < end) {
                if (!isBlank(*p)) {
                    error = lineError(path, lineNumber, "expected a space after the article id");
                    return false;
                }
                ++p;
            }
            if (id >= static_cast<int>(titles.size())) {
                titles.resize(static_cast<std::size_t>(id) + 1);
            }
            titles[id].assign(p, end);
            return true;
        },
        error);

    if (!ok) {
        lastError_ = error;
        return false;
    }

    titles_ = std::move(titles);
    ensureVertexCount(static_cast<int>(titles_.size()));
    indexTitles();
    lastError_.clear();
    return true;
}

int Graph::idForTitle(const std::string& title) const {
    return titleToId_.find(title);
}

const std::string& Graph::titleForId(int id) const {
    static const std::string kEmpty;
    if (id < 0 || id >= static_cast<int>(titles_.size())) {
        return kEmpty;
    }
    return titles_[id];
}

Graph Graph::inducedConnectedSubgraph(int seed, int minVertices) const {
    Graph sub;
    int n = vertexCount();
    if (seed < 0 || seed >= n || minVertices <= 0) {
        return sub;
    }
    std::size_t target = static_cast<std::size_t>(minVertices < n ? minVertices : n);

    std::vector<int> newId(n, -1);
    std::vector<int> order;
    order.reserve(target);
    newId[seed] = 0;
    order.push_back(seed);

    for (std::size_t head = 0; head < order.size() && order.size() < target; ++head) {
        int v = order[head];
        for (const std::vector<int>* neighbors : {&out_[v], &in_[v]}) {
            for (int u : *neighbors) {
                if (newId[u] == -1) {
                    newId[u] = static_cast<int>(order.size());
                    order.push_back(u);
                    if (order.size() == target) {
                        break;
                    }
                }
            }
            if (order.size() == target) {
                break;
            }
        }
    }

    int m = static_cast<int>(order.size());
    sub.out_.resize(m);
    sub.in_.resize(m);
    for (int i = 0; i < m; ++i) {
        int old = order[i];
        for (int u : out_[old]) {
            if (newId[u] != -1) {
                sub.out_[i].push_back(newId[u]);
            }
        }
        for (int u : in_[old]) {
            if (newId[u] != -1) {
                sub.in_[i].push_back(newId[u]);
            }
        }
        sub.edgeCount_ += static_cast<long long>(sub.out_[i].size());
    }

    if (hasTitles()) {
        sub.titles_.resize(m);
        for (int i = 0; i < m; ++i) {
            sub.titles_[i] = titleForId(order[i]);
        }
        sub.indexTitles();
    }
    return sub;
}

std::size_t Graph::memoryBytes() const {
    std::size_t bytes = (out_.capacity() + in_.capacity()) * sizeof(std::vector<int>);
    for (std::size_t v = 0; v < out_.size(); ++v) {
        bytes += (out_[v].capacity() + in_[v].capacity()) * sizeof(int);
    }
    bytes += titles_.capacity() * sizeof(std::string);
    for (const std::string& title : titles_) {
        if (title.capacity() > std::string().capacity()) {
            bytes += title.capacity() + 1;
        }
    }
    return bytes + titleToId_.memoryBytes();
}

void Graph::ensureVertexCount(int count) {
    if (count > vertexCount()) {
        out_.resize(count);
        in_.resize(count);
    }
}

void Graph::indexTitles() {
    titleToId_.clear();
    duplicateTitles_ = 0;
    std::size_t named = 0;
    for (const std::string& title : titles_) {
        if (!title.empty()) {
            ++named;
        }
    }
    titleToId_.reserve(named);
    for (int id = 0; id < static_cast<int>(titles_.size()); ++id) {
        if (!titles_[id].empty() && !titleToId_.insert(titles_[id], id)) {
            ++duplicateTitles_;
        }
    }
}
