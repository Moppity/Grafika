//=============================================================================================
// Hullámvasút szimuláció Catmull-Rom Spline-nal
//=============================================================================================
#include "./framework.h"

// csúcspont árnyaló
const char *vertSource = R"(
	#version 330				
    precision highp float;

	layout(location = 0) in vec2 cP;	// 0. bemeneti regiszter
    uniform mat4 MVP;              // Model-View-Projection transzformáció

	void main() {
		gl_Position = MVP * vec4(cP.x, cP.y, 0, 1); 	// transzformáció alkalmazása
	}
)";

// pixel árnyaló
const char *fragSource = R"(
	#version 330
    precision highp float;

	uniform vec3 color;			// konstans szín
	out vec4 fragmentColor;		// pixel szín

	void main() {
		fragmentColor = vec4(color, 1); // RGB -> RGBA
	}
)";

// Az egyszerűbb használat érdekében
using namespace glm;

const int winWidth = 600, winHeight = 600;
const float worldWidth = 20.0f;  // 20m széles világ
const float worldHeight = 20.0f; // 20m magas világ
const float g = 40.0f;           // nehézségi gyorsulás m/s^2-ben

// Állapot enumeráció a gondola állapotaihoz
enum GondolaState {
    WAITING,    // várakozik
    ROLLING,    // mozgásban
    FALLEN      // leesett
};

//---------------------------
// Camera osztály
class Camera {
private:
    vec2 center;         // kamera közepének pozíciója világkoordinátákban
    float width, height; // kamera által látott terület mérete világkoordinátákban
    mat4 viewMatrix;     // view transzformációs mátrix
    mat4 projMatrix;     // projekciós transzformációs mátrix
    mat4 invViewMatrix;  // view mátrix inverze
    mat4 invProjMatrix;  // projekciós mátrix inverze
    mat4 MVPMatrix;      // model-view-projection mátrix
    
    void updateMatrices() {
        // View mátrix számítása - eltolás a világ középpontjába
        viewMatrix = translate(mat4(1.0f), vec3(-center.x, -center.y, 0.0f));
        
        // Projekciós mátrix - skálázás a [-1,1] tartományba
        projMatrix = scale(mat4(1.0f), vec3(2.0f/width, 2.0f/height, 1.0f));
        
        // Inverz mátrixok számítása
        invViewMatrix = inverse(viewMatrix);
        invProjMatrix = inverse(projMatrix);
        
        // Teljes MVP mátrix számítása
        MVPMatrix = projMatrix * viewMatrix;
    }
    
public:
    Camera(const vec2& _center, float _width, float _height) : 
           center(_center), width(_width), height(_height) {
        updateMatrices();
    }
    
    // Világkoordináták átváltása képernyőkoordinátákra
    vec2 worldToScreen(const vec2& worldPos) {
        vec4 clipPos = MVPMatrix * vec4(worldPos, 0.0f, 1.0f);
        return vec2(clipPos.x, clipPos.y);
    }
    
    // Képernyőkoordináták átváltása világkoordinátákra
    vec2 screenToWorld(int screenX, int screenY) {
        // Normalizált eszközkoordináták számítása
        float ndcX = 2.0f * screenX / winWidth - 1.0f;
        float ndcY = 1.0f - 2.0f * screenY / winHeight;
        
        // Homogén koordináták számítása
        vec4 clipPos = vec4(ndcX, ndcY, 0.0f, 1.0f);
        
        // Visszatranszformálás világkoordinátákba
        vec4 worldPos = invViewMatrix * invProjMatrix * clipPos;
        
        return vec2(worldPos.x, worldPos.y);
    }
    
    // MVP mátrix lekérdezése
    const mat4& getMVP() const {
        return MVPMatrix;
    }
};

//---------------------------
// Spline osztály a hullámvasút pályájához
class Spline {
private:
    std::vector<vec2> controlPoints;      // kontrollpontok
    Geometry<vec2> *splineGeometry;       // görbe geometria
    Geometry<vec2> *pointGeometry;        // pontok geometria
    
    // Catmull-Rom spline kiértékelése
    vec2 CatmullRom(const vec2& p0, const vec2& p1, const vec2& p2, const vec2& p3, float t) {
        float t2 = t * t;
        float t3 = t2 * t;
        
        // Catmull-Rom mátrix
        float h1 = -0.5f * t3 + t2 - 0.5f * t;
        float h2 = 1.5f * t3 - 2.5f * t2 + 1.0f;
        float h3 = -1.5f * t3 + 2.0f * t2 + 0.5f * t;
        float h4 = 0.5f * t3 - 0.5f * t2;
        
        return h1 * p0 + h2 * p1 + h3 * p2 + h4 * p3;
    }
    
