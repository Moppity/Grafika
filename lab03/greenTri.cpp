//=============================================================================================
// Mercator vetületű világtérkép a gömb alakú Földön legrövidebb utakkal
//=============================================================================================
#include "./framework.h"
#include <iostream>
#include <cmath>

// A képhez használt konstansok
const int winWidth = 600, winHeight = 600;
const int textureWidth = 64, textureHeight = 64;
const float PI = 3.141592653589793f;

// A térkép adatok a feladatkiírás szerint
const unsigned char mapData[] = {
    252, 252, 252, 252, 252, 252, 252, 252, 252, 0, 9, 80, 1, 148, 13, 72, 13, 140, 25, 60, 21, 132, 41, 12, 1, 28, 
    25, 128, 61, 0, 17, 4, 29, 124, 81, 8, 37, 116, 89, 0, 69, 16, 5, 48, 97, 0, 77, 0, 25, 8, 1, 8, 253, 253, 253, 253, 
    101, 10, 237, 14, 237, 14, 241, 10, 141, 2, 93, 14, 121, 2, 5, 6, 93, 14, 49, 6, 57, 26, 89, 18, 41, 10, 57, 26, 89, 18, 41, 14, 1, 2, 45, 26, 89, 26, 33, 18, 57, 14, 93, 26, 33, 18, 57, 10, 93, 18, 5, 2, 33, 18, 41, 2, 5, 2, 5, 6, 
    89, 22, 29, 2, 1, 22, 37, 2, 1, 6, 1, 2, 97, 22, 29, 38, 45, 2, 97, 10, 1, 2, 37, 42, 17, 2, 13, 2, 5, 2, 89, 10, 49, 
    46, 25, 10, 101, 2, 5, 6, 37, 50, 9, 30, 89, 10, 9, 2, 37, 50, 5, 38, 81, 26, 45, 22, 17, 54, 77, 30, 41, 22, 17, 58, 1, 2, 61, 38, 65, 2, 9, 58, 69, 46, 37, 6, 1, 10, 9, 62, 65, 38, 5, 2, 33, 102, 57, 54, 33, 102, 57, 30, 1, 14, 33, 2, 
    9, 86, 9, 2, 21, 6, 13, 26, 5, 6, 53, 94, 29, 26, 1, 22, 29, 0, 29, 98, 5, 14, 9, 46, 1, 2, 5, 6, 5, 2, 0, 13, 0, 13, 
    118, 1, 2, 1, 42, 1, 4, 5, 6, 5, 2, 4, 33, 78, 1, 6, 1, 6, 1, 10, 5, 34, 1, 20, 2, 9, 2, 12, 25, 14, 5, 30, 1, 54, 13, 6, 9, 2, 1, 32, 13, 8, 37, 2, 13, 2, 1, 70, 49, 28, 13, 16, 53, 2, 1, 46, 1, 2, 1, 2, 53, 28, 17, 16, 57, 14, 1, 18, 1, 14, 1, 2, 57, 24, 13, 20, 57, 0, 2, 1, 2, 17, 0, 17, 2, 61, 0, 5, 16, 1, 28, 25, 0, 41, 2, 117, 56, 25, 0, 33, 2, 1, 2, 117, 
    52, 201, 48, 77, 0, 121, 40, 1, 0, 205, 8, 1, 0, 1, 12, 213, 4, 13, 12, 253, 253, 253, 141
};

// Koordinátarendszer váltó függvények
// Mercator vetületi (u,v) koordinátákból gömbi (hosszúság, szélesség) radiánban
vec2 MercatorToSpherical(float u, float v) {
    // u: [0,1] -> hosszúság: [-180°, 180°]
    // v: [0,1] -> szélesség: [-85°, 85°]
    float longitude = (u - 0.5f) * 2.0f * 180.0f; // hosszúság fokban
    float latitude = (v - 0.5f) * 2.0f * 85.0f;   // szélesség fokban
    
    // Átváltás radiánba
    float lon = longitude * PI / 180.0f;
    float lat = latitude * PI / 180.0f;
    
    return vec2(lon, lat);
}

// Gömbi (hosszúság, szélesség) koordinátákból Mercator vetületi (u,v) koordinátákba
vec2 SphericalToMercator(float lon, float lat) {
    // lon: [-π, π] -> u: [0, 1]
    // lat: [-π/2, π/2] -> v: [0, 1]
    
    // A szélesség csak [-85°, 85°] közötti lehet a Mercator vetületben
    const float MAX_LAT = 85.0f * PI / 180.0f;
    lat = fmax(-MAX_LAT, fmin(MAX_LAT, lat));
    
    float u = (lon / PI / 2.0f) + 0.5f;
    float v = (lat / MAX_LAT / 2.0f) + 0.5f;
    
    return vec2(u, v);
}

