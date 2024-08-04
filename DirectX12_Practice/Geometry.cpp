#include "Geometry.h"
#include "CubeGeometry.h"
#include "PlaneGeometry.h"

CubeGeometry Geometry::mCube = CubeGeometry();
PlaneGeometry Geometry::mPlane = PlaneGeometry();

const CubeGeometry& Geometry::Cube()
{
	return mCube;
}

const PlaneGeometry& Geometry::Plane()
{
	return mPlane;
}