    // Catmull-Rom spline deriváltjának kiértékelése
    vec2 CatmullRomDerivative(const vec2& p0, const vec2& p1, const vec2& p2, const vec2& p3, float t) {
        float t2 = t * t;
        
        // A Catmull-Rom deriváltja
        float dh1 = -1.5f * t2 + 2.0f * t - 0.5f;
        float dh2 = 4.5f * t2 - 5.0f * t;
        float dh3 = -4.5f * t2 + 4.0f * t + 0.5f;
        float dh4 = 1.5f * t2 - 1.0f * t;
        
        return dh1 * p0 + dh2 * p1 + dh3 * p2 + dh4 * p3;
    }
    
    // Geometria frissítése
    void updateGeometry() {
        if (controlPoints.size() < 2) return;
        
        // Görbe pontok generálása
        std::vector<vec2> curvePoints;
        
        // Ha van legalább 2 kontrollpont, görbe generálása
        if (controlPoints.size() >= 2) {
            // 100 pontra felosztjuk a görbét
            const int segments = 100;
            
            for (int i = 0; i < controlPoints.size() - 1; i++) {
                // Meghatározzuk a 4 kontrollpontot a szakaszhoz
                vec2 p0 = (i > 0) ? controlPoints[i-1] : controlPoints[i];
                vec2 p1 = controlPoints[i];
                vec2 p2 = controlPoints[i+1];
                vec2 p3 = (i < controlPoints.size() - 2) ? controlPoints[i+2] : controlPoints[i+1];
                
                // A kezdő és végpontokban nulla sebességvektor (Hermite feltétel)
                if (i == 0) p0 = p1 - (p2 - p1) * 0.01f;
                if (i == controlPoints.size() - 2) p3 = p2 + (p2 - p1) * 0.01f;
                
                // Szakasz felosztása
                for (int j = 0; j <= segments; j++) {
                    float t = (float)j / segments;
                    curvePoints.push_back(CatmullRom(p0, p1, p2, p3, t));
                }
            }
        }
        
        // Görbe geometria frissítése
        if (splineGeometry) {
            delete splineGeometry;
        }
        
        splineGeometry = new Geometry<vec2>;
        splineGeometry->Vtx() = curvePoints;
        splineGeometry->updateGPU();
        
        // Kontrollpontok geometria frissítése
        if (pointGeometry) {
            delete pointGeometry;
        }
        
        pointGeometry = new Geometry<vec2>;
        pointGeometry->Vtx() = controlPoints;
        pointGeometry->updateGPU();
    }
    
public:
    Spline() {
        splineGeometry = nullptr;
        pointGeometry = nullptr;
    }
    
    ~Spline() {
        if (splineGeometry) delete splineGeometry;
        if (pointGeometry) delete pointGeometry;
    }
    
    // Új kontrollpont hozzáadása
    void addControlPoint(vec2 point) {
        controlPoints.push_back(point);
        updateGeometry();
    }
    
    // Kontrollpontok számának lekérdezése
    size_t getNumControlPoints() const {
        return controlPoints.size();
    }
    
    // Görbe paraméteres egyenlete: r(t)
    vec2 r(float t) {
        if (controlPoints.size() < 2) return vec2(0, 0);
        
        // t paraméter korlátozása a [0, n-1] tartományra, ahol n a kontrollpontok száma
        if (t < 0) t = 0;
        if (t > controlPoints.size() - 1) t = controlPoints.size() - 1;
        
        // Egész és tört rész kiszámítása
        int i = (int)t;
        float u = t - i;
        
        // Kontrollpontok kiválasztása
        vec2 p0 = (i > 0) ? controlPoints[i-1] : controlPoints[i];
        vec2 p1 = controlPoints[i];
        vec2 p2 = (i < controlPoints.size() - 1) ? controlPoints[i+1] : controlPoints[i];
        vec2 p3 = (i < controlPoints.size() - 2) ? controlPoints[i+2] : p2;
        
        // A kezdő és végpontokban nulla sebességvektor (Hermite feltétel)
        if (i == 0) p0 = p1 - (p2 - p1) * 0.01f;
        if (i >= controlPoints.size() - 2) p3 = p2 + (p2 - p1) * 0.01f;
        
        // Spline kiértékelése
        return CatmullRom(p0, p1, p2, p3, u);
    }
    
