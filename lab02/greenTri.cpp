//=============================================================================================
// Hullámvasút szimuláció: A framework.h osztályait felhasználó megoldás
//=============================================================================================
#include "framework.h"

// Vertex shader (csúcspont árnyaló)
const char *vertSource = R"(
	#version 330
    precision highp float;

	layout(location = 0) in vec2 cP;	// 0. bemeneti regiszter
    uniform mat4 MVP;                 // Model-View-Projection mátrix

	void main() {
		gl_Position = MVP * vec4(cP.x, cP.y, 0, 1);
	}
)";

// Fragment shader (pixelpont árnyaló)
const char *fragSource = R"(
	#version 330
    precision highp float;

	uniform vec3 color;			// konstans szín
	out vec4 fragmentColor;		// pixel szín

	void main() {
		fragmentColor = vec4(color, 1); // RGB -> RGBA
	}
)";

const int winWidth = 600, winHeight = 600;

// Kameraablak beállítása a feladat szerint: 20m x 20m
const float windowWidth = 20.0f, windowHeight = 20.0f;

// Camera osztály definiálása
class Camera {
private:
    float windowWidth, windowHeight;
    vec2 center;
    mat4 viewMatrix, projMatrix, invViewMatrix, invProjMatrix;
public:
    Camera(float width, float height) : windowWidth(width), windowHeight(height), center(0, 0) {
        updateMatrices();
    }

    void updateMatrices() {
        // View mátrix számítása a kameraablak középpontjából
        viewMatrix = mat4(
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            -center.x, -center.y, 0, 1
        );

        // Ortogonális projekciós mátrix számítása
        projMatrix = mat4(
            2.0f / windowWidth, 0, 0, 0,
            0, 2.0f / windowHeight, 0, 0,
            0, 0, -1, 0,
            0, 0, 0, 1
        );

        // Inverz mátrixok számítása
        invViewMatrix = inverse(viewMatrix);
        invProjMatrix = inverse(projMatrix);
    }

    mat4 getViewMatrix() const { return viewMatrix; }
    mat4 getProjMatrix() const { return projMatrix; }
    mat4 getVPMatrix() const { return projMatrix * viewMatrix; }

    // Bemeneti csővezeték: képernyőkoordinátából világkoordináta
    vec2 convertToWorldCoords(int pixelX, int pixelY, int screenWidth, int screenHeight) {
        // Normalizált eszközkoordináták kiszámítása
        float ndcX = 2.0f * pixelX / screenWidth - 1.0f;
        float ndcY = 1.0f - 2.0f * pixelY / screenHeight;

        // Világkoordináták kiszámítása
        vec4 worldPos = invViewMatrix * invProjMatrix * vec4(ndcX, ndcY, 0, 1);
        return vec2(worldPos.x, worldPos.y);
    }
};

// Catmull-Rom spline osztály
class Spline {
private:
    std::vector<vec2> controlPoints;
    Geometry<vec2>* splineCurve;
    Geometry<vec2>* controlPointsGeom;
    float paramRange;

    // Catmull-Rom bázisfüggvények
    mat4 getBaseFunctions() {
        return mat4(
            -0.5f,  1.5f, -1.5f,  0.5f,
             1.0f, -2.5f,  2.0f, -0.5f,
            -0.5f,  0.0f,  0.5f,  0.0f,
             0.0f,  1.0f,  0.0f,  0.0f
        );
    }

public:
    Spline() {
        splineCurve = new Geometry<vec2>();
        controlPointsGeom = new Geometry<vec2>();
        paramRange = 0.0f;
    }

    ~Spline() {
        delete splineCurve;
        delete controlPointsGeom;
    }

    void addControlPoint(vec2 point) {
        controlPoints.push_back(point);
        controlPointsGeom->Vtx() = controlPoints;
        controlPointsGeom->updateGPU();
        updateSplineCurve();
    }

    int getNumControlPoints() const {
        return controlPoints.size();
    }

