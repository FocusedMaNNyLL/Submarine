#include "Render.h"
#include "GUItextRectangle.h"
#include "MyShaders.h"
#include "ObjLoader.h"
#include "Texture.h"
#include "Camera.h"
#include "Light.h"
#include "debout.h"
#include "MyOGL.h"

#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <cmath>
#include <vector>
#include <cstdlib>
#include <ctime>

extern OpenGL gl;

Light light;
Camera camera;

// ----------------------------------------------------------------------
//  Глобальные флаги (управление с клавиатуры)
// ----------------------------------------------------------------------
bool texturing = true;          // Вкл/выкл текстур (клавиша T)
bool lightning = true;          // Вкл/выкл освещения (L)
bool alpha = true;              // Вкл/выкл альфа-смешения (C)
bool isPlayerDead = false;      // Погиб от медузы
bool gameActive = true;         // Игра активна (сбор кристаллов)
bool missionComplete = false;   // Все кристаллы собраны
double freezeTime = 0.0;        // Время заморозки (фиксируется в момент смерти/победы)

// ----------------------------------------------------------------------
//  Структуры для объектов сцены
// ----------------------------------------------------------------------

// Камни (статические, разбросаны случайно)
struct Rock { float x, y, z, size; };
std::vector<Rock> rocks;

// Крабы (движутся по дну, отражаются от границ)
struct Crab {
    float x, z;
    float vx, vz;
    float angle;            // угол поворота (градусы)
    float changeDirTimer;   // таймер до смены направления (сек)
    float scale;
};
std::vector<Crab> crabs;

// Рыбы (свободное движение, меняют курс)
struct FishData {
    float x, z;
    float vx, vz;
    float angle;
    float changeDirTimer;
};
std::vector<FishData> fishes;

double full_time = 0;               // Общее время игры (сек)
float subX = 0.0f, subY = -0.7f, subZ = 0.0f;   // Позиция батискафа
float subAngle = 0.0f;              // Угол поворота батискафа (градусы)
const float modelFixAngle = 180.0f; // Доворот модели (для правильной ориентации)

// Кристаллы (12 штук, фиксированные позиции)
const int crystalCount = 12;
float crystalX[crystalCount] = { 3.0f, -4.0f,  8.0f, -9.0f,  6.0f, -2.0f, -20.0f, 20.0f, -30.0f, 30.0f,  0.0f, -15.0f };
float crystalY[crystalCount] = { -1.45f, -0.8f, -0.2f, 0.3f, 0.8f, 1.2f, -0.5f, 0.0f, 0.9f, -1.2f, -0.3f, 1.5f };
float crystalZ[crystalCount] = { -3.0f, -6.0f, -8.0f,  2.0f,  5.0f,  8.0f, -25.0f, 25.0f,  15.0f, -20.0f, -35.0f,  30.0f };
float crystalScale[crystalCount] = { 0.45f, 0.40f, 0.50f, 0.42f, 0.38f, 0.46f, 0.45f, 0.40f, 0.50f, 0.42f, 0.38f, 0.46f };
float crystalAngle[crystalCount] = { 20.0f, 70.0f, 130.0f, 200.0f, 300.0f, 250.0f, 20.0f, 70.0f, 130.0f, 200.0f, 300.0f, 250.0f };
bool crystalCollected[crystalCount] = { false };
int collectedCrystals = 0;

// ----------------------------------------------------------------------
//  Обработка нажатий клавиш (T, L, C, R)
// ----------------------------------------------------------------------
void switchModes(OpenGL* sender, KeyEventArg arg)
{
    auto key = LOWORD(MapVirtualKeyA(arg.key, MAPVK_VK_TO_CHAR));
    switch (key)
    {
    case 'L': lightning = !lightning; break;
    case 'T': texturing = !texturing; break;
    case 'C': alpha = !alpha; break;
    case 'R':
        // Перезапуск игры после смерти или победы
        if (isPlayerDead || missionComplete) {
            isPlayerDead = false;
            missionComplete = false;
            gameActive = true;
            subX = 0.0f; subY = -0.7f; subZ = 0.0f; subAngle = 0.0f;
            collectedCrystals = 0;
            for (int i = 0; i < crystalCount; ++i) crystalCollected[i] = false;
            freezeTime = 0.0;
        }
        break;
    }
}