// Gömbi (hosszúság, szélesség) koordinátákból 3D Descartes koordinátákba
vec3 SphericalToCartesian(float lon, float lat) {
    // Gömbi koordinátából 3D Descartes koordináta (egységsugarú gömbön)
    float x = cos(lat) * cos(lon);
    float y = cos(lat) * sin(lon);
    float z = sin(lat);
    
    return vec3(x, y, z);
}

// Overload a vec2 paraméteres verzióra
vec3 SphericalToCartesian(vec2 spherical) {
    return SphericalToCartesian(spherical.x, spherical.y);
}

// 3D Descartes koordinátákból gömbi (hosszúság, szélesség) koordinátákba
vec2 CartesianToSpherical(vec3 p) {
    // Normalizáljuk, hogy biztosan a gömbön legyen
    p = normalize(p);
    
    float lat = asin(p.z);
    float lon = atan2(p.y, p.x);
    
    return vec2(lon, lat);
}

// Pixel koordinátákból Mercator vetületi (u,v) koordinátákba
vec2 PixelToMercator(int x, int y) {
    float u = (float)x / winWidth;
    float v = 1.0f - (float)y / winHeight; // Y tengely fordított
    return vec2(u, v);
}

// Mercator vetületi (u,v) koordinátákból Pixel koordinátákba
vec2 MercatorToPixel(float u, float v) {
    int x = (int)(u * winWidth);
    int y = (int)((1.0f - v) * winHeight); // Y tengely fordított
    return vec2(x, y);
}

// Két pont közötti legrövidebb út számítása a gömbön (Nagy kör ív)
std::vector<vec3> CalculateGreatCirclePoints(vec3 p1, vec3 p2, int segments) {
    std::vector<vec3> points;
    points.reserve(segments + 1);
    
    // Gömbön mozgó pont interpolációja
    for (int i = 0; i <= segments; i++) {
        float t = (float)i / segments;
        
        // Lineáris interpoláció
        vec3 p = p1 * (1.0f - t) + p2 * t;
        // Vetítés vissza a gömb felületére
        p = normalize(p);
        
        points.push_back(p);
    }
    
    return points;
}

// Vertex shader
const char* vertexSource = R"(
    #version 330
    precision highp float;

    uniform mat4 MVP;
    layout(location = 0) in vec2 vp;       // position
    layout(location = 1) in vec2 vertexUV; // texture coordinates
    
    out vec2 texCoord;
    
    void main() {
        gl_Position = vec4(vp.x, vp.y, 0, 1) * MVP;
        texCoord = vertexUV;
    }
)";

// Fragment shader
const char* fragmentSource = R"(
    #version 330
    precision highp float;
    
    uniform sampler2D textureUnit;
    uniform float isDayTime;
    
    in vec2 texCoord;
    out vec4 fragmentColor;
    
    void main() {
        vec4 texColor = texture(textureUnit, texCoord);
        if (isDayTime < 0.5) {
            // Éjszaka esetén 50%-kal sötétítünk
            fragmentColor = texColor * 0.5;
        } else {
            fragmentColor = texColor;
        }
    }
)";

// Közös ősosztály minden objektumnak
class Object {
protected:
    unsigned int vao, vbo;
    vec3 color;
    
public:
    Object() {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vbo);
    }
    
    virtual void Draw(GPUProgram* gpuProgram) = 0;
    
    virtual ~Object() {
        glDeleteBuffers(1, &vbo);
        glDeleteVertexArrays(1, &vao);
    }
};

// Mercator térkép osztály
class Map : public Object {
private:
    unsigned int textureId;
    std::vector<vec4> decodedImage;
    
public:
    Map() {
        // A térképet lefedő négyzet csúcsai és textúra koordinátái
        float vertices[] = {
            // Pozíció (x,y)     // Textúra koordináták (u,v)
            -1.0f, -1.0f,        0.0f, 0.0f,  // bal alsó
             1.0f, -1.0f,        1.0f, 0.0f,  // jobb alsó
             1.0f,  1.0f,        1.0f, 1.0f,  // jobb felső
            -1.0f,  1.0f,        0.0f, 1.0f   // bal felső
        };
        
        // Vertex buffer feltöltése
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        
        // Attribútumok beállítása
        // Pozíció attribútum
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
        
        // Textúra koordináta attribútum
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        
        // Kép dekódolása
        DecodeImage();
        
        // Textúra létrehozása
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);
        