    // Catmull-Rom spline pont kiszámítása tau paraméter szerint
    vec2 r(float tau) {
        if (controlPoints.size() < 2) {
            return vec2(0, 0); // Nem elég kontrollpont a spline számításához
        }
        
        // Ha csak 2 vagy 3 kontrollpont van, lineáris interpolációt használunk
        if (controlPoints.size() < 4) {
            int n = controlPoints.size() - 1;
            float u = tau * n;
            int i = min(int(u), n-1);
            float t = u - i;
            
            vec2 p0 = controlPoints[i];
            vec2 p1 = controlPoints[i+1];
            
            return p0 * (1-t) + p1 * t;
        }

        // Tau-ból segment és lokális paraméter meghatározása
        int n = controlPoints.size() - 3;
        float u = tau * n;
        int i = min(int(u), n-1);
        float t = u - i;

        // Kontrollpontok kiválasztása (4 pont a Catmull-Rom spline-hoz)
        vec2 p0 = controlPoints[i];
        vec2 p1 = controlPoints[i+1];
        vec2 p2 = controlPoints[i+2];
        vec2 p3 = controlPoints[i+3];

        // Az első és utolsó szakasznál a sebességvektorok nullák
        if (i == 0 || i == n-1) {
            // Módosított bázisfüggvény első és utolsó szakaszhoz
            if (i == 0) {
                // Hermite-féle interpolációs mátrix, ahol a p0-nál a derivált 0
                mat4 basis = mat4(
                    0.0f, 0.0f, 0.0f, 1.0f,  // a p0-nál a derivált 0
                    0.0f, 0.0f, 1.0f, 0.0f,  // p1 pont
                    0.0f, 1.0f, 0.0f, 0.0f,  // p2 pont
                    1.0f, 0.0f, 0.0f, 0.0f   // p3 pont
                );
                
                vec4 tVec = vec4(t*t*t, t*t, t, 1);
                vec4 px = vec4(p0.x, p1.x, p2.x, p3.x);
                vec4 py = vec4(p0.y, p1.y, p2.y, p3.y);
                
                float x = dot(tVec, basis * px);
                float y = dot(tVec, basis * py);
                
                return vec2(x, y);
            }
            else { // i == n-1
                // Hermite-féle interpolációs mátrix, ahol a p3-nál a derivált 0
                mat4 basis = mat4(
                    1.0f, 0.0f, 0.0f, 0.0f,  // p0 pont
                    0.0f, 1.0f, 0.0f, 0.0f,  // p1 pont
                    0.0f, 0.0f, 1.0f, 0.0f,  // p2 pont
                    0.0f, 0.0f, 0.0f, 1.0f   // a p3-nál a derivált 0
                );
                
                vec4 tVec = vec4(t*t*t, t*t, t, 1);
                vec4 px = vec4(p0.x, p1.x, p2.x, p3.x);
                vec4 py = vec4(p0.y, p1.y, p2.y, p3.y);
                
                float x = dot(tVec, basis * px);
                float y = dot(tVec, basis * py);
                
                return vec2(x, y);
            }
        }
        
        // Standard Catmull-Rom spline köztes szakaszokhoz
        mat4 basis = getBaseFunctions();
        vec4 tVec = vec4(t*t*t, t*t, t, 1);
        
        vec4 px = vec4(p0.x, p1.x, p2.x, p3.x);
        vec4 py = vec4(p0.y, p1.y, p2.y, p3.y);
        
        float x = dot(tVec * basis, px);
        float y = dot(tVec * basis, py);
        
        return vec2(x, y);
    }

    // Catmull-Rom spline deriváltja tau szerint
    vec2 rDot(float tau) {
        if (controlPoints.size() < 4) {
            return vec2(0, 0); // Nem elég kontrollpont a derivált számításához
        }

        // Tau-ból segment és lokális paraméter meghatározása
        int n = controlPoints.size() - 3;
        float u = tau * n;
        int i = min(int(u), n-1);
        float t = u - i;

        // Kontrollpontok kiválasztása
        vec2 p0 = controlPoints[i];
        vec2 p1 = controlPoints[i+1];
        vec2 p2 = controlPoints[i+2];
        vec2 p3 = controlPoints[i+3];

        // Az első és utolsó szakasznál a sebességvektorok nullák
        if (i == 0 && t < 0.001f) return vec2(0, 0);
        if (i == n-1 && t > 0.999f) return vec2(0, 0);

        // Derivált számítása a bázisfüggvények deriváltjai segítségével
        mat4 basis = getBaseFunctions();
        vec4 dtVec = vec4(3*t*t, 2*t, 1, 0);
        
        vec4 px = vec4(p0.x, p1.x, p2.x, p3.x);
        vec4 py = vec4(p0.y, p1.y, p2.y, p3.y);
        
        float dx = dot(dtVec * basis, px) * n; // Szorzás n-nel a lánc-szabály miatt
        float dy = dot(dtVec * basis, py) * n;
        
        return vec2(dx, dy);
    }

