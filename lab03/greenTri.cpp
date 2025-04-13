#include "./framework.h"

const char* fragmentSource = R"(
    #version 330
    precision highp float;
    
    uniform sampler2D textureUnit;
    uniform int objectType;      
    uniform vec3 color;          
    
    uniform int currentHour;        
    uniform float axisTilt;    
    
    in vec2 texCoord;         
    out vec4 fragmentColor;
    
    const float pie = 3.14;
    
   
    vec3 sphericalToCartesian(vec2 spherical) {
        float lon = spherical.x;
        float lat = spherical.y;
        
        float x = cos(lat) * cos(lon);
        float y = cos(lat) * sin(lon);
        float z = sin(lat);
        
        return vec3(x, y, z);
    }

    
    vec2 mercatorToSpherical(vec2 mercator) {
        float longitude = (mercator.x - 0.5) * 2.0 * 180.0; 
        float latitude = (mercator.y - 0.5) * 2.0 * 85.0;    
        
        float lon = longitude * pie / 180.0;
        float lat = latitude * pie / 180.0;
        
        return vec2(lon, lat);
    }
    
    bool isDaytime(vec2 mercator) {
        vec2 spherical = mercatorToSpherical(mercator);
        
        vec3 surfaceNormal = sphericalToCartesian(spherical);
        
        float declination = axisTilt;
        
        float hourAngle = float(currentHour) / 24.0 * 2.0 * pie;
        
        vec3 sunDirection;
        sunDirection.x = -cos(hourAngle); 
        sunDirection.y = -sin(hourAngle); 
        sunDirection.z = sin(declination); 
        
        sunDirection = normalize(sunDirection);
        
        return dot(surfaceNormal, sunDirection) > 0.0;
    }
    
    void main() {
        if (objectType == 0) { 
            vec4 texColor = texture(textureUnit, texCoord);
            
            bool daytime = isDaytime(texCoord);
            
            const float help = 0.6;


            if (!daytime) {
                fragmentColor = texColor * 0.5 * help ;
            } else {
                fragmentColor = texColor;
            }
        } else { 
            fragmentColor = vec4(color, 1.0);
        }
    }
)";

const char* vertexSource = R"(
    #version 330
    precision highp float;

    uniform mat4 MVP;
    layout(location = 0) in vec2 vp;      
    layout(location = 1) in vec2 vertexUV; 
    
    out vec2 texCoord;
    
    void main() {
        gl_Position = vec4(vp.x, vp.y, 0, 1) * MVP;
        texCoord = vertexUV;
    }
)";


const int winWidth = 600, winHeight = 600;
const int textureWidth = 64, textureHeight = 64;
const float pie = 3.14f;

const float EARTH_RADIUS = 6371.0f; 
const float EARTH_CIRCUMFERENCE = 40000.0f; 
const float AXIS_TILT = 23.0f * pie / 180.0f; 


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


int currentHour = 0;

vec2 MercatorToSpherical(float u, float v) {
    float longitude = (u - 0.5f) * 2.0f * 180.0f; 
    float latitude = (v - 0.5f) * 2.0f * 85.0f;   
    
    float lon = longitude * pie / 180.0f;
    float lat = latitude * pie / 180.0f;
    
    return vec2(lon, lat);
}

vec2 SphericalToMercator(float lon, float lat) {
    const float MAX_LAT = 85.0f * pie / 180.0f;
    lat = fmax(-MAX_LAT, fmin(MAX_LAT, lat));
    
    float u = (lon / pie / 2.0f) + 0.5f;
    float v = (lat / MAX_LAT / 2.0f) + 0.5f;
    
    return vec2(u, v);
}

vec3 SphericalToCartesian(float lon, float lat) {
    float x = cos(lat) * cos(lon);
    float y = cos(lat) * sin(lon);
    float z = sin(lat);
    
    return vec3(x, y, z);
}

vec3 SphericalToCartesian(vec2 spherical) {
    return SphericalToCartesian(spherical.x, spherical.y);
}

