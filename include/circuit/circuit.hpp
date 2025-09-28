#pragma once

#include <cctype>
#include <pqxx/pqxx>
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
		string cable;
	};

	struct UniGraph {
		std::unordered_map<string, std::vector<UniEdge>> adj; // parent -> edges (parent -> child)
		std::unordered_map<string, int> indeg;				  // node -> indegree count
		std::unordered_map<string, int> level;				  // BFS level from roots
		std::vector<string> roots;							  // indegree==0 nodes
	};

	Circuit();

	Circuit(Circuit &&) = default;
	Circuit(const Circuit &) = delete;
	Circuit &operator=(Circuit &&) = default;
	Circuit &operator=(const Circuit &) = delete;
	~Circuit() = default;

	// Read-only accessors
	const UniGraph &unifilar() const { return uni; }

  private:
	UniGraph uni;

	string conn_str = "postgresql://postgres:postgres@127.0.0.1:5432/appdb";
	std::unique_ptr<pqxx::connection> conn;

	void buildUnifilar();
};