    // Érintővektor számítása adott tau paraméterre
    vec2 T(float tau) {
        vec2 tangent = rDot(tau);
        return normalize(tangent);
    }

    // Normálvektor számítása adott tau paraméterre (érintővektor 90 fokos elforgatása)
    vec2 N(float tau) {
        vec2 t = T(tau);
        return vec2(-t.y, t.x);
    }

    // Görbület számítása adott tau paraméterre
    float curvature(float tau) {
        vec2 firstDeriv = rDot(tau);
        
        // Második derivált számítása
        if (controlPoints.size() < 4) {
            return 0.0f;
        }

        // Tau-ból segment és lokális paraméter meghatározása
        int n = controlPoints.size() - 3;
        float u = tau * n;
        int i = min(int(u), n-1);
        float t = u - i;

        // Kontrollpontok kiválasztása
        vec2 p0 = controlPoints[i];
        vec2 p1 = controlPoints[i+1];
        vec2 p2 = controlPoints[i+2];
        vec2 p3 = controlPoints[i+3];

        // Második derivált számítása
        mat4 basis = getBaseFunctions();
        vec4 ddtVec = vec4(6*t, 2, 0, 0);
        
        vec4 px = vec4(p0.x, p1.x, p2.x, p3.x);
        vec4 py = vec4(p0.y, p1.y, p2.y, p3.y);
        
        float ddx = dot(ddtVec * basis, px) * n * n; // Szorzás n^2-nel a lánc-szabály miatt
        float ddy = dot(ddtVec * basis, py) * n * n;
        
        vec2 secondDeriv = vec2(ddx, ddy);
        
        // Görbület számítása a képlet alapján: κ = |r″(τ)·N| / |r′(τ)|²
        float firstDerivMagnitudeSq = dot(firstDeriv, firstDeriv);
        if (firstDerivMagnitudeSq < 0.0001f) return 0.0f; // Elkerüljük a nullával osztást
        
        vec2 normal = N(tau);
        
        return abs(dot(secondDeriv, normal)) / firstDerivMagnitudeSq;
    }

    // A spline görbe diszkretizálása és megjelenítése
    void updateSplineCurve() {
        if (controlPoints.size() < 2) {
            return;
        }

        std::vector<vec2>& curvePoints = splineCurve->Vtx();
        curvePoints.clear();

        // 100 mintavételi pont a görbén
        const int sampleCount = 100;
        for (int i = 0; i <= sampleCount; i++) {
            float tau = (float)i / sampleCount;
            curvePoints.push_back(r(tau));
        }

        splineCurve->updateGPU();
        paramRange = 1.0f;
    }

    void Draw(GPUProgram* gpuProgram, mat4 VPMatrix) {
        // MVP mátrix beállítása
        gpuProgram->setUniform(VPMatrix, "MVP");
        
        // Görbe megjelenítése sárga vonallal
        if (controlPoints.size() >= 2) {
            splineCurve->Draw(gpuProgram, GL_LINE_STRIP, vec3(1.0f, 1.0f, 0.0f));
        }

        // Kontrollpontok megjelenítése piros pontokkal
        glPointSize(10.0f);
        controlPointsGeom->Draw(gpuProgram, GL_POINTS, vec3(1.0f, 0.0f, 0.0f));
    }

    // A pálya paramétere adott pontra
    float getParameterRange() const {
        return paramRange;
    }
};

// Gondola osztály a kocsi ábrázolásához és fizikai szimulációjához
class Gondola {
private:
    Spline* pTrack;                // A pálya referenciája
    Geometry<vec2>* wheelDisk;     // A kerék körlap
    Geometry<vec2>* wheelOutline;  // A kerék körvonal
    Geometry<vec2>* spokes;        // A küllők
    
    float tau;                     // A pálya paramétere
    float radius;                  // A kerék sugara
    float rotationAngle;           // A kerék elfordulási szöge
    
    enum State { WAITING, MOVING, FALLEN } state;
    
    const float g = 40.0f;         // Nehézségi gyorsulás
    float velocity;                // Pillanatnyi sebesség
    float startHeight;             // Kezdeti magasság
    float lambda;                  // Alaktényező (tehetetlenségi nyomaték: Θ = λmR²)
    
