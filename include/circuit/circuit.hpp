#pragma once

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Circuit {
  public:
	using string = std::string;

	struct Node {
		string id;
	};

	struct Edge {
		int id{};
		Node u; // origin
		Node v; // destination
		string cableName;
		string conditionAndCaliber;
		float length{};
	};

	// Unifilar/derived graph structures
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
	explicit Circuit(const string &csvPath) { readFromFile(csvPath); }

	Circuit(Circuit &&) = default;
	Circuit(const Circuit &) = default;
	Circuit &operator=(Circuit &&) = default;
	Circuit &operator=(const Circuit &) = default;
	~Circuit() = default;

	// Read-only accessors
	const std::unordered_map<string, Node> & nodes() const { return nodesById_; }
	const std::vector<Edge> &edges() const { return edges_; }
	const UniGraph &unifilar() const { return uni_; }

  private:
	// --- storage ---
	std::unordered_map<string, Node> nodesById_;
	std::vector<Edge> edges_;
	UniGraph uni_; // derived unifilar graph

	// --- I/O ---
	void readFromFile(const string &csvPath);
	void buildUnifilar();

	// --- CSV helpers (defined in .cpp) ---
	static void trimInPlace(string &s);
	static string lower(string s);
	static int findHeader(const std::vector<string> &headers, std::initializer_list<string> candidates);
	static std::vector<string> parseCsvLine(const string &line);

	// --- node management ---
	Node &getOrCreateNode(const string &id);

	// --- cable path parsing helpers (defined in .cpp) ---
	static bool isAllDigits(const string &s);
	static bool isTagToken(const string &t);		 // B01, R01, …
	static string stripTrailingTag(const string &s); // drop nested “/...”
	static bool isTTC(const string &tok);
	static string extractTTC(const string &t); // pull TTC-### out of embedded token
	static string normalizeToken(string raw);  // trim, strip tags, extract TTC if embedded
	static void splitPath(const string &s, std::vector<string> &out);
	static void tokenizeCable(const string &cableName, std::vector<string> &out);
};
