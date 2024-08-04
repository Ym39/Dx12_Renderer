#pragma once
#include <vector>
#include "Vertex.h"

class IGeometry
{
public:
	virtual const std::vector<Vertex>& GetVertices() const = 0;
	virtual const std::vector<unsigned int>& GetIndices() const = 0;
};