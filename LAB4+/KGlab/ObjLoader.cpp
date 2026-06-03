#include "ObjLoader.h"
#include "debout.h"
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <algorithm>
#include "stb_image.h"

using namespace std;

ObjModel::ObjModel() : listId(-1), textureId(0) {}

ObjModel::~ObjModel()
{
    if (listId != -1)
        glDeleteLists(listId, 1);
    if (textureId != 0)
        glDeleteTextures(1, &textureId);
}

int ObjModel::LoadModel(const char* filename)
{
    vector<ObjVertex> V;
    vector<ObjTexCord> VT;
    vector<ObjNormal> VN;
    vector<ObjFace> F;

    string mtlFile = "";
    string basePath = filename;
    size_t lastSlash = basePath.find_last_of("/\\");
    if (lastSlash != string::npos)
        basePath = basePath.substr(0, lastSlash + 1);

    string line;
    ifstream fin(filename);
    if (!fin.is_open())
    {
        debout << "[ObjLoader] ERROR: Cannot open file " << filename << "\n";
        return 0;
    }

    debout << "[ObjLoader] Loading " << filename << "\n";

    while (getline(fin, line))
    {
        istringstream isstr(line);
        string mode;
        isstr >> mode;
        if (mode == "v")
        {
            ObjVertex v;
            isstr >> v.x >> v.y >> v.z;
            V.push_back(v);
        }
        else if (mode == "vt")
        {
            ObjTexCord t;
            isstr >> t.u >> t.v;
            VT.push_back(t);
        }
        else if (mode == "vn")
        {
            ObjNormal n;
            isstr >> n.x >> n.y >> n.z;
            VN.push_back(n);
        }
        else if (mode == "f")
        {
            string face;
            ObjFace f;
            for (isstr >> face; isstr; isstr >> face)
            {
                istringstream isstr2(face);
                string digit;
                int n[3] = {0,0,0};
                int i = 0;
                while (getline(isstr2, digit, '/'))
                {
                    if (!digit.empty())
                        n[i] = stoi(digit);
                    i++;
                    if (i == 3) break;
                }
                if (n[0] > 0 && n[0] <= (int)V.size())
                    f.vertex.push_back(V[n[0] - 1]);
                if (n[1] > 0 && n[1] <= (int)VT.size())
                    f.texCoord.push_back(VT[n[1] - 1]);
                if (n[2] > 0 && n[2] <= (int)VN.size())
                    f.normal.push_back(VN[n[2] - 1]);
            }
            if (!f.vertex.empty())
                F.push_back(std::move(f));
        }
        else if (mode == "mtllib")
        {
            isstr >> mtlFile;
            debout << "[ObjLoader] mtllib found: " << mtlFile << "\n";
        }
    }
    fin.close();

    // Загрузка текстуры из MTL
    if (!mtlFile.empty())
    {
        string mtlPath = basePath + mtlFile;
        debout << "[ObjLoader] Looking for MTL: " << mtlPath << "\n";
        ifstream mtlStream(mtlPath);
        if (mtlStream.is_open())
        {
            string texPath;
            string line;
            while (getline(mtlStream, line))
            {
                istringstream iss(line);
                string key;
                iss >> key;
                if (key == "map_Kd")
                {
                    iss >> texPath;
                    debout << "[ObjLoader] map_Kd found: " << texPath << "\n";
                    break;
                }
            }
            mtlStream.close();

            if (!texPath.empty())
            {
                string fullTexPath = basePath + texPath;
                for (auto& c : fullTexPath)
                    if (c == '\\') c = '/';
                debout << "[ObjLoader] Loading texture: " << fullTexPath << "\n";

                glGenTextures(1, &textureId);
                glBindTexture(GL_TEXTURE_2D, textureId);
                int w, h, n;
                unsigned char* data = stbi_load(fullTexPath.c_str(), &w, &h, &n, 4);
                if (data)
                {
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    stbi_image_free(data);
                    debout << "[ObjLoader] Texture loaded successfully, ID=" << textureId << "\n";
                }
                else
                {
                    debout << "[ObjLoader] ERROR: stbi_load failed for " << fullTexPath << "\n";
                    textureId = 0;
                }
                glBindTexture(GL_TEXTURE_2D, 0);
            }
            else
            {
                debout << "[ObjLoader] No map_Kd found in MTL file.\n";
            }
        }
        else
        {
            debout << "[ObjLoader] ERROR: Cannot open MTL file " << mtlPath << "\n";
        }
    }
    else
    {
        debout << "[ObjLoader] No mtllib in OBJ file.\n";
    }

    // Сохраняем загруженные полигоны
    Faces = std::move(F);

    // Создаём display list
    if (listId != -1)
        glDeleteLists(listId, 1);
    listId = glGenLists(1);
    glNewList(listId, GL_COMPILE);
    RenderModel(GL_POLYGON);
    glEndList();

    // Очищаем временные данные
    Faces.clear();

    return 1;
}

void ObjModel::RenderModel(int mode) const
{
    for (const auto& face : Faces)
    {
        glBegin(mode);
        bool hasNormals = !face.normal.empty();
        bool hasTexCoords = !face.texCoord.empty();
        auto it_n = face.normal.begin();
        auto it_t = face.texCoord.begin();

        for (auto it_v = face.vertex.begin(); it_v != face.vertex.end(); ++it_v)
        {
            if (hasNormals)
            {
                glNormal3dv((it_n++)->_ptr());
            }
            if (hasTexCoords)
            {
                glTexCoord2dv((it_t++)->_ptr());
            }
            glVertex4dv(it_v->_ptr());
        }
        glEnd();
    }
}

void ObjModel::Draw(GLenum mode)
{
    if (listId != -1)
        glCallList(listId);
}
