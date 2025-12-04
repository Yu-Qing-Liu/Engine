#include "shaderquad.hpp"

struct DestinationPC {
	float time = 0.0f;
};

class Destination : public ShaderQuad<DestinationPC> {
  public:
	Destination(Scene *scene);
	~Destination() = default;
};
