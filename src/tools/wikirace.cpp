#include <iostream>
#include <string>
#include <limits>
#include <fstream>
#include <chrono>
#include <random>
#include <vector>
#include "graph/Graph.h"
#include "graph/Algorithms.h"

using namespace std;
using namespace std::chrono;

void runBatchBenchmark(Graph& graph, int num_pairs) {
    cout << "Starting batch benchmark for " << num_pairs << " random pairs..." << endl;
    
    // Opens CSV file for writing
    ofstream csvFile("benchmark_results.csv");
    csvFile << "Algorithm,Weighted,StartID,TargetID,PathLength,NodesExplored,TimeMS\n";
    
    int num_vertices = graph.vertexCount();
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> distrib(0, num_vertices - 1);

    for (int i = 0; i < num_pairs; ++i) {
        int start_id = distrib(gen);
        int target_id = distrib(gen);
        
        // Time Bellman-Ford
        auto start_time = high_resolution_clock::now();
        PathResult res = bellmanFord(graph, start_id, target_id); 
        auto end_time = high_resolution_clock::now();
        
        auto duration_ms = duration_cast<milliseconds>(end_time - start_time).count();
        
        // Writes result to CSV
        csvFile << "Bellman-Ford," 
                << (graph.isWeighted() ? "True" : "False") << ","
                << start_id << "," 
                << target_id << "," 
                << res.path_length << "," 
                << res.nodes_explored << "," 
                << duration_ms << "\n";
    }
    
    csvFile.close();
    cout << "Benchmark complete. Results saved to benchmark_results.csv" << endl;
}

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
            cin >> ws; 
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
            // This now calls the benchmark function for 100 pairs
            runBatchBenchmark(graph, 100);
            
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