// ----------------------------------------------------------------------
//  Глобальные объекты (модели, шейдеры, текстуры)
// ----------------------------------------------------------------------
GuiTextRectangle text;                       // 2D текст (интерфейс)
ObjModel submarine, rockModel, fish01Model, crystalModel, jellyfishModel, crabModel;
Shader cassini_sh, phong_sh, simple_texture_sh;
Texture rockTex, sandTex, causticsTex;
Texture submarineFallbackTex;               // резервная текстура батискафа

// ----------------------------------------------------------------------
//  Инициализация: загрузка шейдеров, текстур, моделей
// ----------------------------------------------------------------------
void initRender()
{
    srand((unsigned)time(NULL));

    // ----- Шейдеры -----
    cassini_sh.VshaderFileName = "shaders/v.vert";
    cassini_sh.FshaderFileName = "shaders/cassini.frag";
    cassini_sh.LoadShaderFromFile(); cassini_sh.Compile();

    phong_sh.VshaderFileName = "shaders/v.vert";
    phong_sh.FshaderFileName = "shaders/light.frag";
    phong_sh.LoadShaderFromFile(); phong_sh.Compile();

    simple_texture_sh.VshaderFileName = "shaders/v.vert";
    simple_texture_sh.FshaderFileName = "shaders/textureShader.frag";
    simple_texture_sh.LoadShaderFromFile(); simple_texture_sh.Compile();

    // ----- Текстуры -----
    rockTex.LoadTexture("textures/rock_diffuse.jpg");
    sandTex.LoadTexture("textures/sand_seabed.png");
    causticsTex.LoadTexture("textures/water_caustics.png");

    // ----- 3D модели (текстуры берутся из MTL) -----
    crabModel.LoadModel("models/crab/crab_01.obj");
    rockModel.LoadModel("models/rock/rock.obj");
    fish01Model.LoadModel("models/fish01/fish01.obj");
    crystalModel.LoadModel("models/crystal/crystal_single.obj");
    jellyfishModel.LoadModel("models/jellyfish/jellyfish.obj");
    submarine.LoadModel("models/submarine/submarine.obj");

    // ----- Инициализация крабов (25 штук, случайное движение) -----
    crabs.resize(25);
    for (int i = 0; i < 25; ++i) {
        crabs[i].x = -80.0f + (rand() % 16000) / 100.0f;
        crabs[i].z = -80.0f + (rand() % 16000) / 100.0f;
        float angleRad = (float)(rand() % 360) * 3.14159f / 180.0f;
        float speed = 0.8f + (rand() % 15) / 10.0f;
        crabs[i].vx = cos(angleRad) * speed;
        crabs[i].vz = sin(angleRad) * speed;
        crabs[i].angle = atan2(crabs[i].vz, crabs[i].vx) * 180.0f / 3.14159f;
        crabs[i].changeDirTimer = 3.0f + (rand() % 40) / 10.0f;
        crabs[i].scale = 0.16f + (rand() % 20) / 100.0f;
    }

    // ----- Инициализация рыб (60 штук, физика движения) -----
    fishes.resize(60);
    for (int i = 0; i < 60; ++i) {
        fishes[i].x = -70.0f + fmodf(i * 19.3f, 140.0f);
        fishes[i].z = -70.0f + fmodf(i * 27.7f, 140.0f);
        float angleRad = (float)(rand() % 360) * 3.14159f / 180.0f;
        float speed = 2.5f + (rand() % 40) / 10.0f;
        fishes[i].vx = cos(angleRad) * speed;
        fishes[i].vz = sin(angleRad) * speed;
        fishes[i].angle = atan2(fishes[i].vz, fishes[i].vx) * 180.0f / 3.14159f;
        fishes[i].changeDirTimer = 4.0f + (rand() % 60) / 10.0f;
    }

    // ----- Настройки OpenGL и камеры -----
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    camera.caclulateCameraPos();

    // Привязываем события мыши/клавиатуры к обработчикам
    gl.WheelEvent.reaction(&camera, &Camera::Zoom);
    gl.MouseMovieEvent.reaction(&camera, &Camera::MouseMovie);
    gl.MouseLeaveEvent.reaction(&camera, &Camera::MouseLeave);
    gl.MouseLdownEvent.reaction(&camera, &Camera::MouseStartDrag);
    gl.MouseLupEvent.reaction(&camera, &Camera::MouseStopDrag);
    gl.MouseMovieEvent.reaction(&light, &Light::MoveLight);
    gl.KeyDownEvent.reaction(&light, &Light::StartDrug);
    gl.KeyUpEvent.reaction(&light, &Light::StopDrug);
    gl.KeyDownEvent.reaction(switchModes);

    text.setSize(512, 180);
    camera.setPosition(2, 1.5, 1.5);
}

