#include "framework.h"
#include "glad/glad.h"


const int winWidth = 600, winHeight = 600;
// Vertex shader: Csúcspont pozíciójának egyszerű továbbítása
const char* vertexSource = R"(
    #version 330
    precision highp float;
    layout(location = 0) in vec3 vp;    // Csúcspont pozíciója
    
    void main() {
        gl_Position = vec4(vp.x, vp.y, 0, 1);    // A csúcspont pozíciója homogén koordinátákban
        gl_PointSize = 10.0;            // Pontméret 10-es
    }
)";

// Fragment shader: Konstans szín kibocsátása
const char* fragmentSource = R"(
    #version 330
    precision highp float;
    uniform vec3 color;
    out vec4 outColor;
    
    void main() {
        outColor = vec4(color, 1);    // A kibocsátott szín - homogén koordinátákban
    }
)";

// Állapotok a program működéséhez
enum ProgramState {
    POINT_DRAWING,   // 'p' - Pont rajzolás
    LINE_DRAWING,    // 'l' - Egyenes rajzolás
    LINE_MOVING,     // 'm' - Egyenes mozgatás
    INTERSECTION     // 'i' - Metszéspont számítás
};

// Object osztály - Geometriai objektumok tárolására és megjelenítésére
class Object {
protected:
    unsigned int vao, vbo;      // Vertex Array Object és Vertex Buffer Object
    std::vector<vec3> vertices; // Csúcspontok (x, y, z=1)

public:
    Object() {
        // VAO és VBO létrehozása
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        
        // Pozíció attribútum beállítása
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    }

    // CPU-n tárolt csúcspontok frissítése a GPU-n
    void updateGPU() {
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vec3), vertices.data(), GL_DYNAMIC_DRAW);
    }

    // GPU-n tárolt geometria kirajzolása
    void draw(GPUProgram* gpuProgram, GLenum primitiveType, const vec3& color) {
        if (vertices.size() > 0) {
            gpuProgram->setUniform(color, "color");
            glBindVertexArray(vao);
            glDrawArrays(primitiveType, 0, vertices.size());
        }
    }

    virtual ~Object() {
        glDeleteBuffers(1, &vbo);
        glDeleteVertexArrays(1, &vao);
    }
};

// PointCollection osztály - pontok tárolására és manipulálására
class PointCollection : public Object {
public:
    // Új pont hozzáadása
    void addPoint(const vec3& point) {
        vertices.push_back(point);
        updateGPU();
        printf("Point %g, %g added\n", point.x, point.y);
    }

    // Legközelebbi pont keresése
    int findClosestPoint(const vec3& position, float threshold = 0.05f) {
        float minDist = threshold;
        int closestIdx = -1;
        
        for (int i = 0; i < vertices.size(); i++) {
            float dist = length(vec2(vertices[i].x - position.x, vertices[i].y - position.y));
            if (dist < minDist) {
                minDist = dist;
                closestIdx = i;
            }
        }
        
        return closestIdx;
    }

    // Pontok kirajzolása
    void drawPoints(GPUProgram* gpuProgram, const vec3& color) {
        draw(gpuProgram, GL_POINTS, color);
    }

    // Adott indexű pont lekérdezése
    vec3 getPoint(int idx) const {
        if (idx >= 0 && idx < vertices.size()) {
            return vertices[idx];
        }
        return vec3(0, 0, 0);
    }

    // Pontok számának lekérdezése
    size_t size() const {
        return vertices.size();
    }
};

// Line osztály - egy egyenes geometriai és matematikai tulajdonságainak tárolása
class Line {
private:
    vec3 p1, p2;          // Az egyenest meghatározó két pont
    vec3 point;           // Egy pont az egyenesen
    vec3 direction;       // Az egyenes irányvektora
    vec3 normal;          // Az egyenes normálvektora
    float a, b, c;        // Az implicit egyenlet együtthatói (ax + by + c = 0)

public:
    // Két pont alapján egyenes konstruálása
    Line(const vec3& point1, const vec3& point2) {
        p1 = point1;
        p2 = point2;
        
        // Paraméteres egyenlet számítása
        point = point1;
        direction = normalize(vec3(point2.x - point1.x, point2.y - point1.y, 0));
        
        // Implicit egyenlet számítása (ax + by + c = 0)
        normal = vec3(-direction.y, direction.x, 0);  // Normálvektor: merőleges az irányvektorra
        a = normal.x;
        b = normal.y;
        c = -(a * point.x + b * point.y);
        
        // Egyenlet kiírása a konzolra
        printf("Line added\n");
        printf("  Implicit: %g x + %g y + %g = 0\n", a, b, c);
        printf("  Parametric: r(t) = (%g, %g) + (%g, %g)t\n", 
               point.x, point.y, direction.x, direction.y);
    }

