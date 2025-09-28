#include "circuit.hpp"
#include <algorithm>
#include <iostream>
#include <queue>
#include <regex>
#include <unordered_set>

Circuit::Circuit() {
	conn = make_unique<pqxx::connection>(conn_str);

	if (!conn->is_open()) {
		std::cout << "Failed to open DB connection\n";
	}
	std::cout << "Connected to " << conn->dbname() << "\n";

	// Quick smoke-read (optional)
	{
		pqxx::work tx{*conn};
		pqxx::result r = tx.exec("SELECT id_gm, cable, origine, destination, cond_et_calibre, longueur_retenue "
								 "FROM public.cables ORDER BY id_gm LIMIT 5");
		for (auto const &row : r) {
			int id = row["id_gm"].as<int>();
			std::string cable = row["cable"].as<std::string>("");
			std::string origine = row["origine"].as<std::string>("");
			std::string destination = row["destination"].as<std::string>("");
			std::string cond = row["cond_et_calibre"].as<std::string>("");
			double len = row["longueur_retenue"].as<double>(0.0);
			std::cout << id << " | " << cable << " | " << origine << " -> " << destination << " | " << cond << " | " << len << "\n";
		}
		// no commit needed for read
	}

	buildUnifilar();
}

static inline std::string trim_copy(std::string s) {
	auto notspace = [](int ch) { return !std::isspace(ch); };
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
	s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
	return s;
}

static inline bool is_route_tag(const std::string &t) {
	// common trailing tags like B01, R01 (and variants B1, R1 just in case)
	static const std::regex route_re(R"(^[BR]\d{1,3}$)", std::regex::icase);
	return std::regex_match(t, route_re);
}

static inline bool is_placeholder(const std::string &t) { return t == "*" || t.empty(); }

static std::vector<std::string> split_nodes(const std::string &s) {
	std::vector<std::string> out;
	std::string cur;
	for (char c : s) {
		if (c == '/') {
			if (!cur.empty()) {
				std::string tok = trim_copy(cur);
				if (!is_placeholder(tok) && !is_route_tag(tok))
					out.push_back(tok);
				cur.clear();
			} else {
				// skip empty
			}
		} else {
			cur.push_back(c);
		}
	}
	if (!cur.empty()) {
		std::string tok = trim_copy(cur);
		if (!is_placeholder(tok) && !is_route_tag(tok))
			out.push_back(tok);
	}
	return out;
}

static size_t common_prefix_len(const std::vector<std::string> &a, const std::vector<std::string> &b) {
	size_t k = 0;
	while (k < a.size() && k < b.size() && a[k] == b[k])
		++k;
	return k;
}

void Circuit::buildUnifilar() {
	uni = {};
	std::unordered_set<string> nodes;

	pqxx::work tx{*conn};
	pqxx::result r = tx.exec("SELECT id_gm, cable, origine, destination, cond_et_calibre, longueur_retenue "
							 "FROM public.cables "
							 "WHERE origine IS NOT NULL AND destination IS NOT NULL");

	for (auto const &row : r) {
		int id = row["id_gm"].as<int>();
		string cable = row["cable"].as<string>("");
		string orig_raw = trim_copy(row["origine"].as<string>(""));
		string dest_raw = trim_copy(row["destination"].as<string>(""));
		string cond = row["cond_et_calibre"].as<string>("");
		float length = static_cast<float>(row["longueur_retenue"].as<double>(0.0));

		auto O = split_nodes(orig_raw);
		auto D = split_nodes(dest_raw);
		if (O.empty() || D.empty())
			continue;

		// Longest common prefix (usually the feeder)
		size_t k = common_prefix_len(O, D);

		// Start from the last explicit origin node
		string start = O.back();
		nodes.insert(start);

		// Destination tail after the common prefix
		std::vector<string> tail;
		if (k < D.size()) {
			tail.assign(D.begin() + static_cast<long>(k), D.end());
		} else {
			// If destination has no tail (identical to origin path), fall back to last node
			tail.push_back(D.back());
		}

		// Build 1..N edges along the path: start -> tail[0] -> tail[1] ...
		string u = start;
		for (const auto &v : tail) {
			if (v == u)
				continue; // skip self-loop if identical
			UniEdge e;
			e.id = id;
			e.parent = u;
			e.child = v;
			e.length = length; // if you have per-span lengths, split here; otherwise keep total
			e.cond = cond;
			e.cable = cable;

			uni.adj[u].push_back(e);
			(void)uni.indeg[u]; // ensure present
			++uni.indeg[v];

			nodes.insert(v);
			u = v;
		}
	}

	// Ensure all nodes appear in indeg map
	for (auto const &n : nodes)
		(void)uni.indeg[n];

	// Roots
	for (auto const &kv : uni.indeg) {
		if (kv.second == 0)
			uni.roots.push_back(kv.first);
	}

	// Multi-source BFS levels
	std::queue<string> q;
	for (auto const &root : uni.roots) {
		uni.level[root] = 0;
		q.push(root);
	}
	while (!q.empty()) {
		string u = q.front();
		q.pop();
		int next = uni.level[u] + 1;
		auto it = uni.adj.find(u);
		if (it == uni.adj.end())
			continue;
		for (auto const &e : it->second) {
			const string &v = e.child;
			auto lv = uni.level.find(v);
			if (lv == uni.level.end() || next < lv->second) {
				uni.level[v] = next;
				q.push(v);
			}
		}
	}
}