    // Kerék geometriájának létrehozása
    void createGeometry() {
        // Körlap létrehozása
        wheelDisk = new Geometry<vec2>();
        std::vector<vec2>& diskPoints = wheelDisk->Vtx();
        
        // Középpont hozzáadása
        diskPoints.push_back(vec2(0, 0));
        
        // Körpontok hozzáadása
        const int segments = 36;
        for (int i = 0; i <= segments; i++) {
            float angle = (float)i / segments * 2.0f * M_PI;
            diskPoints.push_back(vec2(radius * cos(angle), radius * sin(angle)));
        }
        wheelDisk->updateGPU();
        
        // Körvonal létrehozása
        wheelOutline = new Geometry<vec2>();
        std::vector<vec2>& outlinePoints = wheelOutline->Vtx();
        
        for (int i = 0; i <= segments; i++) {
            float angle = (float)i / segments * 2.0f * M_PI;
            outlinePoints.push_back(vec2(radius * cos(angle), radius * sin(angle)));
        }
        wheelOutline->updateGPU();
        
        // Küllők létrehozása (legalább 2, most 4-et készítünk)
        spokes = new Geometry<vec2>();
        std::vector<vec2>& spokePoints = spokes->Vtx();
        
        // Vízszintes küllő
        spokePoints.push_back(vec2(-radius, 0));
        spokePoints.push_back(vec2(radius, 0));
        
        // Függőleges küllő
        spokePoints.push_back(vec2(0, -radius));
        spokePoints.push_back(vec2(0, radius));
        
        // Átlós küllők (opcionális)
        float r45 = radius * 0.7071f; // radius/sqrt(2)
        spokePoints.push_back(vec2(-r45, -r45));
        spokePoints.push_back(vec2(r45, r45));
        
        spokePoints.push_back(vec2(-r45, r45));
        spokePoints.push_back(vec2(r45, -r45));
        
        spokes->updateGPU();
    }
    
public:
    Gondola(Spline* track) : pTrack(track), radius(1.0f), state(WAITING), lambda(1.0f), rotationAngle(0.0f) {
        createGeometry();
    }
    
    ~Gondola() {
        delete wheelDisk;
        delete wheelOutline;
        delete spokes;
    }
    
    // Gondola indítása a pályán
    void Start() {
        if (pTrack->getNumControlPoints() < 2) return;
        
        state = MOVING;
        tau = 0.01f;  // Kezdő paraméter a görbén
        velocity = 0.0f;  // Indulási sebesség 0
        startHeight = pTrack->r(tau).y;  // Kezdeti magasság (y koordináta)
        rotationAngle = 0.0f;  // Kezdeti elfordulás 0
    }
    
    // Animáció - fizikai szimuláció egy időlépésben
    void Animate(float dt) {
        if (state != MOVING || pTrack->getNumControlPoints() < 2) return;
        
        // Aktuális pozíció és vektorok a görbén
        vec2 position = pTrack->r(tau);
        vec2 tangent = pTrack->T(tau);
        vec2 normal = pTrack->N(tau);
        
        // Sebesség számítása energiamegmaradás alapján
        float height = position.y;
        float heightDiff = startHeight - height;
        
        // v = sqrt(2g * heightDiff / (1 + λ))
        // A kerék tömege a körvonalon egyenletesen oszlik szét, λ = 1
        velocity = sqrt(2.0f * g * heightDiff / (1.0f + lambda));
        
        // Ellenőrizzük, hogy a sebesség nem vált-e negatívvá (visszafele nem mozoghat)
        if (velocity < 0) {
            // Újraindítás a kezdőpontból
            tau = 0.0f;
            startHeight = pTrack->r(tau).y;
            velocity = 0.0f;
            rotationAngle = 0.0f;
            return;
        }
        
        // Görbület és centripetális erő számítása
        float kappa = pTrack->curvature(tau);
        float centripetalForce = velocity * velocity * kappa;
        
        // Kényszererő számítása: K = m(g·N + v²κ)
        // Mivel m = 1, ezért K = g·N + v²κ
        float normalForceComponent = g * dot(normal, vec2(0, -1)); // g·N
        float constraintForce = normalForceComponent + centripetalForce;
        
        // Ha a kényszererő negatív, a kerék lerepül a pályáról
        if (constraintForce < 0) {
            state = FALLEN;
            return;
        }
        
        // Paraméter léptetése az eltelt idő alapján
        float ds = velocity * dt;  // Megtett út = sebesség * idő
        float paramDerivativeMagnitude = length(pTrack->rDot(tau));
        
        // Paraméternövekmény: Δτ = Δs / |r'(τ)|
        float dtau = ds / paramDerivativeMagnitude;
        tau += dtau;
        
        // Kerék elfordulási szögének frissítése
        // Δθ = -Δs / r (a gördülési feltétel alapján, a negatív előjel a forgás irányát jelzi)
        rotationAngle -= ds / radius;
        
        // Ha elértük a pálya végét, visszatérünk a kezdőpontra
        if (tau >= pTrack->getParameterRange()) {
            tau = 0.0f;
            startHeight = pTrack->r(tau).y;
            velocity = 0.0f;
            rotationAngle = 0.0f;
        }
    }
    