// --------------------------------------------------------------
//  Вспомогательные функции
// --------------------------------------------------------------

// Направление движения батискафа (X, Z) по углу
void GetForwardVector(float& dirX, float& dirZ)
{
    float rad = subAngle * 3.14159265f / 180.0f;
    dirX = -sin(rad);
    dirZ = -cos(rad);
}

// Рисование текстурированной плоскости (песок или каустики)
void DrawTexturedPlane(float sizeX, float sizeZ, float repeatX, float repeatZ)
{
    glBegin(GL_QUADS);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-sizeX, 0.0f, -sizeZ);
    glTexCoord2f(repeatX, 0.0f); glVertex3f(sizeX, 0.0f, -sizeZ);
    glTexCoord2f(repeatX, repeatZ); glVertex3f(sizeX, 0.0f, sizeZ);
    glTexCoord2f(0.0f, repeatZ); glVertex3f(-sizeX, 0.0f, sizeZ);
    glEnd();
}

// Отрисовка одного камня
void DrawRock(double x, double y, double z, double sx, double sy, double sz, double angle)
{
    glPushMatrix();
    glTranslated(x, y, z);
    glScaled(sx, sy, sz);
    glRotated(angle, 0, 1, 0);
    rockModel.Draw();
    glPopMatrix();
}

// Евклидово расстояние между двумя точками
float Distance3D(float x1, float y1, float z1, float x2, float y2, float z2)
{
    float dx = x1 - x2, dy = y1 - y2, dz = z1 - z2;
    return sqrt(dx * dx + dy * dy + dz * dz);
}

// Отрисовка одного кристалла
void DrawCrystal(double x, double y, double z, double sx, double sy, double sz, double angle)
{
    glPushMatrix();
    glTranslated(x, y, z);
    glScaled(sx, sy, sz);
    glRotated(angle, 0, 1, 0);
    crystalModel.Draw();
    glPopMatrix();
}

// --------------------------------------------------------------
//  Камера, освещение, туман
// --------------------------------------------------------------

// Камера от третьего лица (следует за батискафом)
void SetThirdPersonCamera()
{
    float dirX, dirZ;
    GetForwardVector(dirX, dirZ);
    float camX = subX - dirX * 3.5f, camY = subY + 1.5f, camZ = subZ - dirZ * 3.5f;
    float tarX = subX + dirX * 2.0f, tarY = subY + 0.8f, tarZ = subZ + dirZ * 2.0f;
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(camX, camY, camZ, tarX, tarY, tarZ, 0.0f, 1.0f, 0.0f);
}

