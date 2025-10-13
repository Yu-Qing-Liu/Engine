#pragma once

#include "inventoryqueries.hpp"

namespace RecipesQueries {

using Ingredient = InventoryQueries::Ingredient;

struct Step {
	int num{};
	string instruction;
	float duration = 0.0f;
	string unit = "s";
};

struct Recipe {
	string name; // unique
	vector<Step> steps;
	vector<Ingredient> ingredients;
};

inline void createTable() {
	// recipes, steps, and per-recipe ingredients
	static const char *sql = R"SQL(
		CREATE TABLE IF NOT EXISTS recipes (
			name TEXT PRIMARY KEY
		);

		CREATE TABLE IF NOT EXISTS recipe_steps (
			recipe_name TEXT NOT NULL,
			num         INTEGER NOT NULL,
			instruction TEXT NOT NULL DEFAULT '',
			duration    REAL NOT NULL DEFAULT 0,
			unit        TEXT NOT NULL DEFAULT 's',
			PRIMARY KEY (recipe_name, num),
			FOREIGN KEY (recipe_name) REFERENCES recipes(name) ON DELETE CASCADE
		);

		CREATE TABLE IF NOT EXISTS recipe_ingredients (
			recipe_name TEXT NOT NULL,
			name        TEXT NOT NULL,
			quantity    REAL NOT NULL DEFAULT 0,
			unit        TEXT NOT NULL DEFAULT '',
			PRIMARY KEY (recipe_name, name),
			FOREIGN KEY (recipe_name) REFERENCES recipes(name) ON DELETE CASCADE
		);
	)SQL";

	char *errmsg = nullptr;
	int rc = sqlite3_exec(AppData::conn.get(), sql, nullptr, nullptr, &errmsg);
	if (rc != SQLITE_OK) {
		std::string m = errmsg ? errmsg : "unknown";
		sqlite3_free(errmsg);
		throw std::runtime_error("create recipes schema failed: " + m);
	}
}

inline vector<Step> fetchStepsFor(const string &recipeName) {
	const char *sql = R"SQL(
		SELECT num, instruction, duration, unit
		FROM recipe_steps
		WHERE recipe_name = ?
		ORDER BY num ASC;
	)SQL";
	sqlite3_stmt *stmt = nullptr;
	int rc = sqlite3_prepare_v2(AppData::conn.get(), sql, -1, &stmt, nullptr);
	AppData::checkRC(rc, AppData::conn.get(), "prepare fetchStepsFor");
	sqlite3_bind_text(stmt, 1, recipeName.c_str(), -1, SQLITE_TRANSIENT);

	vector<Step> steps;
	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		Step s;
		s.num = sqlite3_column_int(stmt, 0);
		const unsigned char *ins = sqlite3_column_text(stmt, 1);
		s.instruction = ins ? reinterpret_cast<const char *>(ins) : "";
		s.duration = static_cast<float>(sqlite3_column_double(stmt, 2));
		const unsigned char *u = sqlite3_column_text(stmt, 3);
		s.unit = u ? reinterpret_cast<const char *>(u) : "s";
		steps.push_back(std::move(s));
	}
	if (rc != SQLITE_DONE)
		AppData::checkRC(rc, AppData::conn.get(), "step fetchStepsFor");
	sqlite3_finalize(stmt);
	return steps;
}

inline vector<Ingredient> fetchIngredientsFor(const string &recipeName) {
	const char *sql = R"SQL(
		SELECT name, quantity, unit
		FROM recipe_ingredients
		WHERE recipe_name = ?
		ORDER BY name ASC;
	)SQL";
	sqlite3_stmt *stmt = nullptr;
	int rc = sqlite3_prepare_v2(AppData::conn.get(), sql, -1, &stmt, nullptr);
	AppData::checkRC(rc, AppData::conn.get(), "prepare fetchIngredientsFor");
	sqlite3_bind_text(stmt, 1, recipeName.c_str(), -1, SQLITE_TRANSIENT);

	vector<Ingredient> ings;
	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		Ingredient ing;
		const unsigned char *n = sqlite3_column_text(stmt, 0);
		ing.name = n ? reinterpret_cast<const char *>(n) : "";
		ing.quantity = static_cast<float>(sqlite3_column_double(stmt, 1));
		const unsigned char *u = sqlite3_column_text(stmt, 2);
		ing.unit = u ? reinterpret_cast<const char *>(u) : "";
		ings.push_back(std::move(ing));
	}
	if (rc != SQLITE_DONE)
		AppData::checkRC(rc, AppData::conn.get(), "step fetchIngredientsFor");
	sqlite3_finalize(stmt);
	return ings;
}

