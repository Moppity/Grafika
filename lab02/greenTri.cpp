#include "./framework.h"

const char *vertSource = R"(
	#version 330				
    precision highp float;

	layout(location = 0) in vec2 cP;	// 0. bemeneti regiszter
    uniform mat4 MVP;              // Model-View-Projection transzformĂĄciĂł

	void main() {
		gl_Position = MVP * vec4(cP.x, cP.y, 0, 1); 	// transzformĂĄciĂł alkalmazĂĄsa
	}
)";

// pixel ĂĄrnyalĂł
const char *fragSource = R"(
	#version 330
    precision highp float;

	uniform vec3 color;			// konstans szĂ­n
	out vec4 fragmentColor;		// pixel szĂ­n

	void main() {
		fragmentColor = vec4(color, 1); // RGB -> RGBA
	}
)";

const int winWidth = 600, winHeight = 600;
const float worldWidth = 20.0f;  
const float worldHeight = 20.0f; 
const float g = 40.0f;           

// gondola ĂĄllapotai
enum GondolaState {
    WAITING,    
    ROLLING,    
    FALLEN      
};

// SajĂĄt mĂĄtrix inverz fĂźggvĂŠny
mat4 invertMatrix(const mat4& m) {
    // EgyszerĹąsĂ­tett 4x4 mĂĄtrix inverz (csak eltolĂĄst ĂŠs skĂĄlĂĄzĂĄst kezel)
    mat4 inv(1.0f);
    
    // Az eltolĂĄs inverze
    inv[3][0] = -m[3][0];
    inv[3][1] = -m[3][1];
    inv[3][2] = -m[3][2];
    
    // A skĂĄlĂĄzĂĄs inverze
    if (m[0][0] != 0) inv[0][0] = 1.0f / m[0][0];
    if (m[1][1] != 0) inv[1][1] = 1.0f / m[1][1];
    if (m[2][2] != 0) inv[2][2] = 1.0f / m[2][2];
    
    return inv;
}

class Camera {
private:
    vec2 center;         
    float width, height; 
    mat4 viewMatrix;     // view transzformĂĄciĂłs mĂĄtrix
    mat4 projMatrix;     // projekciĂłs transzformĂĄciĂłs mĂĄtrix
    mat4 invViewMatrix;  // view mĂĄtrix inverze
    mat4 invProjMatrix;  // projekciĂłs mĂĄtrix inverze
    mat4 MVPMatrix;      // model-view-projection mĂĄtrix
    
    void updateMatrices() {
        // View mĂĄtrix szĂĄmĂ­tĂĄsa - eltolĂĄs a vilĂĄg kĂśzĂŠppontjĂĄba
        viewMatrix = mat4(1.0f);
        viewMatrix[3][0] = -center.x;
        viewMatrix[3][1] = -center.y;
        
        // ProjekciĂłs mĂĄtrix - skĂĄlĂĄzĂĄs a [-1,1] tartomĂĄnyba
        projMatrix = mat4(1.0f);
        projMatrix[0][0] = 2.0f/width;
        projMatrix[1][1] = 2.0f/height;
        
        // Inverz mĂĄtrixok szĂĄmĂ­tĂĄsa
        invViewMatrix = invertMatrix(viewMatrix);
        invProjMatrix = invertMatrix(projMatrix);
        
        // Teljes MVP mĂĄtrix szĂĄmĂ­tĂĄsa
        MVPMatrix = projMatrix * viewMatrix;
    }
    
public:
    Camera(const vec2& _center, float _width, float _height) : 
           center(_center), width(_width), height(_height) {
        updateMatrices();
    }
    
    // VilĂĄgkoordinĂĄtĂĄk ĂĄtvĂĄltĂĄsa kĂŠpernyĹkoordinĂĄtĂĄkra
    vec2 worldToScreen(const vec2& worldPos) {
        vec4 clipPos = MVPMatrix * vec4(worldPos.x, worldPos.y, 0.0f, 1.0f);
        return vec2(clipPos.x, clipPos.y);
    }
    
