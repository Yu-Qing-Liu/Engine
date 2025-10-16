#pragma once

#include "grid.hpp"
#include "instancedrectangle.hpp"
#include "polygon.hpp"
#include "recipesqueries.hpp"
#include "scene.hpp"
#include "textinput.hpp"
#include "textlabel.hpp"
#include "texture.hpp"

using namespace std::chrono;

class Recipe : public Scene {
	using RecipeData = RecipesQueries::Recipe;

  public:
	Recipe(Scenes &scenes, bool show = true);
	Recipe(Recipe &&) = delete;
	Recipe(const Recipe &) = delete;
	Recipe &operator=(Recipe &&) = delete;
	Recipe &operator=(const Recipe &) = delete;
	~Recipe() = default;

	std::string getName() override { return "Recipe"; }

	void fetchData() override;

	void onEnable() override;
	void onDisable() override;

	void updateScreenParams() override;

	void updateComputeUniformBuffers() override;
	void computePass() override;

	void updateUniformBuffers() override;
	void renderPass() override;
	void renderPass1() override;
	void swapChainUpdate() override;

  private:
	unique_ptr<Grid> stepsGrid;
	unique_ptr<Texture> addStepIcon;

	unique_ptr<Grid> ingredientsGrid;
	unique_ptr<Texture> addIngredientIcon;

	unique_ptr<InstancedRectangle> stepsGridBg;
	unique_ptr<InstancedRectangle> ingredientsGridBg;

	unique_ptr<TextInput> recipeNameInput;

	unique_ptr<Polygon> closeBtn;
	unique_ptr<Texture> closeBtnIcon;
	bool closePressed = false;

	unique_ptr<Polygon> confirmBtn;
	unique_ptr<Texture> confirmBtnIcon;
	bool confirmPressed = false;

	string recipeName;
	RecipeData recipe;

	vector<unique_ptr<TextLabel>> steps;
	vector<unique_ptr<TextLabel>> ingredients;

	void createStepsGridBg();
	void createIngredientsGridBg();
};
