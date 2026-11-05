#include <iostream>
#include <string>
#include <limits>
#include "graph/Graph.h"
#include "graph/Algorithms.h"

using namespace std;

void runMenu(Graph& graph) {
    int choice = 0;
    
    while (true) {
        cout << "=========== WikiRace Pathfinder ===========" << endl;
        cout << "1. Find a path between two articles" << endl;
        cout << "2. Compare all algorithms on one query" << endl;
        cout << "3. Run batch benchmark (N random pairs)" << endl;
        cout << "4. Toggle edge weights (currently: " << (graph.isWeighted() ? "ON" : "OFF") << ")" << endl;
        cout << "5. Exit" << endl;
        cout << "===========================================" << endl;
        cout << "Choice: ";
        
        if (!(cin >> choice)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            continue;
        }

        if (choice == 1 || choice == 2) {
            string start_title, target_title;
            cout << "Start article: ";
            cin >> ws; // clear leading whitespace
            getline(cin, start_title);
            cout << "Target article: ";
            getline(cin, target_title);
            
            int start_id = graph.idForTitle(start_title);
            int target_id = graph.idForTitle(target_title);
            
            if (start_id == -1 || target_id == -1) {
                cout << "Error: Article title not found in dataset." << endl;
                continue;
            }

            if (choice == 1) {
                cout << "Running Bellman-Ford..." << endl;
                PathResult res = bellmanFord(graph, start_id, target_id);
                cout << "Path length: " << res.path_length << " | Nodes explored: " << res.nodes_explored << endl;
            } else {
                cout << "Running comparison..." << endl;
                PathResult res = bellmanFord(graph, start_id, target_id);
                cout << "Bellman-Ford length: " << res.path_length << " | Nodes explored: " << res.nodes_explored << endl;
            }
            
        } else if (choice == 3) {
            cout << "Running batch benchmark..." << endl;
            
        } else if (choice == 4) {
            graph.setWeighted(!graph.isWeighted()); 
            
        } else if (choice == 5) {
            break;
            
        } else {
            cout << "Invalid choice. Please try again." << endl;
        }
    }
}

int main() {
    Graph graph;
    cout << "Loading WikiRace graph..." << endl;
    
    if (!graph.loadEdges("data/wiki-topcats.txt") || !graph.loadTitles("data/wiki-topcats-page-names.txt")) {
        cerr << "Failed to load graph: " << graph.lastError() << endl;
        return 1;
    }
    
    cout << "Graph loaded! Vertices: " << graph.vertexCount() << endl;
    
    runMenu(graph);
    
    return 0;
}