    // Két egyenes metszéspontja
    bool intersect(const Line& other, vec3& intersection) {
        // Ha a két egyenes párhuzamos, nincs metszéspont
        float det = a * other.b - other.a * b;
        if (fabs(det) < 1e-10) return false;
        
        // Metszéspont számítása az implicit egyenletekből
        intersection.x = (b * other.c - other.b * c) / det;
        intersection.y = (other.a * c - a * other.c) / det;
        intersection.z = 1.0f;
        
        return true;
    }

    // Pont távolsága az egyenestől
    float distanceToPoint(const vec3& p) const {
        return fabs(a * p.x + b * p.y + c) / sqrt(a * a + b * b);
    }

    // Egyenes eltolása, hogy átmenjen egy adott ponton
    void moveToPoint(const vec3& newPoint) {
        // Az irány nem változik, csak a c paramétert változtatjuk
        c = -(a * newPoint.x + b * newPoint.y);
        
        // Frissítsük a point-ot, hogy az új pontot használja
        point = newPoint;
        
        // Frissítsük a p1, p2 pontokat is
        p1 = newPoint;
        p2 = newPoint + direction;
        
        printf("Move\n");
    }

    // A képernyőre eső szakasz kiszámítása
    void getClippedLinePoints(vec3& outP1, vec3& outP2) const {
        // Szakasz végpontjainak meghatározása a (-1,-1)-(1,1) viewporthoz
        const float LARGE_VALUE = 10.0f;  // Elég nagy szám a biztos metszéshez
        
        vec3 farPoint1 = point - direction * LARGE_VALUE;
        vec3 farPoint2 = point + direction * LARGE_VALUE;
        
        // Clipping algoritmus a határok között
        bool p1Inside = clipPoint(farPoint1, outP1);
        bool p2Inside = clipPoint(farPoint2, outP2);
        
        // Ha egyik pont sincs belül, akkor nincs látható szakasz
        if (!p1Inside && !p2Inside) {
            // Mindkét pont kívül van, ellenőrizzük, hogy a szakasz metszi-e a viewport-ot
            // Egyszerűsítésként itt csak két pontot adunk vissza a (-1,-1) és (1,1) négyzet mentén
            outP1 = vec3(-1, -1, 1);
            outP2 = vec3(1, 1, 1);
        }
    }
    
    // Pont vágása a (-1,-1)-(1,1) határok közé
    bool clipPoint(const vec3& p, vec3& clipped) const {
        clipped = p;
        
        // x koordináta vágása
        if (p.x < -1.0f) {
            float t = (-1.0f - point.x) / direction.x;
            clipped = point + direction * t;
        }
        else if (p.x > 1.0f) {
            float t = (1.0f - point.x) / direction.x;
            clipped = point + direction * t;
        }
        
        // y koordináta vágása
        if (p.y < -1.0f) {
            float t = (-1.0f - point.y) / direction.y;
            vec3 intersection = point + direction * t;
            if (intersection.x >= -1.0f && intersection.x <= 1.0f) {
                clipped = intersection;
            }
        }
        else if (p.y > 1.0f) {
            float t = (1.0f - point.y) / direction.y;
            vec3 intersection = point + direction * t;
            if (intersection.x >= -1.0f && intersection.x <= 1.0f) {
                clipped = intersection;
            }
        }
        
        // Ellenőrizzük, hogy a pont a (-1,-1)-(1,1) négyzeten belül van-e
        return (clipped.x >= -1.0f && clipped.x <= 1.0f && 
                clipped.y >= -1.0f && clipped.y <= 1.0f);
    }
    