    // KĂŠpernyĹkoordinĂĄtĂĄk ĂĄtvĂĄltĂĄsa vilĂĄgkoordinĂĄtĂĄkra
    vec2 screenToWorld(int screenX, int screenY) {
        // NormalizĂĄlt eszkĂśzkoordinĂĄtĂĄk szĂĄmĂ­tĂĄsa
        float ndcX = 2.0f * screenX / winWidth - 1.0f;
        float ndcY = 1.0f - 2.0f * screenY / winHeight;
        
        // HomogĂŠn koordinĂĄtĂĄk szĂĄmĂ­tĂĄsa
        vec4 clipPos = vec4(ndcX, ndcY, 0.0f, 1.0f);
        
        // VisszatranszformĂĄlĂĄs vilĂĄgkoordinĂĄtĂĄkba
        vec4 worldPos = invProjMatrix * invViewMatrix * clipPos;
        
        return vec2(worldPos.x, worldPos.y);
    }
    
    // MVP mĂĄtrix lekĂŠrdezĂŠse
    const mat4& getMVP() const {
        return MVPMatrix;
    }
};

class Spline {
private:
    std::vector<vec2> controlPoints;      
    Geometry<vec2> *splineGeometry;       // gĂśrbe geometria
    Geometry<vec2> *pointGeometry;        // pontok geometria
    
    // Catmull-Rom spline kiĂŠrtĂŠkelĂŠse
    vec2 CatmullRom(const vec2& p0, const vec2& p1, const vec2& p2, const vec2& p3, float t) {
        float t2 = t * t;
        float t3 = t2 * t;
        
        // Catmull-Rom mĂĄtrix
        float h1 = -0.5f * t3 + t2 - 0.5f * t;
        float h2 = 1.5f * t3 - 2.5f * t2 + 1.0f;
        float h3 = -1.5f * t3 + 2.0f * t2 + 0.5f * t;
        float h4 = 0.5f * t3 - 0.5f * t2;
        
        return h1 * p0 + h2 * p1 + h3 * p2 + h4 * p3;
    }
    
    // Catmull-Rom spline derivĂĄltjĂĄnak kiĂŠrtĂŠkelĂŠse
    vec2 CatmullRomDerivative(const vec2& p0, const vec2& p1, const vec2& p2, const vec2& p3, float t) {
        float t2 = t * t;
        
        // A Catmull-Rom derivĂĄltja
        float dh1 = -1.5f * t2 + 2.0f * t - 0.5f;
        float dh2 = 4.5f * t2 - 5.0f * t;
        float dh3 = -4.5f * t2 + 4.0f * t + 0.5f;
        float dh4 = 1.5f * t2 - 1.0f * t;
        
        return dh1 * p0 + dh2 * p1 + dh3 * p2 + dh4 * p3;
    }
    