        // Textúra feltöltése a dekódolt képpel
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureWidth, textureHeight, 0, GL_RGBA, GL_FLOAT, &decodedImage[0]);
        
        // Textúra paraméterek beállítása
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    
    void DecodeImage() {
    // Dekódoljuk a képet a helyes RLE algoritmussal
    decodedImage.resize(textureWidth * textureHeight);
    
    // Színtábla definiálása
    vec4 colorTable[4] = {
        vec4(1.0f, 1.0f, 1.0f, 1.0f), // fehér
        vec4(0.0f, 0.0f, 1.0f, 1.0f), // kék
        vec4(0.0f, 1.0f, 0.0f, 1.0f), // zöld
        vec4(0.0f, 0.0f, 0.0f, 1.0f)  // fekete
    };
    
    int pixel_index = 0;
    
    // Minden bájtot feldolgozunk
    for (unsigned int i = 0; i < sizeof(mapData) && pixel_index < textureWidth * textureHeight; i++) {
        unsigned char byte = mapData[i];
        
        // Felső 6 bit a hossz, alsó 2 bit a szín
        int length = byte >> 2;       // A hossz (a felső 6 bit)
        int colorIndex = byte & 0x3;  // A szín (az alsó 2 bit)
        
        // Az adott színt length+1 alkalommal ismételjük
        for (int j = 0; j <= length && pixel_index < textureWidth * textureHeight; j++) {
            decodedImage[pixel_index++] = colorTable[colorIndex];
        }
    }
    
    // Ha nem sikerült kitölteni a teljes képet, a maradékot feketével töltjük
    while (pixel_index < textureWidth * textureHeight) {
        decodedImage[pixel_index++] = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}
    
    void Draw(GPUProgram* gpuProgram) override {
        // Aktív textúra beállítása
        int samplerUnit = 0;
        gpuProgram->setUniform(samplerUnit, "textureUnit");
        
        glActiveTexture(GL_TEXTURE0 + samplerUnit);
        glBindTexture(GL_TEXTURE_2D, textureId);
        
        // Rendering
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }
    
    ~Map() {
        glDeleteTextures(1, &textureId);
    }
};

// Út osztály - töröttvonal a gömbi geometria szerint
class Path : public Object {
private:
    std::vector<vec2> points; // Töröttvonal pontjai Mercator koordinátákban (u,v)
    
public:
    Path() {
        color = vec3(1.0f, 1.0f, 0.0f); // Sárga szín
    }
    
    void AddPoint(vec2 point) {
        points.push_back(point);
        
        // Ha legalább két pont van, akkor tudunk már vonalszakaszt rajzolni
        if (points.size() >= 2) {
            // Előre foglaljunk helyet 100 pontnak (majd később bővítjük, ha kell)
            std::vector<float> lineVertices;
            lineVertices.reserve(100 * 2);
            
            // Gömbi geometria szerinti legrövidebb út számítása
            CalculateGreatCircleRoute(lineVertices);
            
            // Buffer frissítése
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, lineVertices.size() * sizeof(float), lineVertices.data(), GL_STATIC_DRAW);
            
            // Pozíció attribútum beállítása
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
        }
    }
    
    void CalculateGreatCircleRoute(std::vector<float>& vertices) {
        // Az utolsó két pont között számítjuk a legrövidebb utat
        int idx = points.size() - 2;
        vec2 uv1 = points[idx];
        vec2 uv2 = points[idx + 1];
        
        // Átváltás gömbi koordinátákra
        vec2 spherical1 = MercatorToSpherical(uv1.x, uv1.y);
        vec2 spherical2 = MercatorToSpherical(uv2.x, uv2.y);
        
        // Átváltás 3D Descartes koordinátákba
        vec3 p1 = SphericalToCartesian(spherical1);
        vec3 p2 = SphericalToCartesian(spherical2);
        
        // Köztes pontok számítása (a töröttvonalhoz 100 pont)
        const int segments = 100;
        std::vector<vec3> greatCirclePoints = CalculateGreatCirclePoints(p1, p2, segments);
        
        // Pontok átalakítása és hozzáadása a kimeneti tömbhöz
        for (const vec3& p : greatCirclePoints) {
            // Vissza gömbi koordinátákra
            vec2 spherical = CartesianToSpherical(p);
            
            // Vissza Mercator vetületre
            vec2 uv = SphericalToMercator(spherical.x, spherical.y);
            
            // 2D koordináták a képernyőn (-1..1 tartomány)
            float x = uv.x * 2.0f - 1.0f;
            float y = uv.y * 2.0f - 1.0f;
            
            vertices.push_back(x);
            vertices.push_back(y);
        }
    }
    
    void Draw(GPUProgram* gpuProgram) override {
        // Ha nincs elég pont, nem rajzolunk semmit
        if (points.size() < 2) return;
        
        // Szín beállítása
        gpuProgram->setUniform(color, "color");
        
        // Rendering
        glBindVertexArray(vao);
        glLineWidth(3.0); // Vastagabb vonal
        glDrawArrays(GL_LINE_STRIP, 0, 101); // 100 szakasz = 101 pont
    }
};

