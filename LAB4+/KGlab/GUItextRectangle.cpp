#include "GUItextRectangle.h"
#include <windows.h>
#include <GL/gl.h>
#include <cstring>
#include <algorithm>

class GuiTextRectanglePrivate
{
public:
    HBITMAP bitmap;
    int w;
    int h;
    HDC dc;
    int pos_x;
    int pos_y;
    unsigned char* b;
    unsigned char* _tmp;
    GLuint tex_id;
    RECT r;
    GuiTextRectanglePrivate() : bitmap(nullptr), w(0), h(0), dc(nullptr), pos_x(0), pos_y(0), b(nullptr), _tmp(nullptr), tex_id(0)
    {
        r.top = 0;
        r.left = 0;
        r.right = 0;
        r.bottom = 0;
    }
    ~GuiTextRectanglePrivate()
    {
        if (bitmap) DeleteObject(bitmap);
        if (dc) DeleteDC(dc);
        delete[] _tmp;
        if (tex_id) glDeleteTextures(1, &tex_id);
    }
};

GuiTextRectangle::GuiTextRectangle()
{
    d_ptr = new GuiTextRectanglePrivate;
}

GuiTextRectangle::~GuiTextRectangle()
{
    delete d_ptr;
}

void GuiTextRectangle::setSize(int width, int height)
{
    GuiTextRectanglePrivate* _d = d_ptr;
    if (_d->bitmap)
        DeleteObject(_d->bitmap);
    if (_d->dc)
        DeleteDC(_d->dc);

    _d->h = height;
    _d->w = width;
    _d->dc = CreateCompatibleDC(0);

    BITMAPINFOHEADER binfo = {0};
    binfo.biBitCount = 32;
    binfo.biWidth = width;
    binfo.biHeight = height;
    binfo.biSize = sizeof(binfo);
    binfo.biPlanes = 1;
    binfo.biCompression = BI_RGB;

    _d->bitmap = CreateDIBSection(0, (BITMAPINFO*)&binfo, DIB_RGB_COLORS, (void**)&(_d->b), 0, 0);
    SelectObject(_d->dc, _d->bitmap);

    delete[] _d->_tmp;
    _d->_tmp = new unsigned char[_d->w * _d->h * 4];

    if (_d->tex_id != 0)
        glDeleteTextures(1, &(_d->tex_id));
    glGenTextures(1, &(_d->tex_id));
    glBindTexture(GL_TEXTURE_2D, _d->tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

int GuiTextRectangle::getWidth() { return d_ptr->w; }
int GuiTextRectangle::getHeight() { return d_ptr->h; }
void GuiTextRectangle::setPosition(int x, int y) { d_ptr->pos_x = x; d_ptr->pos_y = y; }

void GuiTextRectangle::setText(const wchar_t* text, char r, char g, char b)
{
    GuiTextRectanglePrivate* _d = d_ptr;
    _d->r.right = _d->w;
    _d->r.bottom = _d->h;

    // Заливаем чёрным (фон станет прозрачным)
    memset(_d->b, 0, _d->w * _d->h * 4);

    SetTextColor(_d->dc, RGB(255, 255, 255));  // текст белый в GDI
    SetBkColor(_d->dc, RGB(0, 0, 0));          // фон чёрный

    HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Consolas");
    SelectObject(_d->dc, hFont);
    DrawTextW(_d->dc, text, -1, &(_d->r), DT_LEFT | DT_TOP);
    DeleteObject(hFont);

    glBindTexture(GL_TEXTURE_2D, _d->tex_id);
    // Строим текстуру: чёрный фон -> прозрачный, белый текст -> нужный цвет (r,g,b)
    for (int i = 0; i < _d->h; ++i) {
        for (int j = 0; j < _d->w; ++j) {
            int idx = i * _d->w * 4 + j * 4;
            unsigned char R = _d->b[idx + 0];
            unsigned char G = _d->b[idx + 1];
            unsigned char B = _d->b[idx + 2];
            // Если пиксель не чёрный (яркость > 10) – считаем текстом
            if (R > 10 || G > 10 || B > 10) {
                _d->_tmp[idx + 0] = r;
                _d->_tmp[idx + 1] = g;
                _d->_tmp[idx + 2] = b;
                _d->_tmp[idx + 3] = 255;
            }
            else {
                _d->_tmp[idx + 0] = 0;
                _d->_tmp[idx + 1] = 0;
                _d->_tmp[idx + 2] = 0;
                _d->_tmp[idx + 3] = 0;
            }
        }
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _d->w, _d->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, _d->_tmp);
}

void GuiTextRectangle::Draw()
{
    GuiTextRectanglePrivate* _d = d_ptr;
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_LIGHTING);
    bool texEnabled = glIsEnabled(GL_TEXTURE_2D);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, _d->tex_id);
    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2i(_d->pos_x, _d->pos_y);
    glTexCoord2f(0, 1); glVertex2i(_d->pos_x, _d->pos_y + _d->h);
    glTexCoord2f(1, 1); glVertex2i(_d->pos_x + _d->w, _d->pos_y + _d->h);
    glTexCoord2f(1, 0); glVertex2i(_d->pos_x + _d->w, _d->pos_y);
    glEnd();
    if (!texEnabled) glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}
