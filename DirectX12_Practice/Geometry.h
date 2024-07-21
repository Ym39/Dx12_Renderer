#pragma once
#include "CubeGeometry.h"

class Geometry
{
public:
	static const CubeGeometry& Cube();

private:
	static CubeGeometry mCube;
};