    // Getter metódusok
    const vec3& getPoint() const { return point; }
    const vec3& getDirection() const { return direction; }
    const vec3& getNormal() const { return normal; }
    float getA() const { return a; }
    float getB() const { return b; }
    float getC() const { return c; }
};

// LineCollection osztály - egyenesek tárolására és manipulálására
class LineCollection : public Object {
private:
    std::vector<Line> lines;         // Tárolt egyenesek
    int selectedLineIndex = -1;      // Kiválasztott egyenes indexe

public:
    // Új egyenes hozzáadása
    void addLine(const Line& line) {
        lines.push_back(line);
        
        // A megjelenítéshez frissítsük a vertices-t
        updateVerticesFromLines();
    }
    
    // Egyenes keresése egy adott ponthoz
    int findClosestLine(const vec3& position, float threshold = 0.05f) {
        float minDist = threshold;
        int closestIdx = -1;
        
        for (int i = 0; i < lines.size(); i++) {
            float dist = lines[i].distanceToPoint(position);
            if (dist < minDist) {
                minDist = dist;
                closestIdx = i;
            }
        }
        
        return closestIdx;
    }
    
    // Kiválasztott egyenes mozgatása
    void moveSelectedLine(const vec3& position) {
        if (selectedLineIndex >= 0 && selectedLineIndex < lines.size()) {
            lines[selectedLineIndex].moveToPoint(position);
            updateVerticesFromLines();
        }
    }
    
    // Egyenes kiválasztása
    void selectLine(int idx) {
        selectedLineIndex = idx;
    }
    
    // Kiválasztott egyenes lekérdezése
    int getSelectedLine() const {
        return selectedLineIndex;
    }
    
    // Két egyenes metszéspontja
    bool findIntersection(int lineIndex1, int lineIndex2, vec3& intersection) {
        if (lineIndex1 >= 0 && lineIndex1 < lines.size() && 
            lineIndex2 >= 0 && lineIndex2 < lines.size() &&
            lineIndex1 != lineIndex2) {
            
            bool hasIntersection = lines[lineIndex1].intersect(lines[lineIndex2], intersection);
            if (hasIntersection) {
                printf("Intersect\n");
                printf("Point %g, %g added\n", intersection.x, intersection.y);
            }
            return hasIntersection;
        }
        return false;
    }
    
    // Frissíti a vertices tömböt az egyenesek alapján
    void updateVerticesFromLines() {
        vertices.clear();
        
        for (const Line& line : lines) {
            vec3 p1, p2;
            line.getClippedLinePoints(p1, p2);
            
            vertices.push_back(p1);
            vertices.push_back(p2);
        }
        
        updateGPU();
    }
    
    // Egyenesek kirajzolása
    void drawLines(GPUProgram* gpuProgram, const vec3& color) {
        draw(gpuProgram, GL_LINES, color);
    }
    
    // Egyenesek számának lekérdezése
    size_t size() const {
        return lines.size();
    }
    
    // Egy adott egyenes lekérdezése
    const Line& getLine(int idx) const {
        return lines[idx];
    }
};

// Fő alkalmazás osztály
class PointsAndLinesApp : public glApp {
private:
    GPUProgram gpuProgram;            // Shader program
    PointCollection* points;          // Pontgyűjtemény
    LineCollection* lines;            // Egyenesgyűjtemény
    ProgramState state;               // Aktuális program állapot
    
    int selectedPoint1 = -1;          // Egyenes rajzolásához kiválasztott első pont
    int selectedLine1 = -1;           // Metszésponthoz kiválasztott első egyenes

public:
    PointsAndLinesApp() : glApp("Pontok és egyenesek") {
        state = POINT_DRAWING;
    }
    
    void onInitialization() override {
        // Shader program létrehozása
        gpuProgram.create(vertexSource, fragmentSource);
        
        // Objektumok létrehozása
        points = new PointCollection();
        lines = new LineCollection();
        
        // Háttérszín beállítása (szürke)
        glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
        
        // Pont méret és vonal vastagság beállítása
        glPointSize(10.0f);
        glLineWidth(3.0f);
    }
    