inline vector<Recipe> fetchRecipes() {
	const char *sql = "SELECT name FROM recipes ORDER BY name;";
	sqlite3_stmt *stmt = nullptr;
	int rc = sqlite3_prepare_v2(AppData::conn.get(), sql, -1, &stmt, nullptr);
	AppData::checkRC(rc, AppData::conn.get(), "prepare fetchRecipes");

	vector<Recipe> out;
	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		const unsigned char *n = sqlite3_column_text(stmt, 0);
		string name = n ? reinterpret_cast<const char *>(n) : "";
		Recipe r;
		r.name = name;
		r.steps = fetchStepsFor(name);
		r.ingredients = fetchIngredientsFor(name);
		out.push_back(std::move(r));
	}
	if (rc != SQLITE_DONE)
		AppData::checkRC(rc, AppData::conn.get(), "step fetchRecipes");
	sqlite3_finalize(stmt);
	return out;
}

inline Recipe fetchRecipe(const string &name) {
	const char *sql = "SELECT 1 FROM recipes WHERE name = ?;";
	sqlite3_stmt *stmt = nullptr;
	int rc = sqlite3_prepare_v2(AppData::conn.get(), sql, -1, &stmt, nullptr);
	AppData::checkRC(rc, AppData::conn.get(), "prepare fetchRecipe exists?");
	sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

	rc = sqlite3_step(stmt);
	if (rc == SQLITE_DONE) {
		sqlite3_finalize(stmt);
		return Recipe{}; // not found: return default
	}
	if (rc != SQLITE_ROW)
		AppData::checkRC(rc, AppData::conn.get(), "step fetchRecipe exists?");
	sqlite3_finalize(stmt);

	Recipe r;
	r.name = name;
	r.steps = fetchStepsFor(name);
	r.ingredients = fetchIngredientsFor(name);
	return r;
}

inline void createRecipe(const Recipe &recipe) {
	char *errmsg = nullptr;
	int rc = sqlite3_exec(AppData::conn.get(), "BEGIN IMMEDIATE;", nullptr, nullptr, &errmsg);
	if (rc != SQLITE_OK) {
		std::string m = errmsg ? errmsg : "unknown";
		sqlite3_free(errmsg);
		throw std::runtime_error("BEGIN failed: " + m);
	}

	auto rollback_on_error = [](bool &ok) {
		if (!ok) {
			char *em = nullptr;
			sqlite3_exec(AppData::conn.get(), "ROLLBACK;", nullptr, nullptr, &em);
			if (em)
				sqlite3_free(em);
		}
	};

	bool ok = false;
	do {
		// Upsert recipe
		{
			const char *sql = R"SQL(
				INSERT INTO recipes(name) VALUES(?)
				ON CONFLICT(name) DO NOTHING;
			)SQL";
			sqlite3_stmt *stmt = nullptr;
			rc = sqlite3_prepare_v2(AppData::conn.get(), sql, -1, &stmt, nullptr);
			AppData::checkRC(rc, AppData::conn.get(), "prepare upsert recipe");
			sqlite3_bind_text(stmt, 1, recipe.name.c_str(), -1, SQLITE_TRANSIENT);
			rc = sqlite3_step(stmt);
			if (rc != SQLITE_DONE)
				AppData::checkRC(rc, AppData::conn.get(), "step upsert recipe");
			sqlite3_finalize(stmt);
		}

		// Replace steps
		{
			const char *del = "DELETE FROM recipe_steps WHERE recipe_name = ?;";
			sqlite3_stmt *stmt = nullptr;
			rc = sqlite3_prepare_v2(AppData::conn.get(), del, -1, &stmt, nullptr);
			AppData::checkRC(rc, AppData::conn.get(), "prepare delete steps");
			sqlite3_bind_text(stmt, 1, recipe.name.c_str(), -1, SQLITE_TRANSIENT);
			rc = sqlite3_step(stmt);
			if (rc != SQLITE_DONE)
				AppData::checkRC(rc, AppData::conn.get(), "step delete steps");
			sqlite3_finalize(stmt);

			const char *ins = R"SQL(
				INSERT INTO recipe_steps(recipe_name, num, instruction, duration, unit)
				VALUES(?,?,?,?,?);
			)SQL";
			for (const auto &s : recipe.steps) {
				sqlite3_stmt *pst = nullptr;
				rc = sqlite3_prepare_v2(AppData::conn.get(), ins, -1, &pst, nullptr);
				AppData::checkRC(rc, AppData::conn.get(), "prepare insert step");
				sqlite3_bind_text(pst, 1, recipe.name.c_str(), -1, SQLITE_TRANSIENT);
				sqlite3_bind_int(pst, 2, s.num);
				sqlite3_bind_text(pst, 3, s.instruction.c_str(), -1, SQLITE_TRANSIENT);
				sqlite3_bind_double(pst, 4, s.duration);
				sqlite3_bind_text(pst, 5, s.unit.c_str(), -1, SQLITE_TRANSIENT);
				rc = sqlite3_step(pst);
				if (rc != SQLITE_DONE)
					AppData::checkRC(rc, AppData::conn.get(), "step insert step");
				sqlite3_finalize(pst);
			}
		}

		// Replace ingredients
		{
			const char *del = "DELETE FROM recipe_ingredients WHERE recipe_name = ?;";
			sqlite3_stmt *stmt = nullptr;
			rc = sqlite3_prepare_v2(AppData::conn.get(), del, -1, &stmt, nullptr);
			AppData::checkRC(rc, AppData::conn.get(), "prepare delete recipe_ingredients");
			sqlite3_bind_text(stmt, 1, recipe.name.c_str(), -1, SQLITE_TRANSIENT);
			rc = sqlite3_step(stmt);
			if (rc != SQLITE_DONE)
				AppData::checkRC(rc, AppData::conn.get(), "step delete recipe_ingredients");
			sqlite3_finalize(stmt);

			const char *ins = R"SQL(
				INSERT INTO recipe_ingredients(recipe_name, name, quantity, unit)
				VALUES(?,?,?,?);
			)SQL";
			for (const auto &ing : recipe.ingredients) {
				sqlite3_stmt *pst = nullptr;
				rc = sqlite3_prepare_v2(AppData::conn.get(), ins, -1, &pst, nullptr);
				AppData::checkRC(rc, AppData::conn.get(), "prepare insert recipe ingredient");
				sqlite3_bind_text(pst, 1, recipe.name.c_str(), -1, SQLITE_TRANSIENT);
				sqlite3_bind_text(pst, 2, ing.name.c_str(), -1, SQLITE_TRANSIENT);
				sqlite3_bind_double(pst, 3, ing.quantity);
				sqlite3_bind_text(pst, 4, ing.unit.c_str(), -1, SQLITE_TRANSIENT);
				rc = sqlite3_step(pst);
				if (rc != SQLITE_DONE)
					AppData::checkRC(rc, AppData::conn.get(), "step insert recipe ingredient");
				sqlite3_finalize(pst);
			}
		}

		ok = true;
	} while (false);

	if (!ok) {
		rollback_on_error(ok);
		throw std::runtime_error("createRecipe failed (rolled back)");
	}

	rc = sqlite3_exec(AppData::conn.get(), "COMMIT;", nullptr, nullptr, &errmsg);
	if (rc != SQLITE_OK) {
		std::string m = errmsg ? errmsg : "unknown";
		sqlite3_free(errmsg);
		throw std::runtime_error("COMMIT failed: " + m);
	}
}