// Настройка освещения (главный направленный свет + подсветка снизу)
void SetupLighting()
{
    GLfloat globalAmbient[] = { 0.2f, 0.25f, 0.3f, 1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);

    GLfloat lightDir[] = { 0.0f, 2.0f, 1.0f, 0.0f };
    GLfloat lightAmbient[] = { 0.2f, 0.2f, 0.3f, 1.0f };
    GLfloat lightDiffuse[] = { 1.0f, 0.95f, 0.9f, 1.0f };
    GLfloat lightSpecular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lightDir);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
    glEnable(GL_LIGHT0);

    GLfloat fillPos[] = { 0.0f, -3.0f, 0.0f, 1.0f };
    GLfloat fillDiff[] = { 0.4f, 0.5f, 0.7f, 1.0f };
    GLfloat fillSpec[] = { 0.3f, 0.4f, 0.6f, 1.0f };
    glLightfv(GL_LIGHT1, GL_POSITION, fillPos);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, fillDiff);
    glLightfv(GL_LIGHT1, GL_SPECULAR, fillSpec);
    glEnable(GL_LIGHT1);

    glEnable(GL_LIGHTING);
}

// Подводный туман (экспоненциальный)
void SetUnderwaterFog()
{
    glEnable(GL_FOG);
    GLfloat fogColor[] = { 0.03f, 0.12f, 0.20f, 1.0f };
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogi(GL_FOG_MODE, GL_EXP2);
    glFogf(GL_FOG_DENSITY, 0.035f);
}

// --------------------------------------------------------------
//  Игровая логика (столкновения, сбор кристаллов, управление)
// --------------------------------------------------------------

// Проверка столкновения батискафа с камнями
bool IsSubmarineCollidingWithRocks()
{
    float subRadius = 0.3f;
    for (size_t i = 0; i < rocks.size(); ++i) {
        const Rock& r = rocks[i];
        float dx = subX - r.x, dz = subZ - r.z;
        if (sqrt(dx * dx + dz * dz) < subRadius + r.size * 0.6f && subY < -0.8f)
            return true;
    }
    return false;
}

// Проверка столкновения батискафа с медузами (смерть)
bool IsSubmarineCollidingWithJellyfish()
{
    if (!gameActive) return false;
    float subRad = 0.4f, jelRad = 0.4f;
    double t = gameActive ? full_time : freezeTime;
    for (int i = 0; i < 25; i++) {
        float mx = -75.0f + fmodf(i * 16.1f, 150.0f) + sin((float)(t * 0.4f + i)) * 2.0f;
        float mz = -75.0f + fmodf(i * 21.3f, 150.0f) + cos((float)(t * 0.4f + i)) * 2.0f;
        float my = -1.2f + fmodf(i * 0.9f, 3.0f) + sin((float)(t * 0.8f + i)) * 0.5f;
        if (Distance3D(subX, subY, subZ, mx, my, mz) < subRad + jelRad)
            return true;
    }
    return false;
}

// Обновление состояния батискафа (управление, коллизии)
void UpdateSubmarine(double dt)
{
    if (!gameActive || isPlayerDead || missionComplete) return;

    float moveSpeed = 4.0f, rotateSpeed = 90.0f, verticalSpeed = 2.5f;
    bool forward = gl.isKeyPressed('W'), backward = gl.isKeyPressed('S');
    bool left = gl.isKeyPressed('A'), right = gl.isKeyPressed('D');

    float moveDir = (forward && !backward) ? 1.0f : (backward && !forward) ? -1.0f : 0.0f;
    float steerSign = (moveDir < 0.0f) ? -1.0f : 1.0f;
    if (left) subAngle += rotateSpeed * (float)dt * steerSign;
    if (right) subAngle -= rotateSpeed * (float)dt * steerSign;

    float dirX, dirZ;
    GetForwardVector(dirX, dirZ);
    float oldX = subX, oldZ = subZ;
    if (moveDir != 0.0f) {
        subX += dirX * moveSpeed * (float)dt * moveDir;
        subZ += dirZ * moveSpeed * (float)dt * moveDir;
    }
    if (gl.isKeyPressed('Q')) subY -= verticalSpeed * (float)dt;
    if (gl.isKeyPressed('E')) subY += verticalSpeed * (float)dt;

    // Границы мира
    if (subX < -150.0f) subX = -150.0f; if (subX > 150.0f) subX = 150.0f;
    if (subZ < -150.0f) subZ = -150.0f; if (subZ > 150.0f) subZ = 150.0f;
    if (subY < -2.0f) subY = -2.0f; if (subY > 3.0f) subY = 3.0f;

    if (IsSubmarineCollidingWithRocks()) { subX = oldX; subZ = oldZ; }
    if (IsSubmarineCollidingWithJellyfish()) {
        isPlayerDead = true;
        gameActive = false;
        freezeTime = full_time;   // фиксируем время анимации
    }
}

