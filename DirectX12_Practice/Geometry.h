#pragma once

class CubeGeometry;
class PlaneGeometry;

class Geometry
{
public:
	static const CubeGeometry& Cube();
	static const PlaneGeometry& Plane();

private:
	static CubeGeometry mCube;
	static PlaneGeometry mPlane;
};