    // Geometria frissĂ­tĂŠse
    void updateGeometry() {
        if (controlPoints.size() < 2) return;
        
        std::vector<vec2> curvePoints;
        
        // Ha van legalĂĄbb 2 kontrollpont, gĂśrbe generĂĄlĂĄsa
        if (controlPoints.size() >= 2) {
            // 100 pontra felosztjuk a gĂśrbĂŠt
            const int segments = 100;
            
            for (size_t i = 0; i < controlPoints.size() - 1; i++) {
                // MeghatĂĄrozzuk a 4 kontrollpontot a szakaszhoz
                vec2 p0 = (i > 0) ? controlPoints[i-1] : controlPoints[i];
                vec2 p1 = controlPoints[i];
                vec2 p2 = controlPoints[i+1];
                vec2 p3 = (i < controlPoints.size() - 2) ? controlPoints[i+2] : controlPoints[i+1];
                
                // A kezdĹ ĂŠs vĂŠgpontokban nulla sebessĂŠgvektor (Hermite feltĂŠtel)
                if (i == 0) p0 = p1 - (p2 - p1) * 0.01f;
                if (i == controlPoints.size() - 2) p3 = p2 + (p2 - p1) * 0.01f;
                
                // Szakasz felosztĂĄsa
                for (int j = 0; j <= segments; j++) {
                    float t = (float)j / segments;
                    curvePoints.push_back(CatmullRom(p0, p1, p2, p3, t));
                }
            }
        }
        
        // GĂśrbe geometria frissĂ­tĂŠse
        if (splineGeometry) {
            delete splineGeometry;
        }
        
        splineGeometry = new Geometry<vec2>;
        splineGeometry->Vtx() = curvePoints;
        splineGeometry->updateGPU();
        
        // Kontrollpontok geometria frissĂ­tĂŠse
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
    
    // Ăj kontrollpont hozzĂĄadĂĄsa
    void addControlPoint(vec2 point) {
        controlPoints.push_back(point);
        updateGeometry();
    }
    
    // Kontrollpontok szĂĄmĂĄnak lekĂŠrdezĂŠse
    size_t getNumControlPoints() const {
        return controlPoints.size();
    }
    
    // GĂśrbe paramĂŠteres egyenlete: r(t)
    vec2 r(float t) {
        if (controlPoints.size() < 2) return vec2(0, 0);
        
        // t paramĂŠter korlĂĄtozĂĄsa a [0, n-1] tartomĂĄnyra, ahol n a kontrollpontok szĂĄma
        if (t < 0) t = 0;
        float maxParam = (float)(controlPoints.size() - 1);
        if (t > maxParam) t = maxParam;
        
        // EgĂŠsz ĂŠs tĂśrt rĂŠsz kiszĂĄmĂ­tĂĄsa
        int i = (int)t;
        float u = t - i;
        
        // Kontrollpontok kivĂĄlasztĂĄsa
        vec2 p0, p1, p2, p3;
        
        // Az indexek ellenĹrzĂŠsĂŠvel
        if (i == 0) {
            p0 = controlPoints[0];
            p1 = controlPoints[0];
            p2 = (controlPoints.size() > 1) ? controlPoints[1] : controlPoints[0];
            p3 = (controlPoints.size() > 2) ? controlPoints[2] : p2;
            
            // KezdĹponti derivĂĄlt nulla
            p0 = p1 - (p2 - p1) * 0.01f;
        }
        else if (i >= (int)controlPoints.size() - 1) {
            size_t n = controlPoints.size();
            p0 = (n > 2) ? controlPoints[n-3] : controlPoints[0];
            p1 = (n > 1) ? controlPoints[n-2] : controlPoints[0];
            p2 = controlPoints[n-1];
            p3 = controlPoints[n-1];
            
            // VĂŠgponti derivĂĄlt nulla
            p3 = p2 + (p2 - p1) * 0.01f;
        }
        else {
            p0 = (i > 0) ? controlPoints[i-1] : controlPoints[i];
            p1 = controlPoints[i];
            p2 = controlPoints[i+1];
            p3 = (i < (int)controlPoints.size() - 2) ? controlPoints[i+2] : p2;
        }
        
        // Spline kiĂŠrtĂŠkelĂŠse
        return CatmullRom(p0, p1, p2, p3, u);
    }
    
    // GĂśrbe derivĂĄltja
    vec2 rDerivative(float t) {
        if (controlPoints.size() < 2) return vec2(0, 0);
        
        // t paramĂŠter korlĂĄtozĂĄsa a [0, n-1] tartomĂĄnyra, ahol n a kontrollpontok szĂĄma
        if (t < 0) t = 0;
        float maxParam = (float)(controlPoints.size() - 1);
        if (t > maxParam) t = maxParam;
        
        // EgĂŠsz ĂŠs tĂśrt rĂŠsz kiszĂĄmĂ­tĂĄsa
        int i = (int)t;
        float u = t - i;
        
        // Kontrollpontok kivĂĄlasztĂĄsa
        vec2 p0, p1, p2, p3;
        
        // Az indexek ellenĹrzĂŠsĂŠvel
        if (i == 0) {
            p0 = controlPoints[0];
            p1 = controlPoints[0];
            p2 = (controlPoints.size() > 1) ? controlPoints[1] : controlPoints[0];
            p3 = (controlPoints.size() > 2) ? controlPoints[2] : p2;
            
            // KezdĹponti derivĂĄlt nulla
            p0 = p1 - (p2 - p1) * 0.01f;
        }
        else if (i >= (int)controlPoints.size() - 1) {
            size_t n = controlPoints.size();
            p0 = (n > 2) ? controlPoints[n-3] : controlPoints[0];
            p1 = (n > 1) ? controlPoints[n-2] : controlPoints[0];
            p2 = controlPoints[n-1];
            p3 = controlPoints[n-1];
            
            // VĂŠgponti derivĂĄlt nulla
            p3 = p2 + (p2 - p1) * 0.01f;
        }
        else {
            p0 = (i > 0) ? controlPoints[i-1] : controlPoints[i];
            p1 = controlPoints[i];
            p2 = controlPoints[i+1];
            p3 = (i < (int)controlPoints.size() - 2) ? controlPoints[i+2] : p2;
        }
        
        // DerivĂĄlt kiĂŠrtĂŠkelĂŠse
        return CatmullRomDerivative(p0, p1, p2, p3, u);
    }
    
    // EgysĂŠgvektor a pĂĄlya ĂŠrintĹje irĂĄnyĂĄban 
    vec2 T(float t) {
        vec2 derivative = rDerivative(t);
        float len = sqrt(derivative.x * derivative.x + derivative.y * derivative.y);
        if (len < 0.0001f) return vec2(1, 0); // Ha tĂşl kicsi a hossz, vĂ­zszintes vektort adunk vissza
        return vec2(derivative.x / len, derivative.y / len);
    }
    
    // NormĂĄlvektor a pĂĄlya ĂŠrintĹjĂŠre merĹlegesen 
    vec2 N(float t) {
        vec2 tangent = T(t);
        return vec2(-tangent.y, tangent.x); // 90 fokos elforgatĂĄs
    }
    
   
    void Draw(GPUProgram* gpuProgram) {
        // Ha nincs elĂŠg pont, nem rajzolunk
        if (controlPoints.size() < 2) return;
        
        // GĂśrbe kirajzolĂĄsa sĂĄrga szĂ­nnel
        glLineWidth(3.0f);
        splineGeometry->Draw(gpuProgram, GL_LINE_STRIP, vec3(1.0f, 1.0f, 0.0f));
        
        // Kontrollpontok kirajzolĂĄsa piros nĂŠgyzetkĂŠnt
        glPointSize(10.0f);
        pointGeometry->Draw(gpuProgram, GL_POINTS, vec3(1.0f, 0.0f, 0.0f));
    }
};

class Gondola {
private:
    Geometry<vec2> *wheel;        
    Geometry<vec2> *spokes;       
    Spline *track;                
    
