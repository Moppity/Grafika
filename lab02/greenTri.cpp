//=============================================================================================
// Hullámvasút szimuláció Catmull-Rom Spline-nal
//=============================================================================================
#include "./framework.h"

// csúcspont árnyaló
const char *vertSource = R"(
	#version 330				
    precision highp float;

	layout(location = 0) in vec2 cP;	// 0. bemeneti regiszter

	void main() {
		gl_Position = vec4(cP.x, cP.y, 0, 1); 	// bemenet már normalizált eszközkoordinátákban
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

const int winWidth = 600, winHeight = 600;

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
    
    // Spline kirajzolása
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
    
    // Kontrollpontok számának lekérdezése
    int getControlPointCount() const {
        return controlPoints.size();
    }
};

class RollerCoasterApp : public glApp {
    Spline *track;         // pálya
    GPUProgram *gpuProgram;   // csúcspont és pixel árnyalók
public:
    RollerCoasterApp() : glApp("Hullámvasút szimuláció") {}

    // Inicializáció
    void onInitialization() {
        track = new Spline();
        gpuProgram = new GPUProgram(vertSource, fragSource);
    }

    // Ablak újrarajzolás
    void onDisplay() {
        glClearColor(0, 0, 0, 0);     // háttér szín
        glClear(GL_COLOR_BUFFER_BIT); // rasztertér törlés
        glViewport(0, 0, winWidth, winHeight);
        
        // Pálya kirajzolása, ha van legalább 2 kontrollpont
        track->Draw(gpuProgram);
    }
    
    // Egér lenyomás kezelése
    void onMousePressed(MouseButton button, int pX, int pY) {
        if (button == MOUSE_LEFT) {
            // Képernyő koordináták normalizálása [-1,1] tartományba
            float cX = 2.0f * pX / winWidth - 1.0f;
            float cY = 1.0f - 2.0f * pY / winHeight;
            
            // Új kontrollpont hozzáadása
            track->addControlPoint(vec2(cX, cY));
            
            // Újrarajzolás kérése
            refreshScreen();
        }
    }
    
    // Idő változása
    void onTimeElapsed(float startTime, float endTime) {
        refreshScreen();
    }
    
    // Felszabadítás
    ~RollerCoasterApp() {
        delete track;
        delete gpuProgram;
    }
};

RollerCoasterApp app;