inline void deleteRecipe(const Recipe &recipe) {
	const char *sql = "DELETE FROM recipes WHERE name = ?;";
	sqlite3_stmt *stmt = nullptr;
	int rc = sqlite3_prepare_v2(AppData::conn.get(), sql, -1, &stmt, nullptr);
	AppData::checkRC(rc, AppData::conn.get(), "prepare deleteRecipe");
	sqlite3_bind_text(stmt, 1, recipe.name.c_str(), -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
		AppData::checkRC(rc, AppData::conn.get(), "step deleteRecipe");
	sqlite3_finalize(stmt);
}

// === Step helpers ===
// createStep: append a blank step at the end (num = max(num)+1)
inline void createStep(const string &recipeName) {
	const char *sql = R"SQL(
		INSERT INTO recipe_steps(recipe_name, num, instruction, duration, unit)
		SELECT ?, COALESCE(MAX(num)+1, 1), '', 0.0, 's'
		FROM recipe_steps
		WHERE recipe_name = ?;
	)SQL";
	sqlite3_stmt *stmt = nullptr;
	int rc = sqlite3_prepare_v2(AppData::conn.get(), sql, -1, &stmt, nullptr);
	AppData::checkRC(rc, AppData::conn.get(), "prepare createStep");
	sqlite3_bind_text(stmt, 1, recipeName.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, recipeName.c_str(), -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
		AppData::checkRC(rc, AppData::conn.get(), "step createStep");
	sqlite3_finalize(stmt);
}

// deleteStep: remove a specific step number and keep sequence contiguous
inline void deleteStep(const string &recipeName, int stepNum) {
	if (stepNum <= 0) {
		throw std::invalid_argument("deleteStep: numToDelete must be >= 1");
	}

	// Begin a short transaction to keep things consistent
	char *errmsg = nullptr;
	int rc = sqlite3_exec(AppData::conn.get(), "BEGIN IMMEDIATE;", nullptr, nullptr, &errmsg);
	if (rc != SQLITE_OK) {
		std::string m = errmsg ? errmsg : "unknown";
		if (errmsg)
			sqlite3_free(errmsg);
		throw std::runtime_error("BEGIN failed: " + m);
	}

	bool ok = false;
	do {
		// 1) Delete the requested step
		const char *del_sql = R"SQL(
			DELETE FROM recipe_steps
			WHERE recipe_name = ? AND num = ?;
		)SQL";
		sqlite3_stmt *del = nullptr;
		rc = sqlite3_prepare_v2(AppData::conn.get(), del_sql, -1, &del, nullptr);
		AppData::checkRC(rc, AppData::conn.get(), "prepare delete step by num");
		sqlite3_bind_text(del, 1, recipeName.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(del, 2, stepNum);
		rc = sqlite3_step(del);
		if (rc != SQLITE_DONE)
			AppData::checkRC(rc, AppData::conn.get(), "step delete step by num");
		const int deleted_rows = sqlite3_changes(AppData::conn.get());
		sqlite3_finalize(del);

		// If nothing was deleted, just commit and return (no renumbering needed)
		if (deleted_rows == 0) {
			ok = true;
			break;
		}

		// 2) Shift down any steps after the deleted one to keep numbering contiguous
		const char *shift_sql = R"SQL(
			UPDATE recipe_steps
			SET num = num - 1
			WHERE recipe_name = ? AND num > ?;
		)SQL";
		sqlite3_stmt *shift = nullptr;
		rc = sqlite3_prepare_v2(AppData::conn.get(), shift_sql, -1, &shift, nullptr);
		AppData::checkRC(rc, AppData::conn.get(), "prepare shift steps down");
		sqlite3_bind_text(shift, 1, recipeName.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(shift, 2, stepNum);
		rc = sqlite3_step(shift);
		if (rc != SQLITE_DONE)
			AppData::checkRC(rc, AppData::conn.get(), "step shift steps down");
		sqlite3_finalize(shift);

		ok = true;
	} while (false);

	if (!ok) {
		char *e2 = nullptr;
		sqlite3_exec(AppData::conn.get(), "ROLLBACK;", nullptr, nullptr, &e2);
		if (e2)
			sqlite3_free(e2);
		throw std::runtime_error("deleteStep failed (rolled back)");
	}

	rc = sqlite3_exec(AppData::conn.get(), "COMMIT;", nullptr, nullptr, &errmsg);
	if (rc != SQLITE_OK) {
		std::string m = errmsg ? errmsg : "unknown";
		if (errmsg)
			sqlite3_free(errmsg);
		throw std::runtime_error("COMMIT failed: " + m);
	}
}

// === Ingredient helpers ===
// NOTE: These signatures don't provide ingredient details.
// We insert/remove a placeholder entry to keep the API functional.
// Prefer adding better overloads, e.g. createIngredient(recipeName, Ingredient) and deleteIngredient(recipeName, name).

inline void createIngredient(const string &recipeName) {
	const char *sql = R"SQL(
		INSERT INTO recipe_ingredients(recipe_name, name, quantity, unit)
		VALUES(?, '__placeholder__', 0.0, '')
		ON CONFLICT(recipe_name, name) DO NOTHING;
	)SQL";
	sqlite3_stmt *stmt = nullptr;
	int rc = sqlite3_prepare_v2(AppData::conn.get(), sql, -1, &stmt, nullptr);
	AppData::checkRC(rc, AppData::conn.get(), "prepare createIngredient (placeholder)");
	sqlite3_bind_text(stmt, 1, recipeName.c_str(), -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
		AppData::checkRC(rc, AppData::conn.get(), "step createIngredient (placeholder)");
	sqlite3_finalize(stmt);
}

inline void deleteIngredient(const string &recipeName) {
	// Remove the placeholder if present; otherwise remove an arbitrary ingredient
	const char *sql = R"SQL(
		DELETE FROM recipe_ingredients
		WHERE rowid IN (
		  SELECT rowid FROM recipe_ingredients
		  WHERE recipe_name = ?
		  ORDER BY (name = '__placeholder__') DESC, name ASC
		  LIMIT 1
		);
	)SQL";
	sqlite3_stmt *stmt = nullptr;
	int rc = sqlite3_prepare_v2(AppData::conn.get(), sql, -1, &stmt, nullptr);
	AppData::checkRC(rc, AppData::conn.get(), "prepare deleteIngredient (heuristic)");
	sqlite3_bind_text(stmt, 1, recipeName.c_str(), -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
		AppData::checkRC(rc, AppData::conn.get(), "step deleteIngredient (heuristic)");
	sqlite3_finalize(stmt);
}

} // namespace RecipesQueries