    void onDisplay() override {
        // Képernyő törlése
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Pontok kirajzolása (piros színnel)
        points->drawPoints(&gpuProgram, vec3(1, 0, 0));
        
        // Egyenesek kirajzolása (cián színnel)
        lines->drawLines(&gpuProgram, vec3(0, 1, 1));
    }
    
    void onKeyboard(int key) override {
        // Állapot váltás a billentyű alapján
        if (key == 'p') {
            state = POINT_DRAWING;
            printf("Point drawing mode\n");
        }
        else if (key == 'l') {
            state = LINE_DRAWING;
            selectedPoint1 = -1;
            printf("Line drawing mode\n");
        }
        else if (key == 'm') {
            state = LINE_MOVING;
            lines->selectLine(-1);
            printf("Line moving mode\n");
        }
        else if (key == 'i') {
            state = INTERSECTION;
            selectedLine1 = -1;
            printf("Intersection mode\n");
        }
    }
    
    // Pixelkoordináták átalakítása normalizált eszközkoordinátákká
    vec3 pixelToNDC(int pX, int pY) {
        // Használd a glApp osztályból örökölt winWidth és winHeight változókat
        float x = 2.0f * pX / (float)winWidth - 1.0f;
        float y = 1.0f - 2.0f * pY / (float)winHeight;  // Y tengely megfordítása
        return vec3(x, y, 1.0f);
    }
    
    void onMousePressed(MouseButton button, int pX, int pY) override {
    if (button != MOUSE_LEFT) return;
    
    // Pixel koordináták konvertálása normalizált eszközkoordinátákká
    vec3 position = pixelToNDC(pX, pY);
    
    // Változók előre deklarálása
    int closestPoint = -1;
    int closestLine = -1;
    
    switch (state) {
        case POINT_DRAWING: {
            // Új pont hozzáadása
            points->addPoint(position);
            break;
        }
        case LINE_DRAWING: {
            // Legközelebbi pont keresése
            closestPoint = points->findClosestPoint(position);
            if (closestPoint >= 0) {
                if (selectedPoint1 < 0) {
                    // Első pont kiválasztása
                    selectedPoint1 = closestPoint;
                } else if (selectedPoint1 != closestPoint) {
                    // Második pont kiválasztása és egyenes létrehozása
                    Line newLine(points->getPoint(selectedPoint1), points->getPoint(closestPoint));
                    lines->addLine(newLine);
                    selectedPoint1 = -1;  // Visszaállítás a következő egyeneshez
                }
            }
            break;
        }
        case LINE_MOVING: {
            // Egyenes kiválasztása mozgatáshoz
            closestLine = lines->findClosestLine(position);
            lines->selectLine(closestLine);
            if (closestLine >= 0) {
                // Egyenes mozgatása a kattintás helyére
                lines->moveSelectedLine(position);
            }
            break;
        }
        case INTERSECTION: {
            // Egyenes kiválasztása metszésponthoz
            int lineIdx = lines->findClosestLine(position);
            if (lineIdx >= 0) {
                if (selectedLine1 < 0) {
                    // Első egyenes kiválasztása
                    selectedLine1 = lineIdx;
                } else if (selectedLine1 != lineIdx) {
                    // Második egyenes kiválasztása és metszéspont számítása
                    vec3 intersection;
                    if (lines->findIntersection(selectedLine1, lineIdx, intersection)) {
                        points->addPoint(intersection);
                    }
                    selectedLine1 = -1;  // Visszaállítás
                }
            }
            break;
        }
    }
    
    // Képernyő frissítése
    refreshScreen();
}
    
    void onMouseMotion(int pX, int pY) override {
        // Csak mozgatás módban van jelentősége
        if (state == LINE_MOVING && lines->getSelectedLine() >= 0) {
            vec3 position = pixelToNDC(pX, pY);
            lines->moveSelectedLine(position);
            refreshScreen();
        }
    }
    
    void onMouseReleased(MouseButton button, int pX, int pY) override {
        // Mozgatás befejezése
        if (state == LINE_MOVING && button == MOUSE_LEFT) {
            lines->selectLine(-1);
        }
    }
    
    ~PointsAndLinesApp() {
        delete points;
        delete lines;
    }
};

// Fő program - alkalmazás példányosítása
PointsAndLinesApp app;