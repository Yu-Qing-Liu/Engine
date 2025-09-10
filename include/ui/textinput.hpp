#include "text.hpp"

class TextInput {
  public:
	TextInput(TextInput &&) = default;
	TextInput(const TextInput &) = default;
	TextInput &operator=(TextInput &&) = default;
	TextInput &operator=(const TextInput &) = default;
	~TextInput() = default;

	struct StyleParams {};

	TextInput();

  private:
};
