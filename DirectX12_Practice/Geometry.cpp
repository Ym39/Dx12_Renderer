#include "Geometry.h"

CubeGeometry Geometry::mCube = CubeGeometry();

const CubeGeometry& Geometry::Cube()
{
	return mCube;
}