    // Görbe deriváltja adott paraméternél: r'(t)
    vec2 rDerivative(float t) {
        if (controlPoints.size() < 2) return vec2(0, 0);
        
        // t paraméter korlátozása a [0, n-1] tartományra, ahol n a kontrollpontok száma
        if (t < 0) t = 0;
        if (t > controlPoints.size() - 1) t = controlPoints.size() - 1;
        
        // Egész és tört rész kiszámítása
        int i = (int)t;
        float u = t - i;
        
        // Kontrollpontok kiválasztása
        vec2 p0 = (i > 0) ? controlPoints[i-1] : controlPoints[i];
        vec2 p1 = controlPoints[i];
        vec2 p2 = (i < controlPoints.size() - 1) ? controlPoints[i+1] : controlPoints[i];
        vec2 p3 = (i < controlPoints.size() - 2) ? controlPoints[i+2] : p2;
        
        // A kezdő és végpontokban nulla sebességvektor (Hermite feltétel)
        if (i == 0) p0 = p1 - (p2 - p1) * 0.01f;
        if (i >= controlPoints.size() - 2) p3 = p2 + (p2 - p1) * 0.01f;
        
        // Derivált kiértékelése
        return CatmullRomDerivative(p0, p1, p2, p3, u);
    }
    
    // Egységvektor a pálya érintője irányában T(t)
    vec2 T(float t) {
        vec2 derivative = rDerivative(t);
        float len = length(derivative);
        if (len < 0.0001f) return vec2(1, 0); // Ha túl kicsi a hossz, vízszintes vektort adunk vissza
        return derivative / len;
    }
    
    // Normálvektor a pálya érintőjére merőlegesen N(t)
    vec2 N(float t) {
        vec2 tangent = T(t);
        return vec2(-tangent.y, tangent.x); // 90 fokos elforgatás
    }
    
    // Görbe kirajzolása
    void Draw(GPUProgram* gpuProgram) {
        // Ha nincs elég pont, nem rajzolunk
        if (controlPoints.size() < 2) return;
        
        // Görbe kirajzolása sárga színnel
        glLineWidth(3.0f);
        splineGeometry->Draw(gpuProgram, GL_LINE_STRIP, vec3(1.0f, 1.0f, 0.0f));
        
        // Kontrollpontok kirajzolása piros négyzetként
        glPointSize(10.0f);
        pointGeometry->Draw(gpuProgram, GL_POINTS, vec3(1.0f, 0.0f, 0.0f));
    }
};

//---------------------------
// Gondola osztály a kerékhez
class Gondola {
private:
    Geometry<vec2> *wheel;        // kör geometria
    Geometry<vec2> *spokes;       // küllők geometria
    Spline *track;                // pálya referencia
    
    GondolaState state;          // állapot
    float splineParam;           // pálya paraméter (t)
    float wheelRadius;           // kerék sugara méterben
    vec2 position;               // kerék pozíciója
    float angle;                 // kerék elfordulása
    float velocity;              // sebesség
    float lambda;                // alaktényező a tehetetlenségi nyomatékhoz
    
    // Kör létrehozása
    void createWheel(float radius) {
        wheel = new Geometry<vec2>();
        
        // Kör létrehozása
        const int segments = 36;
        std::vector<vec2> vertices;
        
        // Középpont
        vertices.push_back(vec2(0, 0));
        
        // Körvonal pontjai
        for (int i = 0; i <= segments; i++) {
            float phi = i * 2 * M_PI / segments;
            vertices.push_back(vec2(radius * cos(phi), radius * sin(phi)));
        }
        
        // Geometria beállítása
        wheel->Vtx() = vertices;
        wheel->updateGPU();
    }
    
    // Küllők létrehozása
    void createSpokes(float radius) {
        spokes = new Geometry<vec2>();
        
        // Küllők pontjai
        std::vector<vec2> vertices;
        
        // 2 küllő létrehozása
        vertices.push_back(vec2(0, 0));
        vertices.push_back(vec2(radius, 0));
        
        vertices.push_back(vec2(0, 0));
        vertices.push_back(vec2(0, radius));
        
        // Geometria beállítása
        spokes->Vtx() = vertices;
        spokes->updateGPU();
    }
    
