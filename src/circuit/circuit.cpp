#include "circuit.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <queue>
#include <unordered_set>

static std::vector<std::string> split(const std::string &s, char delim, bool keepEmpty = false) {
	std::vector<std::string> out;
	std::string token;
	std::istringstream ss(s);
	while (std::getline(ss, token, delim)) {
		if (!token.empty() || keepEmpty)
			out.push_back(token);
	}
	return out;
}

static bool parseCableParentChild(const std::string &cableName, std::string &parent, std::string &child) {
	parent.clear();
	child.clear();

	// Tokenize by '/'
	std::vector<std::string> tokens = split(cableName, '/');
	if (tokens.empty())
		return false;

	// Clean tokens: drop empty and '*'
	std::vector<std::string> clean;
	clean.reserve(tokens.size());
	for (const auto &t : tokens) {
		if (!t.empty() && t != "*")
			clean.push_back(t);
	}
	if (clean.size() < 2)
		return false;

	// Helper: is trailing tag like B01 or R01
	auto looksLikeTailTag = [](const std::string &s) {
		if (s.size() < 2)
			return false;
		char c0 = s[0];
		if (c0 != 'B' && c0 != 'b' && c0 != 'R' && c0 != 'r')
			return false;
		return std::all_of(s.begin() + 1, s.end(), ::isdigit);
	};

	// Strip ONE trailing tag if present (Bxx or Rxx)
	if (looksLikeTailTag(clean.back())) {
		clean.pop_back();
	}
	if (clean.size() < 2)
		return false;

	// TTC-aware rule: if any TTC token exists, connect root panel -> TTC
	auto isTTC = [](const std::string &s) { return s.rfind("TTC-", 0) == 0 || s.rfind("TTC", 0) == 0; };

	int ttcIdx = -1;
	for (int i = static_cast<int>(clean.size()) - 1; i >= 0; --i) {
		if (isTTC(clean[i])) {
			ttcIdx = i;
			break;
		}
	}

	if (ttcIdx >= 0) {
		// Parent: first segment (the feeder/panel), Child: the TTC token
		parent = clean.front();
		child = clean[ttcIdx];
		return true;
	}

	// Default heuristic: parent = penultimate, child = last
	parent = clean[clean.size() - 2];
	child = clean[clean.size() - 1];
	return true;
}

// -----------------------------------------------------------------------------
// Public: load default file
// -----------------------------------------------------------------------------
Circuit::Circuit() { readFromFile(std::string(PROJECT_ROOT_DIR) + "/src/circuit/data.csv"); }

// -----------------------------------------------------------------------------
// CSV helpers
// -----------------------------------------------------------------------------
void Circuit::trimInPlace(string &s) {
	auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
	s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
}

Circuit::string Circuit::lower(string s) {
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
	return s;
}

int Circuit::findHeader(const std::vector<string> &headers, std::initializer_list<string> candidates) {
	for (size_t i = 0; i < headers.size(); ++i) {
		const string h = lower(headers[i]);
		for (const auto &cand : candidates) {
			if (h.find(lower(cand)) != string::npos)
				return static_cast<int>(i);
		}
	}
	return -1;
}

// CSV parsing that respects quotes and doubled quotes
std::vector<Circuit::string> Circuit::parseCsvLine(const string &line) {
	std::vector<string> out;
	string cur;
	bool inQuotes = false;

	for (size_t i = 0; i < line.size(); ++i) {
		char c = line[i];
		if (c == '"') {
			if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
				cur.push_back('"');
				++i;
			} else {
				inQuotes = !inQuotes;
			}
		} else if (c == ',' && !inQuotes) {
			trimInPlace(cur);
			out.emplace_back(std::move(cur));
			cur.clear();
		} else {
			cur.push_back(c);
		}
	}
	trimInPlace(cur);
	out.emplace_back(std::move(cur));
	return out;
}

// -----------------------------------------------------------------------------
// Node management
// -----------------------------------------------------------------------------
Circuit::Node &Circuit::getOrCreateNode(const string &id) {
	auto it = nodesById_.find(id);
	if (it != nodesById_.end())
		return it->second;
	Node n{id};
	auto [insIt, _] = nodesById_.emplace(id, std::move(n));
	return insIt->second;
}

// -----------------------------------------------------------------------------
// Cable path token helpers
// -----------------------------------------------------------------------------
bool Circuit::isAllDigits(const string &s) {
	return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); });
}

// Tokens like B01, R01 are “tags” we don’t want as nodes
bool Circuit::isTagToken(const string &t) {
	if (t.size() >= 2 && (t[0] == 'B' || t[0] == 'R'))
		return isAllDigits(t.substr(1));
	return false;
}

// Remove trailing “/something” from a token if present
Circuit::string Circuit::stripTrailingTag(const string &s) {
	auto p = s.find('/');
	return (p == string::npos) ? s : s.substr(0, p);
}