    GondolaState state;        
    float splineParam;           // pĂĄlya paramĂŠter (t)
    float wheelRadius;           
    vec2 position;               
    float angle;                 
    float velocity;              
    float lambda;                // alaktĂŠnyezĹ
    
    void createWheel(float radius) {
        wheel = new Geometry<vec2>();
        
        // KĂśr lĂŠtrehozĂĄsa
        const int segments = 36;
        std::vector<vec2> vertices;
        
        // KĂśzĂŠppont
        vertices.push_back(vec2(0, 0));
        
        // KĂśrvonal pontjai
        for (int i = 0; i <= segments; i++) {
            float phi = i * 2 * M_PI / segments;
            vertices.push_back(vec2(radius * cos(phi), radius * sin(phi)));
        }
        
        // Geometria beĂĄllĂ­tĂĄsa
        wheel->Vtx() = vertices;
        wheel->updateGPU();
    }
    
    // KĂźllĹk lĂŠtrehozĂĄsa
    void createSpokes(float radius) {
        spokes = new Geometry<vec2>();
        
        // KĂźllĹk pontjai
        std::vector<vec2> vertices;
        
        
        vertices.push_back(vec2(0, 0));
        vertices.push_back(vec2(radius, 0));
        
        vertices.push_back(vec2(0, 0));
        vertices.push_back(vec2(0, radius));
        
       
        spokes->Vtx() = vertices;
        spokes->updateGPU();
    }
    
    // GĂśrbĂźlet szĂĄmĂ­tĂĄsa 
    float getCurvature(float t) {
        vec2 firstDerivative = track->rDerivative(t);
        vec2 normalVector = track->N(t);
        
        // MĂĄsodik derivĂĄlt kĂśzelĂ­tĂŠse
        float deltaT = 0.001f;
        vec2 nextDerivative = track->rDerivative(t + deltaT);
        vec2 secondDerivative = (nextDerivative - firstDerivative) / deltaT;
        
        float firstDerivMagnitudeSq = firstDerivative.x * firstDerivative.x + firstDerivative.y * firstDerivative.y;
        if (firstDerivMagnitudeSq < 0.0001f) return 0.0f;
        
        float dotProd = secondDerivative.x * normalVector.x + secondDerivative.y * normalVector.y;
        return fabs(dotProd) / firstDerivMagnitudeSq;
    }
    
    // SzĂźksĂŠges centripetĂĄlis erĹ 
    float calculateCentripetalForce(float param) {
        return velocity * velocity * getCurvature(param);
    }
    
    // KerĂŠk a pĂĄlyĂĄn van-e
    bool staysOnTrack(float param) {
       
        vec2 normal = track->N(param);
        
       
        vec2 gravity(0, -g);
        float normalGravity = normal.x * gravity.x + normal.y * gravity.y;
        float centripetalForce = calculateCentripetalForce(param);
        
        // A pĂĄlyĂĄn marad, ha a kĂŠnyszererĹ pozitĂ­v (nyomja a pĂĄlyĂĄt)
        return (normalGravity + centripetalForce) > 0;
    }
    