    // Szükséges centripetális erő kiszámítása
    float calculateCentripetalForce(float param) {
        // Sebesség négyzete és a görbület szorzata
        return velocity * velocity * getCurvature(param);
    }
    
    // Ellenőrizzük, hogy a kerék a pályán marad-e
    bool staysOnTrack(float param) {
        // Normálvektor a pályára
        vec2 normal = track->N(param);
        
        // A nehézségi erő és a centripetális erő normálirányú komponense
        vec2 gravity(0, -g);
        float normalGravity = dot(gravity, normal); // g negatív Y irányba mutat
        float centripetalForce = calculateCentripetalForce(param);
        
        // A pályán marad, ha a kényszererő pozitív (nyomja a pályát)
        // K = m * (g·N + v²κ) > 0
        return (normalGravity + centripetalForce) > 0;
    }
    
    // Ellenőrizzük, hogy a kerék nem mozog-e visszafelé
    bool isMovingBackwards() {
        // Egyszerűen a sebesség előjele
        return velocity < 0;
    }
    
public:
    Gondola(Spline *trackRef) : track(trackRef) {
        state = WAITING;
        splineParam = 0.01f;  // kezdő paraméter a görbén
        wheelRadius = 1.0f;   // 1m sugár
        position = vec2(0, 0);
        angle = 0.0f;
        velocity = 0.0f;
        lambda = 1.0f;        // homogén tömegeloszlás a peremén
        
        // Kerék és küllők létrehozása
        createWheel(wheelRadius);
        createSpokes(wheelRadius);
    }
    
    ~Gondola() {
        delete wheel;
        delete spokes;
    }
    
    // Kerék indítása
    void Start() {
        if (state == WAITING && track->getNumControlPoints() >= 2) {
            state = ROLLING;
            // Mindig a 0.01 paraméterű pontnál indítunk
            splineParam = 0.01f;
            position = track->r(splineParam);
            angle = 0.0f;
            velocity = 0.0f; // Nulla kezdősebességgel indulunk
        }
    }
    
    // Kerék állapotának frissítése
    void Animate(float dt) {
        if (state != ROLLING || track->getNumControlPoints() < 2) return;
        
        // Pálya érintő és normál vektorai
        vec2 tangent = track->T(splineParam);
        vec2 normal = track->N(splineParam);
        
        // Aktuális pozíció a pályán
        position = track->r(splineParam);
        
        // Gravitáció komponensei
        vec2 gravity(0, -g);  // g lefelé mutat (negatív y irányba)
        float tangentialGravity = dot(gravity, tangent);
        
        // Gyorsulás számítása a gravitáció tangenciális komponenséből
        // a = g·T / (1 + λ)
        float acceleration = tangentialGravity / (1.0f + lambda);
        
        // Sebesség frissítése a gyorsulás alapján
        velocity += acceleration * dt;
        
        // Debug kiírás
        printf("param: %.2f, pos: (%.2f, %.2f), vel: %.2f, acc: %.2f\n", 
               splineParam, position.x, position.y, velocity, acceleration);
        
        // Normálerő számítása
        // A nehézségi erő és a centripetális erő normálirányú komponense
        float normalGravity = dot(gravity, normal);
        float centripetalAcceleration = velocity * velocity * getCurvature(splineParam);
        float normalForce = normalGravity + centripetalAcceleration;
        
        // Ellenőrizzük, hogy a kerék a pályán marad-e (pozitív normálerő)
        if (normalForce <= 0) {
            state = FALLEN;
            return;
        }
        
        // Paraméter frissítése a sebesség alapján
        // Δτ = v·Δt / |r'(τ)|
        float derivativeLength = length(track->rDerivative(splineParam));
        if (derivativeLength > 0.0001f) {
            float paramStep = velocity * dt / derivativeLength;
            
            // Frissítjük a paramétert
            splineParam += paramStep;
            
            // Biztosítjuk, hogy a paraméter a görbe tartományában maradjon
            float maxParam = float(track->getNumControlPoints() - 1);
            if (splineParam < 0.01f || splineParam >= maxParam) {
                // Ha kimegy a tartományból, visszaállítjuk a kezdőpontra
                splineParam = 0.01f;
                velocity = 0.0f;
            }
        }
        
        // Kerék középpontjának pozíció frissítése
        position = track->r(splineParam) + normal * wheelRadius;
        
        // Szögsebesség: ω = -v/R
        float angularVelocity = -velocity / wheelRadius;
        angle += angularVelocity * dt;
    }
    