bool Circuit::isTTC(const string &tok) { return tok.rfind("TTC-", 0) == 0 || tok.rfind("TTC", 0) == 0; }

// If a token contains “…/TTC-###/…”, return “TTC-###”
Circuit::string Circuit::extractTTC(const string &t) {
	size_t pos = t.find("TTC");
	if (pos == string::npos)
		return t;
	string out;
	for (size_t j = pos; j < t.size(); ++j) {
		char c = t[j];
		if (std::isalnum((unsigned char)c) || c == '-')
			out.push_back(c);
		else
			break;
		if (out.size() > 24)
			break; // safety
	}
	return out.empty() ? t : out;
}

// Normalize a cable path token into a canonical node id
Circuit::string Circuit::normalizeToken(string raw) {
	trimInPlace(raw);
	raw = stripTrailingTag(raw);
	if (raw.find("TTC") != string::npos) {
		string ttc = extractTTC(raw);
		return ttc;
	}
	return raw;
}

void Circuit::splitPath(const string &s, std::vector<string> &out) {
	out.clear();
	string cur;
	for (char c : s) {
		if (c == '/') {
			out.emplace_back(std::move(cur));
			cur.clear();
		} else {
			cur.push_back(c);
		}
	}
	out.emplace_back(std::move(cur));
}

// Turn one cableName into a list of normalized “node” tokens.
// Keeps “*” so buildUnifilar can wire parent→child at the star.
// Drops tag tokens like B01, R01.
void Circuit::tokenizeCable(const string &cableName, std::vector<string> &out) {
	std::vector<string> raw;
	splitPath(cableName, raw);
	out.clear();
	out.reserve(raw.size());
	for (auto &t : raw) {
		string n = normalizeToken(t);
		if (n.empty())
			continue;
		if (n == "*") {
			out.push_back("*");
			continue;
		}
		if (isTagToken(n))
			continue;
		out.push_back(n);
	}
}

// -----------------------------------------------------------------------------
// CSV reader
// -----------------------------------------------------------------------------
void Circuit::readFromFile(const std::string &csvPath) {
	nodesById_.clear();
	edges_.clear();
	uni_ = UniGraph{}; // reset derived graph

	std::ifstream in(csvPath);
	if (!in)
		throw std::runtime_error("Circuit: cannot open file: " + csvPath);

	string line;
	if (!std::getline(in, line))
		throw std::runtime_error("Circuit: CSV is empty: " + csvPath);

	// Parse header row and locate columns.
	std::vector<string> headers = parseCsvLine(line);

	const int colId = findHeader(headers, {"id gm", "id", "gm"});
	const int colCable = findHeader(headers, {"câble", "cable"});
	const int colOrig = findHeader(headers, {"origine", "origin"});
	const int colDest = findHeader(headers, {"destination", "dest"});
	const int colCond = findHeader(headers, {"cond.", "calibre", "condition", "cond et calibre"});
	const int colLen = findHeader(headers, {"longueur", "length"});

	if (colId < 0 || colCable < 0 || colOrig < 0 || colDest < 0 || colCond < 0 || colLen < 0) {
		std::ostringstream oss;
		oss << "Circuit: missing required columns. Found headers:";
		for (auto &h : headers)
			oss << " [" << h << "]";
		throw std::runtime_error(oss.str());
	}

	// Read data rows
	size_t lineNo = 1;
	while (std::getline(in, line)) {
		++lineNo;
		if (line.empty())
			continue;

		auto cols = parseCsvLine(line);
		if (static_cast<int>(cols.size()) <= std::max({colId, colCable, colOrig, colDest, colCond, colLen})) {
			continue; // malformed
		}

		string idStr = cols[(size_t)colId];
		string cable = cols[(size_t)colCable];
		string origin = cols[(size_t)colOrig];
		string dest = cols[(size_t)colDest];
		string condCal = cols[(size_t)colCond];
		string lengthStr = cols[(size_t)colLen];

		trimInPlace(idStr);
		trimInPlace(cable);
		trimInPlace(origin);
		trimInPlace(dest);
		trimInPlace(condCal);
		trimInPlace(lengthStr);

		// Parse ID
		int id = 0;
		try {
			string tmp = idStr;
			tmp.erase(std::remove_if(tmp.begin(), tmp.end(), [](unsigned char ch) { return std::isspace(ch); }), tmp.end());
			id = std::stoi(tmp);
		} catch (...) {
			continue;
		}

		// Parse length
		float len = 0.f;
		try {
			string tmp = lengthStr;
			tmp.erase(std::remove_if(tmp.begin(), tmp.end(), [](unsigned char ch) { return std::isspace(ch); }), tmp.end());
			len = std::stof(tmp);
		} catch (...) {
			continue;
		}

		// Build/reuse nodes by CSV origin/destination
		Node &uNode = getOrCreateNode(origin);
		Node &vNode = getOrCreateNode(dest);

		// Push edge (store copies)
		edges_.push_back(Edge{id, Node{uNode.id}, Node{vNode.id}, cable, condCal, len});
	}

	// Build derived unifilar graph
	buildUnifilar();
}

