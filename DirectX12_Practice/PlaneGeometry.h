#pragma once
#include "IGeometry.h"

class PlaneGeometry : public IGeometry
{
public:
    PlaneGeometry()
    {
        CreateGeometry();
    }

    virtual const std::vector<Vertex>& GetVertices() const override;
	virtual const std::vector<unsigned int>& GetIndices() const override;

private:
    void CreateGeometry();

private:
    std::vector<Vertex> mVertices{};
    std::vector<unsigned int> mIndices{};
};

inline const std::vector<Vertex>& PlaneGeometry::GetVertices() const
{
    return mVertices;
}

inline const std::vector<unsigned>& PlaneGeometry::GetIndices() const
{
    return mIndices;
}

inline void PlaneGeometry::CreateGeometry()
{
    mVertices = {
        { { -0.5f,  0.0f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f } },
        { {  0.5f,  0.0f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f } },
        { {  0.5f,  0.0f,  0.5f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
        { { -0.5f,  0.0f,  0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
    };

    mIndices = {
        0, 1, 2, 0, 2, 3,
    };
}