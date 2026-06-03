// В файле MyShaders.h (исправление метода UseShader вместо UseShaders)
// Ниже показаны только изменения, но для полноты дадим исправленный класс Shader

// В исходном MyShaders.h нужно заменить UseShaders на UseShader,
// но в вашем коде используется вызов UseShaders() (с 's') – это ошибка.
// Исправьте в классе Shader:

class Shader
{
  public:
    GLhandleARB program;
    GLhandleARB vertex;
    GLhandleARB fragment;
    std::string VshaderFileName;
    std::string FshaderFileName;

    Shader() {}
    ~Shader() {}

    void LoadShaderFromFile();
    void Compile();
    void UseShader();          // было UseShaders? Исправьте на UseShader
    static void DontUseShaders();
};

// В Render.cpp нужно заменить phong_sh.UseShaders(); на phong_sh.UseShader();