// -----------------------------------------------------------------------------
// Build Unifilar (pairwise chain edges + star handling)
// -----------------------------------------------------------------------------
void Circuit::buildUnifilar() {
	// Reset
	uni_ = UniGraph{};

	auto trimTailAfterSlash = [](std::string s) {
		size_t k = s.find('/');
		if (k != std::string::npos)
			s.erase(k);
		return s;
	};
	auto isRootLike = [](const std::string &s) { return s.rfind("CSE-PCHG", 0) == 0 || s.rfind("CSE-PCGH", 0) == 0; };
	auto isBJ = [](const std::string &s) { return s.rfind("BJ-", 0) == 0; };

	// To avoid duplicate synthetic anchors
	std::unordered_set<std::string> addedAnchor;
	auto anchorKey = [](const std::string &p, const std::string &c) { return p + "->" + c; };

	// 1) Build adjacency + indegree
	for (const auto &e : edges_) {
		// --- normal parent/child (your existing behavior) ---
		std::string parent, child;
		if (!parseCableParentChild(e.cableName, parent, child)) {
			parent = trimTailAfterSlash(e.u.id);
			child = trimTailAfterSlash(e.v.id);
		} else {
			parent = trimTailAfterSlash(parent);
			child = trimTailAfterSlash(child);
		}
		if (!parent.empty() && !child.empty()) {
			uni_.adj[parent].push_back(UniEdge{e.id, parent, child, e.length, e.conditionAndCaliber});
			uni_.indeg.try_emplace(parent, 0);
			uni_.indeg.try_emplace(child, 0);
			uni_.indeg[child] += 1;
		}

		// --- NEW: add synthetic panel→firstBJ anchor when needed ---
		// Tokenize full cable path to inspect its structure.
		std::vector<string> toks;
		tokenizeCable(e.cableName, toks); // keeps "*" and drops B01/R01 etc
		// Strip "*" tokens to look only at meaningful segments
		std::vector<string> clean;
		clean.reserve(toks.size());
		for (auto &t : toks)
			if (t != "*")
				clean.push_back(t);

		if (clean.size() >= 3 && isRootLike(clean.front())) {
			// Find the first BJ-* token
			int firstBJ = -1, bjCount = 0;
			for (int i = 1; i < (int)clean.size(); ++i) {
				if (isBJ(clean[i])) {
					if (firstBJ < 0)
						firstBJ = i;
					++bjCount;
				}
			}
			// Only synthesize when there are sibling BJs (e.g., BJ-170A/BJ-170B)
			if (bjCount >= 2 && firstBJ >= 0) {
				const std::string P = trimTailAfterSlash(clean.front());  // panel
				const std::string C = trimTailAfterSlash(clean[firstBJ]); // first BJ-*
				const std::string key = anchorKey(P, C);
				if (!uni_.adj[P].empty() || !uni_.adj.count(P)) {
					// ok; proceed if not already added
				}
				if (!addedAnchor.count(key)) {
					// Use id = 0 and a special cond to mark derived edge (length 0)
					uni_.adj[P].push_back(UniEdge{0, P, C, 0.0f, "derived-anchor"});
					uni_.indeg.try_emplace(P, 0);
					uni_.indeg.try_emplace(C, 0);
					uni_.indeg[C] += 1;
					addedAnchor.insert(key);
				}
			}
		}
	}

	// 2) Roots (same as yours)
	if (uni_.indeg.empty() && uni_.adj.empty())
		return;

	std::unordered_set<std::string> rootSet;
	for (const auto &kv : uni_.indeg)
		if (isRootLike(kv.first))
			rootSet.insert(kv.first);
	for (const auto &kv : uni_.indeg)
		if (kv.second == 0)
			rootSet.insert(kv.first);
	if (rootSet.empty()) {
		if (!uni_.adj.empty())
			rootSet.insert(uni_.adj.begin()->first);
		else if (!uni_.indeg.empty())
			rootSet.insert(uni_.indeg.begin()->first);
	}
	uni_.roots.assign(rootSet.begin(), rootSet.end());

	// 3) Multi-source BFS levels (unchanged)
	std::queue<std::string> q;
	for (const auto &r : uni_.roots) {
		uni_.level[r] = 0;
		q.push(r);
	}
	while (!q.empty()) {
		auto u = q.front();
		q.pop();
		const int lu = uni_.level[u];
		auto it = uni_.adj.find(u);
		if (it == uni_.adj.end())
			continue;
		for (const auto &ue : it->second) {
			const std::string &v = ue.child;
			auto lvIt = uni_.level.find(v);
			if (lvIt == uni_.level.end() || lu + 1 < lvIt->second) {
				uni_.level[v] = lu + 1;
				q.push(v);
			}
		}
	}
}