vec2 CartesianToSpherical(vec3 p) {
    p = normalize(p);
    
    float lat = asin(p.z);
    float lon = atan2(p.y, p.x);
    
    return vec2(lon, lat);
}

vec2 PixelToMercator(int x, int y) {
    float u = (float)x / winWidth;
    float v = 1.0f - (float)y / winHeight; 
    return vec2(u, v);
}

vec2 MercatorToPixel(float u, float v) {
    int x = (int)(u * winWidth);
    int y = (int)((1.0f - v) * winHeight); 
    return vec2(x, y);
}

float CalculateDistance(vec3 p1, vec3 p2) {
    float dotProduct = dot(p1, p2);
    dotProduct = fmax(-1.0f, fmin(1.0f, dotProduct));
    float angle = acos(dotProduct);
    
    float distanceKm = angle * EARTH_RADIUS;
    
    return distanceKm;
}

std::vector<vec3> CalculateGreatCirclePoints(vec3 p1, vec3 p2, int segments) {
    std::vector<vec3> points;
    points.reserve(segments + 1);
    
    float dotProduct = dot(p1, p2);
    dotProduct = fmax(-1.0f, fmin(1.0f, dotProduct)); 
    float omega = acos(dotProduct);
    
    for (int i = 0; i <= segments; i++) {
        float t = (float)i / segments;
        
        vec3 p;
        if (omega < 0.00001f) {
            p = p1; 
        } else {
            p = (sin((1.0f - t) * omega) * p1 + sin(t * omega) * p2) / sin(omega);
        }
        
        p = normalize(p);
        
        points.push_back(p);
    }
    
    return points;
}

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

class Map : public Object {
private:
    unsigned int textureId;
    std::vector<vec4> decodedImage;
    
public:
    Map() {
        float vertices[] = {
            -1.0f, -1.0f,        0.0f, 0.0f,  // bal al
             1.0f, -1.0f,        1.0f, 0.0f,  // jobb al
             1.0f,  1.0f,        1.0f, 1.0f,  // jobb fel
            -1.0f,  1.0f,        0.0f, 1.0f   // bal fel
        };
        
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
        
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        
        DecodeImage();
        
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureWidth, textureHeight, 0, GL_RGBA, GL_FLOAT, &decodedImage[0]);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // FONTOS: NEAREST, nem LINEAR
    }
    
    void DecodeImage() {
        decodedImage.resize(textureWidth * textureHeight);
        
        vec4 colorTable[4] = {
            vec4(1.0f, 1.0f, 1.0f, 1.0f), 
            vec4(0.0f, 0.0f, 1.0f, 1.0f), 
            vec4(0.0f, 1.0f, 0.0f, 1.0f), 
            vec4(0.0f, 0.0f, 0.0f, 1.0f)  
        };
        
        int pixel_index = 0;
        
        for (unsigned int i = 0; i < sizeof(mapData) && pixel_index < textureWidth * textureHeight; i++) {
            unsigned char byte = mapData[i];
            
            int length = byte >> 2;     
            int colorIndex = byte & 0x3;  
            
            for (int j = 0; j <= length && pixel_index < textureWidth * textureHeight; j++) {
                decodedImage[pixel_index++] = colorTable[colorIndex];
            }
        }
        
        while (pixel_index < textureWidth * textureHeight) {
            decodedImage[pixel_index++] = vec4(0.0f, 0.0f, 0.0f, 1.0f);
        }
    }
    
    void Draw(GPUProgram* gpuProgram) override {
        
        gpuProgram->setUniform(0, "objectType");
        
        int samplerUnit = 0;
        gpuProgram->setUniform(samplerUnit, "textureUnit");
        
        glActiveTexture(GL_TEXTURE0 + samplerUnit);
        glBindTexture(GL_TEXTURE_2D, textureId);
        
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }
    
    ~Map() {
        glDeleteTextures(1, &textureId);
    }
};