// Állomás osztály - pont a térképen
class Station : public Object {
private:
    vec2 position; // Állomás pozíciója Mercator koordinátákban (u,v)
    
public:
    Station(vec2 pos) {
        position = pos;
        color = vec3(1.0f, 0.0f, 0.0f); // Piros szín
        
        // Pontot reprezentáló vertex (x,y)
        float vertices[] = {
            position.x * 2.0f - 1.0f, position.y * 2.0f - 1.0f
        };
        
        // Buffer feltöltése
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        
        // Pozíció attribútum beállítása
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    }
    
    void Draw(GPUProgram* gpuProgram) override {
        // Szín beállítása
        gpuProgram->setUniform(color, "color");
        
        // Rendering
        glBindVertexArray(vao);
        glPointSize(10.0f); // 10 pixeles pont
        glDrawArrays(GL_POINTS, 0, 1);
    }
    
    vec2 GetPosition() const {
        return position;
    }
};

// Saját alkalmazás osztály
class MercatorMapApp : public glApp {
    Map* map;
    Path* path;
    std::vector<Station*> stations;
    GPUProgram* gpuProgram;
    bool isDayTime;
    
public:
    MercatorMapApp() : glApp("Mercator Map") {
        map = nullptr;
        path = nullptr;
        gpuProgram = nullptr;
        isDayTime = true; // Alapértelmezetten nappal van
    }
    
    void onInitialization() override {
        glViewport(0, 0, winWidth, winHeight);
        
        // Shader programok létrehozása
        gpuProgram = new GPUProgram(vertexSource, fragmentSource);
        
        // Térkép létrehozása
        map = new Map();
        // Út létrehozása
       path = new Path();
       
       // A uniform változók kezdeti beállítása
       gpuProgram->setUniform((float)isDayTime, "isDayTime");
       
       // MVP mátrix (most identitás, mert nem transzformáljuk a képet)
       mat4 mvp = mat4(1.0f);
       gpuProgram->setUniform(mvp, "MVP");
   }
   
   void onDisplay() override {
       // Háttér törlése
       glClearColor(0, 0, 0, 0);
       glClear(GL_COLOR_BUFFER_BIT);
       
       // Shader program aktiválása
       gpuProgram->Use();
       
       // Napszak beállítása
       gpuProgram->setUniform((float)isDayTime, "isDayTime");
       
       // Térkép rajzolása
       map->Draw(gpuProgram);
       
       // Út rajzolása
       path->Draw(gpuProgram);
       
       // Állomások rajzolása
       for (auto station : stations) {
           station->Draw(gpuProgram);
       }
   }
   
   void onKeyboard(int key) override {
       // 'n' gomb: napszak váltás
       if (key == 'n') {
           isDayTime = !isDayTime;
           refreshScreen();
       }
   }
   
   void onMousePressed(MouseButton button, int pX, int pY) override {
       // Egér bal gomb: új állomás hozzáadása
       if (button == MOUSE_LEFT) {
           // Képernyő koordinátából normalizált koordináta (0..1)
           vec2 mercator = PixelToMercator(pX, pY);
           
           // Új állomás létrehozása
           Station* station = new Station(mercator);
           stations.push_back(station);
           
           // Ha már van legalább egy állomás, akkor az utat is frissítjük
           if (stations.size() > 1) {
               path->AddPoint(stations[stations.size() - 2]->GetPosition());
               path->AddPoint(stations[stations.size() - 1]->GetPosition());
           }
           
           refreshScreen();
       }
   }
   
   ~MercatorMapApp() {
       delete gpuProgram;
       delete map;
       delete path;
       for (auto station : stations) {
           delete station;
       }
   }
};

// Alkalmazás példányosítása
MercatorMapApp app;