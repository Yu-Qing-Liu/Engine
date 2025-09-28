#pragma once

#include <cctype>
#include <string>
#include <unordered_map>
#include <vector>

using string = std::string;

class Circuit {
  public:
	struct UniEdge {
		int id{};
		string parent;
		string child;
		float length{};
		string cond;
	};

	struct UniGraph {
		std::unordered_map<string, std::vector<UniEdge>> adj; // parent -> edges (parent -> child)
		std::unordered_map<string, int> indeg;				  // node -> indegree count
		std::unordered_map<string, int> level;				  // BFS level from roots
		std::vector<string> roots;							  // indegree==0 nodes
	};

  public:
	Circuit();

	Circuit(Circuit &&) = default;
	Circuit(const Circuit &) = default;
	Circuit &operator=(Circuit &&) = default;
	Circuit &operator=(const Circuit &) = default;
	~Circuit() = default;

	// Read-only accessors
	const UniGraph &unifilar() const { return uni; }

  private:
	UniGraph uni;
	void buildUnifilar();
};