class Path : public Object {
private:
    bool initialized;  
    std::vector<std::vector<float>> lineSegments; 
    std::vector<float> distances; 
    
public:
    Path() {
        color = vec3(1.0f, 1.0f, 0.0f); 
        initialized = false;
    }
    
    
    void AddSegment(vec2 startPos, vec2 endPos) {
        vec2 spherical1 = MercatorToSpherical(startPos.x, startPos.y);
        vec2 spherical2 = MercatorToSpherical(endPos.x, endPos.y);
                
        vec3 p1 = SphericalToCartesian(spherical1);
        vec3 p2 = SphericalToCartesian(spherical2);
        
        float distance = CalculateDistance(p1, p2);
        distances.push_back(distance);
        
        printf("Distance: %.0f km\n", distance);
        
        const int segments = 100;
        std::vector<vec3> greatCirclePoints = CalculateGreatCirclePoints(p1, p2, segments);
        
        std::vector<float> lineVertices;
        lineVertices.reserve((segments + 1) * 2);
        
        for (const vec3& p : greatCirclePoints) {
            vec2 spherical = CartesianToSpherical(p);
            
            vec2 uv = SphericalToMercator(spherical.x, spherical.y);
            
            float x = uv.x * 2.0f - 1.0f;
            float y = uv.y * 2.0f - 1.0f;
            
            lineVertices.push_back(x);
            lineVertices.push_back(y);
        }
        
        lineSegments.push_back(lineVertices);
        
        initialized = true;
    }
    
    void Draw(GPUProgram* gpuProgram) override {
        if (!initialized || lineSegments.empty()) return;
        
        gpuProgram->setUniform(1, "objectType");
        
        gpuProgram->setUniform(color, "color");
        
        glBindVertexArray(vao);
        glLineWidth(3.0); 
        
        for (const auto& segment : lineSegments) {
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, segment.size() * sizeof(float), segment.data(), GL_STATIC_DRAW);
            
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
            
            glDrawArrays(GL_LINE_STRIP, 0, (int)(segment.size() / 2));
        }
    }
};

class Station : public Object {
private:
    vec2 position; 
    
public:
    Station(vec2 pos) {
        position = pos;
        color = vec3(1.0f, 0.0f, 0.0f); 
        
        float vertices[] = {
            position.x * 2.0f - 1.0f, position.y * 2.0f - 1.0f
        };
        
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    }
    
    void Draw(GPUProgram* gpuProgram) override {
        gpuProgram->setUniform(2, "objectType");
        
        gpuProgram->setUniform(color, "color");
        
        glBindVertexArray(vao);
        glPointSize(10.0f); 
        glDrawArrays(GL_POINTS, 0, 1);
    }
    
    vec2 GetPosition() const {
        return position;
    }
};

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
        
        gpuProgram = new GPUProgram(vertexSource, fragmentSource);
        
        map = new Map();
        
        path = new Path();
    
        currentHour = 0;

        gpuProgram->setUniform(currentHour, "currentHour");
        gpuProgram->setUniform(AXIS_TILT, "axisTilt"); 
        
        mat4 mvp = mat4(1.0f);
        gpuProgram->setUniform(mvp, "MVP");
        
    }
    
    void onDisplay() override {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        
        gpuProgram->Use();
        
        gpuProgram->setUniform(currentHour, "currentHour");
        gpuProgram->setUniform(AXIS_TILT, "axisTilt");
        
        map->Draw(gpuProgram);
        
        path->Draw(gpuProgram);
        
        for (auto station : stations) {
            station->Draw(gpuProgram);
        }
    }
    
    void onKeyboard(int key) override {
        if (key == 'n') {
            currentHour = (currentHour + 1) % 24;
            refreshScreen();
        }
    }
    
    void onMousePressed(MouseButton button, int pX, int pY) override {
        if (button == MOUSE_LEFT) {
            vec2 mercator = PixelToMercator(pX, pY);
            
            Station* station = new Station(mercator);
            stations.push_back(station);
            
            if (stations.size() >= 2) {
                vec2 prevPos = stations[stations.size() - 2]->GetPosition();
                vec2 currPos = stations[stations.size() - 1]->GetPosition();
                
                path->AddSegment(prevPos, currPos);
                
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

MercatorMapApp app;