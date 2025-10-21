#pragma once

#include "appdata.hpp"

#include <string>
#include <vector>

using std::string;
using std::vector;

namespace InventoryQueries {

struct Ingredient {
	string name; // unique
	float quantity{};
	string unit;
};

struct Inventory {
	vector<Ingredient> ingredients;
};

inline void createTable() {
	static const char *sql = R"SQL(
		CREATE TABLE IF NOT EXISTS inventory (
			name     TEXT PRIMARY KEY,
			quantity REAL NOT NULL DEFAULT 0,
			unit     TEXT NOT NULL DEFAULT ''
		);
	)SQL";
	char *errmsg = nullptr;
	int rc = sqlite3_exec(AppData::conn.get(), sql, nullptr, nullptr, &errmsg);
	if (rc != SQLITE_OK) {
		std::string m = errmsg ? errmsg : "unknown";
		sqlite3_free(errmsg);
		throw std::runtime_error("create inventory table failed: " + m);
	}
}

inline Inventory fetchInventory() {
	const char *sql = "SELECT name, quantity, unit FROM inventory ORDER BY name;";
	sqlite3_stmt *stmt = nullptr;
	int rc = sqlite3_prepare_v2(AppData::conn.get(), sql, -1, &stmt, nullptr);
	AppData::checkRC(rc, AppData::conn.get(), "prepare fetchInventory");

	Inventory out;
	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		Ingredient ing;
		const unsigned char *n = sqlite3_column_text(stmt, 0);
		ing.name = n ? reinterpret_cast<const char *>(n) : "";
		ing.quantity = static_cast<float>(sqlite3_column_double(stmt, 1));
		const unsigned char *u = sqlite3_column_text(stmt, 2);
		ing.unit = u ? reinterpret_cast<const char *>(u) : "";
		out.ingredients.push_back(std::move(ing));
	}
	if (rc != SQLITE_DONE)
		AppData::checkRC(rc, AppData::conn.get(), "step fetchInventory");
	sqlite3_finalize(stmt);
	return out;
}

inline void createIngredient(const Ingredient &ingredient) {
	const char *sql = R"SQL(
		INSERT INTO inventory(name, quantity, unit)
		VALUES(?, ?, ?)
		ON CONFLICT(name) DO UPDATE SET
			quantity=excluded.quantity,
			unit=excluded.unit;
	)SQL";
	sqlite3_stmt *stmt = nullptr;
	int rc = sqlite3_prepare_v2(AppData::conn.get(), sql, -1, &stmt, nullptr);
	AppData::checkRC(rc, AppData::conn.get(), "prepare createIngredient");

	sqlite3_bind_text(stmt, 1, ingredient.name.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_double(stmt, 2, ingredient.quantity);
	sqlite3_bind_text(stmt, 3, ingredient.unit.c_str(), -1, SQLITE_TRANSIENT);

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
		AppData::checkRC(rc, AppData::conn.get(), "step createIngredient");
	sqlite3_finalize(stmt);
}

inline void deleteIngredient(const string &name) {
	const char *sql = "DELETE FROM inventory WHERE name = ?;";
	sqlite3_stmt *stmt = nullptr;
	int rc = sqlite3_prepare_v2(AppData::conn.get(), sql, -1, &stmt, nullptr);
	AppData::checkRC(rc, AppData::conn.get(), "prepare deleteIngredient");

	sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
		AppData::checkRC(rc, AppData::conn.get(), "step deleteIngredient");
	sqlite3_finalize(stmt);
}

} // namespace InventoryQueries
