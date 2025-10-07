#pragma once

#include "assets.hpp"

#include <memory>
#include <sqlite3.h>
#include <string>

using DB = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;

namespace AppData {
inline std::string path = Assets::appdataPath + "/database.db";
inline DB conn{nullptr, &sqlite3_close};

inline void openDB() {
	sqlite3 *raw = nullptr;
	const int rc = sqlite3_open(path.c_str(), &raw);
	if (rc != SQLITE_OK) {
		// handle error...
		if (raw)
			sqlite3_close(raw);
		throw std::runtime_error("sqlite3_open failed");
	}
	conn.reset(raw);
}

inline void checkRC(int rc, sqlite3 *db, const char *msg) {
	if (rc != SQLITE_OK && rc != SQLITE_DONE) {
		throw std::runtime_error(std::string(msg) + ": " + sqlite3_errmsg(db));
	}
}

inline void printTables() {
	const char *list_sql = R"(
        SELECT * 
        FROM sqlite_master 
    )";

	sqlite3_stmt *list_stmt = nullptr;
	int rc = sqlite3_prepare_v2(conn.get(), list_sql, -1, &list_stmt, nullptr);
	if (rc != SQLITE_OK) {
		std::cerr << "Failed to prepare print_tables query: " << sqlite3_errmsg(conn.get()) << "\n";
		return;
	}

	std::cout << "=== Tables in database ===\n";
	while ((rc = sqlite3_step(list_stmt)) == SQLITE_ROW) {
		const char *tbl = reinterpret_cast<const char *>(sqlite3_column_text(list_stmt, 0));
		std::cout << "\n-- Table: " << tbl << " --\n";

		// 1) Fetch column names
		std::vector<std::string> columns;
		{
			std::string pragma_sql = "PRAGMA table_info(" + std::string(tbl) + ");";
			sqlite3_stmt *pragma_stmt = nullptr;
			if (sqlite3_prepare_v2(conn.get(), pragma_sql.c_str(), -1, &pragma_stmt, nullptr) == SQLITE_OK) {
				while (sqlite3_step(pragma_stmt) == SQLITE_ROW) {
					// column name is in the 2nd field of PRAGMA table_info
					const char *col_name = reinterpret_cast<const char *>(sqlite3_column_text(pragma_stmt, 1));
					columns.push_back(col_name);
				}
			}
			sqlite3_finalize(pragma_stmt);
		}

		// print header
		for (auto &col : columns) {
			std::cout << col << "\t";
		}
		std::cout << "\n";

		// 2) Select all rows
		{
			std::string select_sql = "SELECT * FROM " + std::string(tbl) + ";";
			sqlite3_stmt *select_stmt = nullptr;
			if (sqlite3_prepare_v2(conn.get(), select_sql.c_str(), -1, &select_stmt, nullptr) == SQLITE_OK) {
				int ncols = sqlite3_column_count(select_stmt);
				while (sqlite3_step(select_stmt) == SQLITE_ROW) {
					for (int i = 0; i < ncols; ++i) {
						const char *text = reinterpret_cast<const char *>(sqlite3_column_text(select_stmt, i));
						std::cout << (text ? text : "NULL") << "\t";
					}
					std::cout << "\n";
				}
			} else {
				std::cerr << "Failed to query rows for table " << tbl << ": " << sqlite3_errmsg(conn.get()) << "\n";
			}
			sqlite3_finalize(select_stmt);
		}
	}

	if (rc != SQLITE_DONE) {
		std::cerr << "Error iterating tables: " << sqlite3_errmsg(conn.get()) << "\n";
	}

	sqlite3_finalize(list_stmt);
}
} // namespace AppData
