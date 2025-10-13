#pragma once

#include "grid.hpp"
#include "recipesqueries.hpp"
#include "scene.hpp"
#include "textlabel.hpp"
#include "texture.hpp"

using namespace std::chrono;

class Recipes : public Scene {
	using Recipe = RecipesQueries::Recipe;

  public:
	Recipes(Scenes &scenes, bool show = true);
	Recipes(Recipes &&) = delete;
	Recipes(const Recipes &) = delete;
	Recipes &operator=(Recipes &&) = delete;
	Recipes &operator=(const Recipes &) = delete;
	~Recipes() = default;

	std::string getName() override { return "Recipes"; }

	void fetchData() override;

	void updateScreenParams() override;

	void updateComputeUniformBuffers() override;
	void computePass() override;

	void updateUniformBuffers() override;
	void renderPass() override;
	void renderPass1() override;
	void swapChainUpdate() override;

  private:
	unique_ptr<Grid> grid;
	unique_ptr<Texture> addRecipeIcon;

	vector<unique_ptr<TextLabel>> recipeNames;

	vector<Recipe> recipes;
};
