//=============================================================================================
// Mercator vetületű világtérkép a gömb alakú Földön legrövidebb utakkal
//=============================================================================================
#include "./framework.h"

// A képhez használt konstansok
const int winWidth = 600, winHeight = 600;
const int textureWidth = 64, textureHeight = 64;
const float PI = 3.141592653589793f;

// Föld paraméterek
const float EARTH_RADIUS = 6371.0f; // Föld sugara km-ben
const float EARTH_CIRCUMFERENCE = 40000.0f; // Föld kerülete km-ben
const float AXIS_TILT = 23.0f * PI / 180.0f; // Tengely ferdeség radianban

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

// Globális változók az idő kezeléséhez
int currentHour = 0;     // Az aktuális óra (0-23)
int currentDay = 172;    // Nyári napforduló napja (kb. június 21)

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

// Két pont közötti távolság számítása a gömbön
float CalculateDistance(vec3 p1, vec3 p2) {
    // A két pont közti szög a gömb középpontjából nézve
    float dotProduct = dot(p1, p2);
    // Korrigáljuk a numerikus pontatlanságokat
    dotProduct = fmax(-1.0f, fmin(1.0f, dotProduct));
    float angle = acos(dotProduct);
    
    // A távolság a szög és a Föld sugarának szorzata
    float distanceKm = angle * EARTH_RADIUS;
    
    return distanceKm;
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

// Napszak meghatározása egy adott koordinátán
bool isDaytime(vec2 spherical) {
    // A Nap deklinációja (nyári napfordulón +23.5°)
    float declination = AXIS_TILT * cos((float(currentDay) - 172.0) * 2.0 * 3.14159265359 / 365.0);
    
    // A Nap iránya a világűrben (egységvektor)
    vec3 sunDirection;
    
    // Nyári napfordulón 0 órakor:
    // A nap a (0,-1,0) irányban van (nyugati oldalon), és declination szöggel emelkedik az egyenlítő fölé
    float hourAngle = float(currentHour) / 24.0 * 2.0 * 3.14159265359;
    
    // Számítás a helyes 3D koordináta-rendszerben
    sunDirection.x = -cos(hourAngle);
    sunDirection.y = -sin(hourAngle);
    sunDirection.z = 0.0;
    
    // Elforgatás a deklinációval
    vec3 rotatedSun;
    float cosDecl = cos(declination);
    float sinDecl = sin(declination);
    rotatedSun.x = sunDirection.x;
    rotatedSun.y = sunDirection.y * cosDecl - sunDirection.z * sinDecl;
    rotatedSun.z = sunDirection.y * sinDecl + sunDirection.z * cosDecl;
    
    // A felületi normálvektor
    vec3 surfaceNormal = SphericalToCartesian(spherical);
    
    // Ha a skaláris szorzat pozitív, akkor a felület a Nap felé néz (nappal van)
    return dot(surfaceNormal, rotatedSun) > 0.0;
}

// Vertex shader
const char* vertexSource = R"(
    #version 330
    precision highp float;

    uniform mat4 MVP;
    layout(location = 0) in vec2 vp;       // position
    layout(location = 1) in vec2 vertexUV; // texture coordinates
    
    out vec2 texCoord;
    out vec2 mercatorPos;
    
    void main() {
        gl_Position = vec4(vp.x, vp.y, 0, 1) * MVP;
        texCoord = vertexUV;
        mercatorPos = vertexUV; // Mercator koordináták továbbítása
    }
)";