// Сбор кристаллов
void UpdateCrystals()
{
    if (!gameActive) return;
    float collectRadius = 1.8f;
    for (int i = 0; i < crystalCount; ++i) {
        if (!crystalCollected[i] && Distance3D(subX, subY, subZ, crystalX[i], crystalY[i], crystalZ[i]) < collectRadius) {
            crystalCollected[i] = true;
            ++collectedCrystals;
        }
    }
    if (collectedCrystals == crystalCount && !missionComplete) {
        missionComplete = true;
        gameActive = false;
        freezeTime = full_time;   // фиксируем время анимации
    }
}

// Рисование плоскости каустик (текстурный эффект)
void DrawCausticsPlane(float sizeX, float sizeZ, float repeatX, float repeatZ, float time)
{
    glBegin(GL_QUADS);
    glNormal3f(0.0f, 1.0f, 0.0f);
    float shift = time * 0.03f;
    glTexCoord2f(shift, shift);                     glVertex3f(-sizeX, 0.0f, -sizeZ);
    glTexCoord2f(repeatX + shift, shift);           glVertex3f(sizeX, 0.0f, -sizeZ);
    glTexCoord2f(repeatX + shift, repeatZ + shift); glVertex3f(sizeX, 0.0f, sizeZ);
    glTexCoord2f(shift, repeatZ + shift);           glVertex3f(-sizeX, 0.0f, sizeZ);
    glEnd();
}

