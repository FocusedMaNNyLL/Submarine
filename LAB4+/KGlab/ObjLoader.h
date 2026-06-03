#pragma once

#include <vector>
#include <windows.h>
#include <GL/gl.h>

struct ObjVertex
{
    double x = 0, y = 0, z = 0, w = 1;
    inline const double* _ptr() const { return reinterpret_cast<const double*>(this); }
    inline double* _ptr() { return reinterpret_cast<double*>(this); }
};

struct ObjTexCord
{
    double u = 0, v = 0, w = 1;
    inline const double* _ptr() const { return reinterpret_cast<const double*>(this); }
    inline double* _ptr() { return reinterpret_cast<double*>(this); }
};

struct ObjNormal
{
    double x = 0, y = 0, z = 0;
    inline const double* _ptr() const { return reinterpret_cast<const double*>(this); }
    inline double* _ptr() { return reinterpret_cast<double*>(this); }
};

struct ObjFace
{
    std::vector<ObjVertex> vertex;
    std::vector<ObjTexCord> texCoord;
    std::vector<ObjNormal> normal;
};

class ObjModel
{
    std::vector<ObjFace> Faces;
    int listId;
    unsigned int textureId;

public:
    ObjModel();
    ~ObjModel();

    unsigned int getTextureId() const { return textureId; }

    void RenderModel(int mode) const;
    void Draw(GLenum mode = GL_POLYGON);

    int LoadModel(const char* filename);
};