// Fragment shader
const char* fragmentSource = R"(
    #version 330
    precision highp float;
    
    uniform sampler2D textureUnit;
    uniform int objectType;      // 0 = térkép, 1 = út, 2 = állomás
    uniform vec3 color;          // Szín (út, állomás)
    
    // Napszakszámításhoz szükséges paraméterek
    uniform int currentHour;     // 0-23 óra
    uniform int currentDay;      // 172 = nyári napforduló
    uniform float axisTilt;      // 23 fok radiánban (kb 0.4 radián)
    
    in vec2 texCoord;           // Textúra koordináták
    out vec4 fragmentColor;
    
    const float PI = 3.14159265359;
    
    // Gömbi -> 3D koordináta konverzió
    vec3 sphericalToCartesian(vec2 spherical) {
        float lon = spherical.x;
        float lat = spherical.y;
        
        float x = cos(lat) * cos(lon);
        float y = cos(lat) * sin(lon);
        float z = sin(lat);
        
        return vec3(x, y, z);
    }
    
    // Mercator -> gömbi koordináta konverzió
    vec2 mercatorToSpherical(vec2 mercator) {
        // u: [0,1] -> hosszúság: [-180°, 180°]
        // v: [0,1] -> szélesség: [-85°, 85°]
        float longitude = (mercator.x - 0.5) * 2.0 * 180.0;  // fokban
        float latitude = (mercator.y - 0.5) * 2.0 * 85.0;    // fokban
        
        // Átváltás radiánba
        float lon = longitude * PI / 180.0;
        float lat = latitude * PI / 180.0;
        
        return vec2(lon, lat);
    }
    
    // Napszak számítás
    bool isDaytime(vec2 mercator) {
        // Mercator -> gömbi koordináták konvertálása
        vec2 spherical = mercatorToSpherical(mercator);
        
        // A ponthoz tartozó felületi normálvektor (a gömb adott pontján)
        vec3 surfaceNormal = sphericalToCartesian(spherical);
        
        // Nap állása a világtérben
        // A Nap deklinációja (nyári napfordulón +23°)
        float declination = axisTilt;
        
        // Az óra határozza meg a Nap helyzetét a forgás szerint
        float hourAngle = float(currentHour) / 24.0 * 2.0 * PI;
        
        // Nap irányvektora
        vec3 sunDirection;
        sunDirection.x = -cos(hourAngle); // -cos mert 0 órakor nyugat felé áll
        sunDirection.y = -sin(hourAngle); // -sin mert az óra növekedésével kelet felé halad
        sunDirection.z = sin(declination); // A deklinációnak megfelelő magasság
        
        // Normalizálás
        sunDirection = normalize(sunDirection);
        
        // A nappal/éjszaka eldöntése: ha a skaláris szorzat pozitív, akkor a felület a Nap felé néz
        return dot(surfaceNormal, sunDirection) > 0.0;
    }
    
    void main() {
        if (objectType == 0) { // Térkép
            // Térkép textúra beolvasása
            vec4 texColor = texture(textureUnit, texCoord);
            
            // Napszak meghatározása az aktuális pixelre
            bool daytime = isDaytime(texCoord);
            
            // Nappal vagy éjszaka szerinti színezés
            if (!daytime) {
                // Éjszaka 50%-kal sötétebb
                fragmentColor = texColor * 0.5;
            } else {
                fragmentColor = texColor;
            }
        } else { // Út vagy állomás
            fragmentColor = vec4(color, 1.0);
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
        // Objektum típus beállítása (0 = térkép)
        gpuProgram->setUniform(0, "objectType");
        
        // Textúra beállítása
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
    bool initialized;  // Az első két pont már megvan?
    std::vector<std::vector<float>> lineSegments; // Minden szakasz adatai
    std::vector<float> distances; // Távolságok km-ben
    
public:
    Path() {
        color = vec3(1.0f, 1.0f, 0.0f); // Sárga szín
        initialized = false;
    }
    
    // Két pont között útvonal számítása
    void AddSegment(vec2 startPos, vec2 endPos) {
        // Átváltás gömbi koordinátákra
        vec2 spherical1 = MercatorToSpherical(startPos.x, startPos.y);
        vec2 spherical2 = MercatorToSpherical(endPos.x, endPos.y);
        
        // Átváltás 3D Descartes koordinátákba
        vec3 p1 = SphericalToCartesian(spherical1);
        vec3 p2 = SphericalToCartesian(spherical2);
        
        // Távolság számítása
        float distance = CalculateDistance(p1, p2);
        distances.push_back(distance);
        
        // Kiírás a konzolra
        printf("Distance: %.0f km\n", distance);
        
        // Köztes pontok számítása (a töröttvonalhoz 100 pont)
        const int segments = 100;
        std::vector<vec3> greatCirclePoints = CalculateGreatCirclePoints(p1, p2, segments);
        
        // Szakasz adatainak tárolása
        std::vector<float> lineVertices;
        lineVertices.reserve((segments + 1) * 2);
        
        // Pontok átalakítása és hozzáadása a kimeneti tömbhöz
        for (const vec3& p : greatCirclePoints) {
            // Vissza gömbi koordinátákra
            vec2 spherical = CartesianToSpherical(p);
            
            // Vissza Mercator vetületre
            vec2 uv = SphericalToMercator(spherical.x, spherical.y);
            
            // 2D koordináták a képernyőn (-1..1 tartomány)
            float x = uv.x * 2.0f - 1.0f;
            float y = uv.y * 2.0f - 1.0f;
            
            lineVertices.push_back(x);
            lineVertices.push_back(y);
        }
        
        // Hozzáadjuk a szakaszt a tárolóhoz
        lineSegments.push_back(lineVertices);
        
        // Az első szakasz után inicializálva van
        initialized = true;
    }
    
    void Draw(GPUProgram* gpuProgram) override {
        // Ha nincs útvonal, nem rajzolunk semmit
        if (!initialized || lineSegments.empty()) return;
        
        // Objektum típus beállítása (1 = út)
        gpuProgram->setUniform(1, "objectType");
        
        // Szín beállítása
        gpuProgram->setUniform(color, "color");
        
        // Minden szakaszt külön rajzolunk
        glBindVertexArray(vao);
        glLineWidth(3.0); // Vastagabb vonal
        
        for (const auto& segment : lineSegments) {
            // Buffer feltöltése az aktuális szakasszal
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, segment.size() * sizeof(float), segment.data(), GL_STATIC_DRAW);
            
            // Pozíció attribútum beállítása
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
            
            // Szakasz rajzolása
            glDrawArrays(GL_LINE_STRIP, 0, (int)(segment.size() / 2));
        }
    }
    
    // Teljes távolság km-ben
    float GetTotalDistance() const {
        float total = 0.0f;
        for (float d : distances) {
            total += d;
        }
        return total;
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
        // Objektum típus beállítása (2 = állomás)
        gpuProgram->setUniform(2, "objectType");
        
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
    
public:
    MercatorMapApp() : glApp("Mercator Map") {
        map = nullptr;
        path = nullptr;
        gpuProgram = nullptr;
    }
    
    void onInitialization() override {
        glViewport(0, 0, winWidth, winHeight);
        
        // Shader programok létrehozása
        gpuProgram = new GPUProgram(vertexSource, fragmentSource);
        
        // Térkép létrehozása
        map = new Map();
        
        // Út létrehozása
        path = new Path();
    
        // Az idő kezdeti beállítása (nyári napforduló, 0 óra GMT)
        currentHour = 0;
        currentDay = 172; // kb. június 21
        
        // Shader uniform változók kezdeti beállítása
        gpuProgram->setUniform(currentHour, "currentHour");
        gpuProgram->setUniform(currentDay, "currentDay");
        gpuProgram->setUniform(AXIS_TILT, "axisTilt");  // A tengelyferdeség radiánban
        
        // MVP mátrix (most identitás, mert nem transzformáljuk a képet)
        mat4 mvp = mat4(1.0f);
        gpuProgram->setUniform(mvp, "MVP");
        
        printf("Kezdeti idő: %d. nap, %02d:00 GMT (nyári napforduló)\n", currentDay, currentHour);
}
    
    void onDisplay() override {
    // Háttér törlése
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Shader program aktiválása
    gpuProgram->Use();
    
    // Idő beállítása a shadernek
    gpuProgram->setUniform(currentHour, "currentHour");
    gpuProgram->setUniform(currentDay, "currentDay");
    gpuProgram->setUniform(AXIS_TILT, "axisTilt");
    
    // Térkép rajzolása
    gpuProgram->setUniform(0, "objectType");
    map->Draw(gpuProgram);
    
    // Út rajzolása
    gpuProgram->setUniform(1, "objectType");
    path->Draw(gpuProgram);
    
    // Állomások rajzolása
    gpuProgram->setUniform(2, "objectType");
    for (auto station : stations) {
        station->Draw(gpuProgram);
    }
}
    
    void onKeyboard(int key) override {
        // 'n' gomb: óránként léptetjük az időt
        if (key == 'n') {
            currentHour = (currentHour + 1) % 24;
            printf("Aktuális idő: %d. nap, %02d:00 GMT\n", currentDay, currentHour);
            refreshScreen();
        }
    }
    
    void onMousePressed(MouseButton button, int pX, int pY) override {
        // Egér bal gomb: új állomás hozzáadása
        if (button == MOUSE_LEFT) {
            // Képernyő koordinátából normalizált koordináta (0..1)
            vec2 mercator = PixelToMercator(pX, pY);
            
            // Új állomás létrehozása és tárolása
            Station* station = new Station(mercator);
            stations.push_back(station);
            
            // Ha már van legalább két állomás, akkor új útvonalszakasz hozzáadása
            if (stations.size() >= 2) {
                // Az utolsó előtti és utolsó állomás közötti szakasz
                vec2 prevPos = stations[stations.size() - 2]->GetPosition();
                vec2 currPos = stations[stations.size() - 1]->GetPosition();
                
                // Útvonal szakasz hozzáadása
                path->AddSegment(prevPos, currPos);
                
                // Teljes távolság kiírása
                printf("Teljes távolság: %.0f km\n", path->GetTotalDistance());
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