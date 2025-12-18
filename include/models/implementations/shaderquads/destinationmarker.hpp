#include "shaderquad.hpp"

struct DestinationMarkerPC {
	float time = 0.0f;
};

class DestinationMarker : public ShaderQuad<DestinationMarkerPC> {
  public:
	DestinationMarker(Scene *scene);
	~DestinationMarker() = default;
};