// ----------------------------------------------------------------------
//  Главная функция рендеринга (вызывается каждый кадр)
// ----------------------------------------------------------------------
void Render(double delta_time)
{
    // Обновляем игровое время только если игра активна
    if (gameActive) {
        full_time += delta_time;
    }
    UpdateSubmarine(delta_time);
    UpdateCrystals();

    // Дельта для движений (используем актуальное время, но обновление позиций
    // будем делать только если игра активна)
    float delta = (float)delta_time;

    // ----- Начальная настройка камеры, освещения, тумана -----
    glClearColor(0.02f, 0.08f, 0.14f, 1.0f);
    SetThirdPersonCamera();
    SetupLighting();
    SetUnderwaterFog();

    // ----- Включение/выключение глобальных режимов (по клавишам) -----
    glEnable(GL_NORMALIZE);
    if (lightning) glEnable(GL_LIGHTING); else glDisable(GL_LIGHTING);
    if (texturing) glEnable(GL_TEXTURE_2D); else glDisable(GL_TEXTURE_2D);
    if (alpha) { glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); }
    else glDisable(GL_BLEND);

    // ----- Материалы по умолчанию (вода/окружение) -----
    float amb[] = { 0.2f, 0.2f, 0.1f, 1.0f };
    float dif[] = { 0.5f, 0.6f, 0.5f, 1.0f };
    float spec[] = { 0.8f, 0.8f, 0.4f, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, amb);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, dif);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 32.0f);

    // ----- Песчаное дно -----
    float groundAmb[] = { 0.15f, 0.18f, 0.22f, 1.0f };
    float groundDif[] = { 0.85f, 0.80f, 0.70f, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, groundAmb);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, groundDif);
    sandTex.Bind();
    glColor3f(1.0f, 1.0f, 1.0f);
    glPushMatrix();
    glTranslated(0.0, -2.0, 0.0);
    DrawTexturedPlane(180.0f, 180.0f, 40.0f, 40.0f);
    glPopMatrix();

    // ----- Каустики (блики на дне) – анимация замораживается при !gameActive -----
    float animTime = gameActive ? full_time : freezeTime;
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    causticsTex.Bind();
    glColor4f(0.5f, 0.7f, 0.9f, 0.2f);
    glPushMatrix();
    glTranslated(0.0, -1.96, 0.0);
    DrawCausticsPlane(120.0f, 120.0f, 25.0f, 25.0f, (float)animTime);
    glPopMatrix();
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);

    // ----- Камни (статичные, текстура) -----
    rockTex.Bind();
    float rockAmb2[] = { 0.20f, 0.22f, 0.25f, 1.0f };
    float rockDif2[] = { 0.55f, 0.60f, 0.65f, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, rockAmb2);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, rockDif2);
    rocks.clear();
    for (int i = 0; i < 120; ++i) {
        float rx = -80.0f + fmodf(i * 23.5f, 160.0f);
        float rz = -80.0f + fmodf(i * 29.3f, 160.0f);
        if (fabs(rx) < 8.0f && fabs(rz) < 8.0f) continue; // чистая зона вокруг старта
        float scale = 0.8f + fmodf(i * 0.2f, 1.2f);
        DrawRock(rx, -1.40f, rz, scale, scale, scale, fmodf(i * 45.0f, 360.0f));
        rocks.push_back({ rx, -1.40f, rz, scale });
    }

    // ----- Кристаллы (пульсирующий материал) – пульсация замораживается -----
    GLuint crystalTex = crystalModel.getTextureId();
    if (crystalTex != 0 && texturing) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, crystalTex);
    }
    else {
        glDisable(GL_TEXTURE_2D);
    }
    float pulse = 0.5f + 0.25f * sin((float)(animTime * 2.0));
    float crysAmb[] = { 0.15f + 0.05f * pulse, 0.25f + 0.1f * pulse, 0.5f + 0.2f * pulse, 1.0f };
    float crysDif[] = { 0.3f, 0.6f + 0.2f * pulse, 0.9f, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, crysAmb);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, crysDif);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 90.0f);
    for (int i = 0; i < crystalCount; ++i)
        if (!crystalCollected[i])
            DrawCrystal(crystalX[i], crystalY[i], crystalZ[i], crystalScale[i], crystalScale[i], crystalScale[i], crystalAngle[i]);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    // ----- Рыбы (движение – обновляем только если игра активна) -----
    GLuint fishTex = fish01Model.getTextureId();
    if (fishTex != 0 && texturing) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, fishTex);
    }
    else {
        glDisable(GL_TEXTURE_2D);
    }
    float fishAmb[] = { 0.3f, 0.3f, 0.3f, 1.0f };
    float fishDif[] = { 0.8f, 0.7f, 0.6f, 1.0f };
    float fishSpec[] = { 0.4f, 0.4f, 0.4f, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, fishAmb);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, fishDif);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, fishSpec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 32.0f);

    if (gameActive) {
        // Обновляем позиции рыб
        for (int i = 0; i < 60; ++i) {
            fishes[i].x += fishes[i].vx * delta;
            fishes[i].z += fishes[i].vz * delta;
            // отражение от границ
            if (fabs(fishes[i].x) > 140.0f) {
                fishes[i].vx = -fishes[i].vx;
                fishes[i].x = (fishes[i].x > 0) ? 139.9f : -139.9f;
                fishes[i].angle = atan2(fishes[i].vz, fishes[i].vx) * 180.0f / 3.14159f;
            }
            if (fabs(fishes[i].z) > 140.0f) {
                fishes[i].vz = -fishes[i].vz;
                fishes[i].z = (fishes[i].z > 0) ? 139.9f : -139.9f;
                fishes[i].angle = atan2(fishes[i].vz, fishes[i].vx) * 180.0f / 3.14159f;
            }
            fishes[i].changeDirTimer -= delta;
            if (fishes[i].changeDirTimer <= 0) {
                float newAngleRad = fishes[i].angle * 3.14159f / 180.0f + ((rand() % 100) - 50) * 0.03f;
                float speed = sqrt(fishes[i].vx * fishes[i].vx + fishes[i].vz * fishes[i].vz);
                fishes[i].vx = cos(newAngleRad) * speed;
                fishes[i].vz = sin(newAngleRad) * speed;
                fishes[i].angle = atan2(fishes[i].vz, fishes[i].vx) * 180.0f / 3.14159f;
                fishes[i].changeDirTimer = 4.0f + (rand() % 60) / 10.0f;
            }
        }
    }
    // Отрисовка рыб (используем замороженные позиции, если игра не активна)
    for (int i = 0; i < 60; ++i) {
        float y = -1.0f + sin((float)animTime * 1.5f + i) * 0.3f;
        glPushMatrix();
        glTranslated(fishes[i].x, y, fishes[i].z);
        glScaled(1.05, 1.05, 1.05);
        glRotated(fishes[i].angle + 90.0f, 0, 1, 0);
        fish01Model.Draw();
        glPopMatrix();
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    // ----- Медузы (текстура + прозрачность) -----
    GLuint jellyTex = jellyfishModel.getTextureId();
    if (jellyTex != 0 && texturing) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, jellyTex);
    }
    else {
        glDisable(GL_TEXTURE_2D);
    }
    // Временно отключаем освещение, чтобы цвет и прозрачность работали
    GLboolean lightingWasOn = glIsEnabled(GL_LIGHTING);
    if (lightingWasOn) glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (int i = 0; i < 25; ++i) {
        float mx = -75.0f + fmodf(i * 16.1f, 150.0f);
        float mz = -75.0f + fmodf(i * 21.3f, 150.0f);
        float y = -1.2f + fmodf(i * 0.9f, 3.0f) + sin((float)(animTime * 0.8f + i)) * 0.5f;
        float x_sway = sin((float)(animTime * 0.4f + i)) * 2.0f;
        float z_sway = cos((float)(animTime * 0.4f + i)) * 2.0f;

        glPushMatrix();
        glTranslated(mx + x_sway, y, mz + z_sway);
        glScaled(0.70, 0.70, 0.70);
        if (alpha) {
            glColor4f(1.0f, 1.0f, 1.0f, 0.7f);   // полупрозрачные
        }
        else {
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        }
        jellyfishModel.Draw();
        glPopMatrix();
    }
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glDisable(GL_BLEND);
    if (lightingWasOn) glEnable(GL_LIGHTING);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    // ----- Крабы (движение обновляем только если игра активна) -----
    GLuint crabTex = crabModel.getTextureId();
    if (crabTex != 0 && texturing) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, crabTex);
    }
    else {
        glDisable(GL_TEXTURE_2D);
    }
    float crabAmb[] = { 0.25f, 0.2f, 0.15f, 1.0f };
    float crabDif[] = { 0.7f, 0.5f, 0.3f, 1.0f };
    float crabSpec[] = { 0.3f, 0.2f, 0.1f, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, crabAmb);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, crabDif);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, crabSpec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 25.0f);

    if (gameActive) {
        for (int i = 0; i < 25; ++i) {
            crabs[i].x += crabs[i].vx * delta;
            crabs[i].z += crabs[i].vz * delta;
            if (fabs(crabs[i].x) > 140.0f) {
                crabs[i].vx = -crabs[i].vx;
                crabs[i].x = (crabs[i].x > 0) ? 139.9f : -139.9f;
                crabs[i].angle = atan2(crabs[i].vz, crabs[i].vx) * 180.0f / 3.14159f;
            }
            if (fabs(crabs[i].z) > 140.0f) {
                crabs[i].vz = -crabs[i].vz;
                crabs[i].z = (crabs[i].z > 0) ? 139.9f : -139.9f;
                crabs[i].angle = atan2(crabs[i].vz, crabs[i].vx) * 180.0f / 3.14159f;
            }
            crabs[i].changeDirTimer -= delta;
            if (crabs[i].changeDirTimer <= 0) {
                float newAngleRad = crabs[i].angle * 3.14159f / 180.0f + ((rand() % 100) - 50) * 0.03f;
                float speed = sqrt(crabs[i].vx * crabs[i].vx + crabs[i].vz * crabs[i].vz);
                crabs[i].vx = cos(newAngleRad) * speed;
                crabs[i].vz = sin(newAngleRad) * speed;
                crabs[i].angle = atan2(crabs[i].vz, crabs[i].vx) * 180.0f / 3.14159f;
                crabs[i].changeDirTimer = 3.0f + (rand() % 40) / 10.0f;
            }
        }
    }
    // Отрисовка крабов
    for (int i = 0; i < 25; ++i) {
        glPushMatrix();
        glTranslated(crabs[i].x, -1.42f, crabs[i].z);
        glRotated(crabs[i].angle, 0, 1, 0);
        glScaled(crabs[i].scale, crabs[i].scale, crabs[i].scale);
        crabModel.Draw();
        glPopMatrix();
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    // ----- Батискаф (текстура из MTL или запасная) -----
    Shader::DontUseShaders();
    GLuint subTex = submarine.getTextureId();
    if (subTex == 0) subTex = submarineFallbackTex.GetId();
    if (subTex != 0 && texturing) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, subTex);
    }
    else {
        glDisable(GL_TEXTURE_2D);
    }
    float matAmbSub[] = { 0.6f, 0.6f, 0.6f, 1.0f };
    float matDifSub[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float matSpecSub[] = { 0.5f, 0.5f, 0.5f, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, matAmbSub);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, matDifSub);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpecSub);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 32.0f);
    glPushMatrix();
    glTranslated(subX, subY + 0.2f, subZ);
    glRotated(subAngle, 0, 1, 0);
    glRotated(modelFixAngle, 0, 1, 0);
    glScaled(0.3, 0.3, 0.3);
    submarine.Draw();
    glPopMatrix();
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    // ----- 2D интерфейс (текст поверх всего) -----
    glActiveTexture(GL_TEXTURE0);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, gl.getWidth(), 0, gl.getHeight(), -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    std::wstringstream ss;
    if (missionComplete) {
        ss << L"MISSION COMPLETE\n";
        ss << L"Press R to restart\n";
        ss << L"Crystals: " << collectedCrystals << L" / " << crystalCount;
    }
    else if (isPlayerDead) {
        ss << L"SUBMARINE DESTROYED\n";
        ss << L"Press R to restart\n";
        ss << L"Crystals: " << collectedCrystals << L" / " << crystalCount;
    }
    else {
        ss << L"W/A/S/D - move\n";
        ss << L"E/Q - up/down\n";
        ss << L"Crystals: " << collectedCrystals << L" / " << crystalCount << L"\n";
        ss << L"T - texturing " << (texturing ? L"ON" : L"OFF") << L"\n";
        ss << L"L - lighting " << (lightning ? L"OFF" : L"ON") << L"\n";
        ss << L"C - alpha " << (alpha ? L"ON" : L"OFF");
    }

    text.setText(ss.str().c_str(), 255, 255, 255);
    int txtW = text.getWidth();
    int txtH = text.getHeight();
    int posX = (gl.getWidth() - txtW) / 80;
    int posY = (gl.getHeight() - txtH) / 1;
    text.setPosition(posX, posY);
    text.Draw();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}