    // Görbület számítása adott paraméternél
    float getCurvature(float t) {
        vec2 firstDerivative = track->rDerivative(t);
        vec2 normalVector = track->N(t);
        
        // Második derivált közelítése
        float deltaT = 0.001f;
        vec2 nextDerivative = track->rDerivative(t + deltaT);
        vec2 secondDerivative = (nextDerivative - firstDerivative) / deltaT;
        
        // Görbület: κ = |r''·N| / |r'|²
        float firstDerivMagnitudeSq = dot(firstDerivative, firstDerivative);
        if (firstDerivMagnitudeSq < 0.0001f) return 0.0f;
        
        return abs(dot(secondDerivative, normalVector)) / firstDerivMagnitudeSq;
    }
    
    // Kerék kirajzolása
    void Draw(GPUProgram* gpuProgram, const mat4& viewMatrix) {
        if (state == WAITING || track->getNumControlPoints() < 2) return;
        
        // Modell transzformáció beállítása
        mat4 modelMatrix = translate(mat4(1.0f), vec3(position, 0));
        modelMatrix = rotate(modelMatrix, angle, vec3(0, 0, 1));
        
        // MVP mátrix beállítása
        mat4 mvpMatrix = viewMatrix * modelMatrix;
        gpuProgram->setUniform(mvpMatrix, "MVP");
        
        // Kerék kirajzolása
        glPointSize(1.0f);
        wheel->Draw(gpuProgram, GL_TRIANGLE_FAN, vec3(0, 0, 1)); // kék kitöltés
        
        // Körvonal kirajzolása
        glLineWidth(2.0f);
        wheel->Draw(gpuProgram, GL_LINE_LOOP, vec3(1, 1, 1)); // fehér körvonal
        
        // Küllők kirajzolása
        spokes->Draw(gpuProgram, GL_LINES, vec3(1, 1, 1)); // fehér küllők
    }
    
    // Állapot lekérdezése
    GondolaState getState() const {
        return state;
    }
};

class RollerCoasterApp : public glApp {
    Spline *track;          // pálya
    GPUProgram *gpuProgram; // csúcspont és pixel árnyalók
    Camera *camera;         // virtuális kamera
    Gondola *gondola;       // kerék (gondola)
public:
    RollerCoasterApp() : glApp("Hullámvasút szimuláció") {}

    // Inicializáció
    void onInitialization() {
        // Kamera inicializálása a világ közepével és méretével
        camera = new Camera(vec2(0.0f, 0.0f), worldWidth, worldHeight);
        
        // Pálya inicializálása
        track = new Spline();
        
        // Gondola inicializálása
        gondola = new Gondola(track);
        
        // GPU program inicializálása
        gpuProgram = new GPUProgram(vertSource, fragSource);
    }

    // Ablak újrarajzolás
    void onDisplay() {
        glClearColor(0, 0, 0, 0);     // háttér szín
        glClear(GL_COLOR_BUFFER_BIT); // rasztertér törlés
        glViewport(0, 0, winWidth, winHeight);
        
        // MVP mátrix beállítása
        gpuProgram->setUniform(camera->getMVP(), "MVP");
        
        // Pálya kirajzolása
        track->Draw(gpuProgram);
        
        // Gondola kirajzolása
        gondola->Draw(gpuProgram, camera->getMVP());
    }
    
    // Billentyűzet kezelése
    void onKeyboard(int key) {
        if (key == ' ') { // SPACE
            gondola->Start(); // Kerék indítása
            refreshScreen();
        }
    }
    
    // Egér lenyomás kezelése
    void onMousePressed(MouseButton button, int pX, int pY) {
        if (button == MOUSE_LEFT) {
            // Képernyő koordináták átváltása világkoordinátákba
            vec2 worldPos = camera->screenToWorld(pX, pY);
            
            // Új kontrollpont hozzáadása
            track->addControlPoint(worldPos);
            
            // Újrarajzolás kérése
            refreshScreen();
        }
    }
    
    // Idő változása
    void onTimeElapsed(float startTime, float endTime) {
        // Időlépés számítása
        float dt = endTime - startTime;
        
        // Gondola animálása
        gondola->Animate(dt);
        
        // Újrarajzolás kérése
        refreshScreen();
    }
    
    // Felszabadítás
    ~RollerCoasterApp() {
        delete track;
        delete gpuProgram;
        delete camera;
        delete gondola;
    }
};

RollerCoasterApp app;