    // EllenĹrizzĂźk, hogy a kerĂŠk nem mozog-e visszafelĂŠ
    bool isMovingBackwards() {
        return velocity < 0;
    }
    
public:
    Gondola(Spline *trackRef) : track(trackRef) {
        state = WAITING;
        splineParam = 0.01f;  
        wheelRadius = 1.0f;   
        position = vec2(0, 0);
        angle = 0.0f;
        velocity = 0.0f;
        lambda = 0.5f;       
        
        // KerĂŠk ĂŠs kĂźllĹk lĂŠtrehozĂĄsa
        createWheel(wheelRadius);
        createSpokes(wheelRadius);
    }
    
    ~Gondola() {
        delete wheel;
        delete spokes;
    }
    
    // KerĂŠk indĂ­tĂĄsa
    void Start() {
        if (state == WAITING && track->getNumControlPoints() >= 2) {
            state = ROLLING;
            // Mindig a 0.01 paramĂŠterĹą pontnĂĄl indĂ­tunk
            splineParam = 0.01f;
            position = track->r(splineParam);
            angle = 0.0f;
            velocity = 0.0f; 
        }
    }
    
   
   void Animate(float dt) {
    if (state != ROLLING || track->getNumControlPoints() < 2) return;
    
    // PĂĄlya ĂŠrintĹ ĂŠs normĂĄl vektorai
    vec2 tangent = track->T(splineParam);
    vec2 normal = track->N(splineParam);
    
    // PĂĄlya aktuĂĄlis pontja
    vec2 pathPosition = track->r(splineParam);
    
    // GravitĂĄciĂł vektor (g lefelĂŠ mutat, negatĂ­v y irĂĄnyba)
    vec2 gravity(0, -g);
    
    // GravitĂĄciĂł komponensek az ĂŠrintĹ ĂŠs normĂĄl irĂĄnyokban
    float tangentialGravity = tangent.x * gravity.x + tangent.y * gravity.y;
    float normalGravity = normal.x * gravity.x + normal.y * gravity.y;
    
    // GyorsulĂĄs szĂĄmĂ­tĂĄsa (csak tangenciĂĄlis komponens a haladĂĄshoz)
    float acceleration = tangentialGravity / (1.0f + lambda);
    
    // SebessĂŠg frissĂ­tĂŠse a gyorsulĂĄs alapjĂĄn
    velocity += acceleration * dt;
    
    // GĂśrbĂźlet ĂŠs centripetĂĄlis erĹ szĂĄmĂ­tĂĄsa
    float curvature = getCurvature(splineParam);
    float centripetalForce = velocity * velocity * curvature;
    
    // EllenĹrizzĂźk, hogy a kerĂŠk a pĂĄlyĂĄn marad-e
    if (normalGravity + centripetalForce <= 0.0f) {
        // A kerĂŠk leesik a pĂĄlyĂĄrĂłl
        state = FALLEN;
        return;
    }
    
    // EllenĹrizzĂźk, hogy a kerĂŠk nem mozog-e visszafelĂŠ
    if (velocity < 0.0f) {
        // VisszatĂŠrĂŠs a kezdĹponthoz ĂŠs ĂşjraindĂ­tĂĄs
        splineParam = 0.01f;
        velocity = 0.0f;
        return;
    }
    
    // Spline paramĂŠter frissĂ­tĂŠse a sebessĂŠg alapjĂĄn
    vec2 derivVec = track->rDerivative(splineParam);
    float derivLength = sqrt(derivVec.x * derivVec.x + derivVec.y * derivVec.y);
    
    // BiztonsĂĄgi ellenĹrzĂŠs a nullĂĄval valĂł osztĂĄs elkerĂźlĂŠsĂŠre
    if (derivLength < 0.0001f) derivLength = 0.0001f;
    
    float paramStep = velocity * dt / derivLength;
    splineParam += paramStep;
    
    // EllenĹrizzĂźk, hogy a paramĂŠter a megengedett tartomĂĄnyban van-e
    float maxParam = float(track->getNumControlPoints() - 1);
    if (splineParam >= maxParam) {
        splineParam = 0.01f;
        velocity = 0.0f;
    }
    
    // PozĂ­ciĂł frissĂ­tĂŠse: a kerĂŠk kĂśzĂŠppontja a pĂĄlyaponttĂłl a normĂĄlvektor irĂĄnyĂĄban van
    position = pathPosition + normal * wheelRadius;
    
    // KerĂŠk elfordulĂĄsi szĂśgĂŠnek frissĂ­tĂŠse
    float angularVelocity = -velocity / wheelRadius;
    angle += angularVelocity * dt;
}

    
    // KerĂŠk kirajzolĂĄsa
    void Draw(GPUProgram* gpuProgram, const mat4& viewMatrix) {
        if (state == WAITING || track->getNumControlPoints() < 2) return;
        
        // Modell transzformĂĄciĂł beĂĄllĂ­tĂĄsa
        mat4 modelMatrix = mat4(1.0f);
        modelMatrix[3][0] = position.x;
        modelMatrix[3][1] = position.y;
        
        // ForgatĂĄs kĂźlĂśn
        mat4 rotMatrix = mat4(1.0f);
        rotMatrix[0][0] = cos(angle);
        rotMatrix[0][1] = sin(angle);
        rotMatrix[1][0] = -sin(angle);
        rotMatrix[1][1] = cos(angle);
        
        modelMatrix = modelMatrix * rotMatrix;
        
        // MVP mĂĄtrix beĂĄllĂ­tĂĄsa
        mat4 mvpMatrix = viewMatrix * modelMatrix;
        gpuProgram->setUniform(mvpMatrix, "MVP");
        
        // KerĂŠk kirajzolĂĄsa
        glPointSize(1.0f);
        wheel->Draw(gpuProgram, GL_TRIANGLE_FAN, vec3(0, 0, 1));
        
        // KĂśrvonal kirajzolĂĄsa
        glLineWidth(2.0f);
        wheel->Draw(gpuProgram, GL_LINE_LOOP, vec3(1, 1, 1)); 
        
        // KĂźllĹk kirajzolĂĄsa
        spokes->Draw(gpuProgram, GL_LINES, vec3(1, 1, 1));
    }
    