    // Gondola megjelenítése
    void Draw(GPUProgram* gpuProgram, mat4 VPMatrix) {
        if (state == WAITING || pTrack->getNumControlPoints() < 2) return;
        
        // Pozíció a görbén
        vec2 position = pTrack->r(tau);
        
        // Forgásmátrix létrehozása a kerék elfordulásához
        mat4 rotationMatrix = mat4(
            cos(rotationAngle), -sin(rotationAngle), 0, 0,
            sin(rotationAngle), cos(rotationAngle), 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        );
        
        // Eltolási mátrix létrehozása a kerék pozíciójához
        mat4 translationMatrix = mat4(
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            position.x, position.y, 0, 1
        );
        
        // Modelltranszformáció: először forgat, majd eltol
        mat4 modelMatrix = translationMatrix * rotationMatrix;
        
        // MVP mátrix beállítása
        mat4 MVPMatrix = VPMatrix * modelMatrix;
        gpuProgram->setUniform(MVPMatrix, "MVP");
        
        // Kerék megjelenítése (kék kitöltés, fehér körvonal, fehér küllők)
        if (state != FALLEN) {
            // Kék körlap
            wheelDisk->Draw(gpuProgram, GL_TRIANGLE_FAN, vec3(0.0f, 0.0f, 1.0f));
            
            // Fehér körvonal
            wheelOutline->Draw(gpuProgram, GL_LINE_LOOP, vec3(1.0f, 1.0f, 1.0f));
            
            // Fehér küllők
            spokes->Draw(gpuProgram, GL_LINES, vec3(1.0f, 1.0f, 1.0f));
        }
    }
    
    // Gondola állapotának lekérdezése
    bool isMoving() const { return state == MOVING; }
    bool isFallen() const { return state == FALLEN; }
};

// Fő alkalmazás osztály
class HullamvasutApp : public glApp {
    Camera* camera;
    GPUProgram* gpuProgram;
    Spline* spline;
    Gondola* gondola;

public:
    HullamvasutApp() : glApp("Hullámvasút szimuláció") {}

    ~HullamvasutApp() {
        delete camera;
        delete gpuProgram;
        delete spline;
        delete gondola;
    }

    void onInitialization() {
        // Kamera beállítása 20x20-as világablakkal
        camera = new Camera(windowWidth, windowHeight);
        
        // Shader program beállítása
        gpuProgram = new GPUProgram(vertSource, fragSource);
        
        // Spline inicializálása
        spline = new Spline();
        
        // Gondola inicializálása
        gondola = new Gondola(spline);
        
        // OpenGL beállítások
        glLineWidth(3);
        glEnable(GL_LINE_SMOOTH);
    }

    void onDisplay() {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Viewport beállítása
        glViewport(0, 0, winWidth, winHeight);
        
        // MVP mátrix
        mat4 VPMatrix = camera->getVPMatrix();
        
        // Spline megjelenítése
        spline->Draw(gpuProgram, VPMatrix);
        
        // Gondola megjelenítése
        gondola->Draw(gpuProgram, VPMatrix);
    }

    void onKeyboard(int key) {
        // Space gomb lenyomásakor indítjuk a gondolát
        if (key == ' ') {
            gondola->Start();
            refreshScreen();
        }
    }


    void onMousePressed(MouseButton but, int pX, int pY) {
        if (but == MOUSE_LEFT) {
            // Egérpozíció konvertálása világkoordinátákká
            vec2 worldPos = camera->convertToWorldCoords(pX, pY, winWidth, winHeight);
            
            // Új kontrollpont hozzáadása
            spline->addControlPoint(worldPos);
            
            refreshScreen();
        }
    }

    void onTimeElapsed(float startTime, float endTime) {
    float dt = endTime - startTime;
    const float maxDt = 0.01f; // Maximum időlépés
    
    // Nagy időlépéseket kisebbekre bontjuk
    for (float t = 0; t < dt; t += maxDt) {
        float stepDt = min(maxDt, dt - t);
        gondola->Animate(stepDt);
    }
    
    refreshScreen();
}
    
};

// Alkalmazás példányosítása
HullamvasutApp app;