    // Ăllapot lekĂŠrdezĂŠse
    GondolaState getState() const {
        return state;
    }
};

class RollerCoasterApp : public glApp {
    Spline *track;          
    GPUProgram *gpuProgram; 
    Camera *camera;        
    Gondola *gondola;       
public:
    RollerCoasterApp() : glApp("Lab02") {}

    void onInitialization() {
        camera = new Camera(vec2(0.0f, 0.0f), worldWidth, worldHeight);
        
        track = new Spline();
        
        gondola = new Gondola(track);
    
        gpuProgram = new GPUProgram(vertSource, fragSource);
    }

    // Ablak ĂşjrarajzolĂĄs
    void onDisplay() {
        glClearColor(0, 0, 0, 0);     
        glClear(GL_COLOR_BUFFER_BIT); 
        glViewport(0, 0, winWidth, winHeight);
        
        // MVP mĂĄtrix beĂĄllĂ­tĂĄsa
        gpuProgram->setUniform(camera->getMVP(), "MVP");
        
        // PĂĄlya kirajzolĂĄsa
        track->Draw(gpuProgram);
        
        // Gondola kirajzolĂĄsa
        gondola->Draw(gpuProgram, camera->getMVP());
    }
    
    // BillentyĹązet kezelĂŠse
    void onKeyboard(int key) {
        if (key == ' ') { // SPACE
            gondola->Start(); 
            refreshScreen();
        }
    }
    
    // EgĂŠr lenyomĂĄs kezelĂŠse
    void onMousePressed(MouseButton button, int pX, int pY) {
        if (button == MOUSE_LEFT) {
            // KĂŠpernyĹ koordinĂĄtĂĄk ĂĄtvĂĄltĂĄsa vilĂĄgkoordinĂĄtĂĄkba
            vec2 worldPos = camera->screenToWorld(pX, pY);
            
            // Ăj kontrollpont hozzĂĄadĂĄsa
            track->addControlPoint(worldPos);
            
            // ĂjrarajzolĂĄs kĂŠrĂŠse
            refreshScreen();
        }
    }
    
    // IdĹ vĂĄltozĂĄsa
    void onTimeElapsed(float tstart, float tend) { 
    const float dt = 0.01; 
    for (float t = tstart; t < tend; t += dt) {
        float Dt = fmin(dt, tend - t);
        gondola->Animate(Dt);
    }
    refreshScreen();
}
    
    // FelszabadĂ­tĂĄs
    ~RollerCoasterApp() {
        delete track;
        delete gpuProgram;
        delete camera;
        delete gondola;
    }
};

RollerCoasterApp app;