#include "./framework.h"
#include <cmath>

const char *vertSource = R"(
    #version 330
    precision highp float;

    layout(location = 0) in vec2 cP;

    void main() {
        gl_Position = vec4(cP.x, cP.y, 0, 1);
    }
)";

const char *fragSource = R"(
    #version 330
    precision highp float;

    uniform vec3 color;
    out vec4 fragmentColor;

    void main() {
        fragmentColor = vec4(color, 1);
    }
)";

const int windowWidth = 600, windowHeight = 600;
char currentKey = ' ';

class PointsApp : public glApp {
    Geometry<vec2> *points;   
    Geometry<vec2> *lines;    
    GPUProgram *gpuProgram;
    
    int selectedLine = -1;    
    int firstLine = -1;       

public:
    PointsApp() : glApp("PointsApp") {}

    void onInitialization() {
        points = new Geometry<vec2>();
        lines = new Geometry<vec2>();
        gpuProgram = new GPUProgram(vertSource, fragSource);
    }
    //pixel koordinátáit transzformáljuk normalizált eszközkoordinátába
    vec2 toNDC(int pX, int pY) {
        return vec2(
            2.0f * (pX / (float)windowWidth) - 1.0f,
            1.0f - 2.0f * (pY / (float)windowHeight)
        );
    }
    
   
    vec2 getClosestPoint(float ndcX, float ndcY) {
        float minDist = 5.0f;//tutti ne lehessen benne
        vec2 closest;

        for (const vec2& p : points->Vtx()) {
            float dist = sqrt(pow(p.x - ndcX, 2) + pow(p.y - ndcY, 2));
            if (dist < minDist) {
                minDist = dist;
                closest = p;
            }
        }
        return closest;
    }
    
    //2 egyenes 4 pontja alapján, metszéspont keresése
    vec2 getIntersection(vec2 p1, vec2 p2, vec2 p3, vec2 p4) {
        vec2 dir1 = p2 - p1; //első irányvev
        vec2 dir2 = p4 - p3;//második irányvev
        
        float det = dir1.x * dir2.y - dir1.y * dir2.x;
        //determinánssal megnézzük párhuzamosak-e
        if (fabs(det) < 0.0001f) {
            return vec2(10, 10); 
        }
        
        float t = ((p3.x - p1.x) * dir2.y - (p3.y - p1.y) * dir2.x) / det;
        return p1 + t * dir1;
    }
    
    void drawLine(float ndcX, float ndcY) {
        vec2 closest = getClosestPoint(ndcX, ndcY);
        lines->Vtx().push_back(closest);
        //megvan-e a két pont
        if (lines->Vtx().size() % 2 == 0) {
            //pontokkal kirajzolja az egyenest
            vec2 p1 = lines->Vtx()[lines->Vtx().size() - 1];
            vec2 p2 = lines->Vtx()[lines->Vtx().size() - 2];
            vec2 dir = p2 - p1;
            //alul felul megkeressi a metszéspontokat
            vec2 topIntersect = getIntersection(p1, p2, vec2(-1, 1), vec2(1, 1));
            vec2 bottomIntersect = getIntersection(p1, p2, vec2(-1, -1), vec2(1, -1));
            
            //ha nem az alsó felső résznel van, akkor oldalra csinálja meg
            if (topIntersect.x >= -1 && topIntersect.x <= 1 && 
                bottomIntersect.x >= -1 && bottomIntersect.x <= 1) {
                lines->Vtx()[lines->Vtx().size() - 1] = topIntersect;
                lines->Vtx()[lines->Vtx().size() - 2] = bottomIntersect;
            } 
              else {
                vec2 leftIntersect = getIntersection(p1, p2, vec2(-1, -1), vec2(-1, 1));
                vec2 rightIntersect = getIntersection(p1, p2, vec2(1, -1), vec2(1, 1));
                lines->Vtx()[lines->Vtx().size() - 1] = leftIntersect;
                lines->Vtx()[lines->Vtx().size() - 2] = rightIntersect;
            }
            
            float a = p2.y - p1.y;
            float b = p1.x - p2.x;
            float c = p1.y * p2.x - p1.x * p2.y;
            printf("Egyenes: %.2fx + %.2fy + %.2f = 0\n", a, b, c);
        }
        
        lines->updateGPU();
    }
    
    int findClosestLine(float ndcX, float ndcY) {
        float minDist = 0.1f;
        int closest = -1;
        //egyeneseket megnézzük, kettesével a pontjai miatt
        for (int i = 0; i < lines->Vtx().size() - 1; i += 2) {
            vec2 p1 = lines->Vtx()[i];
            vec2 p2 = lines->Vtx()[i + 1];
           //irányvektort kiszámoljuk
            vec2 dir = p2 - p1;
            //csinálunk vele egy nvektort, és normalizáljuk
            vec2 normal = vec2(-dir.y, dir.x);
            normal = normal / sqrt(normal.x * normal.x + normal.y * normal.y);
            //kezdőpont-kurzur távolsága
            vec2 toPoint = vec2(ndcX, ndcY) - p1;
            float dist = fabs(toPoint.x * normal.x + toPoint.y * normal.y);
            
            if (dist < minDist) {
                minDist = dist;
                closest = i;
            }
        }
        
        return closest;
    }
    
    void moveSelectedLine(float ndcX, float ndcY) {
        if (selectedLine == -1) return;
        //egyenes pontjait lekérjük
        vec2 p1 = lines->Vtx()[selectedLine];
        vec2 p2 = lines->Vtx()[selectedLine + 1];
        //egyenes irányvec
        vec2 dir = p2 - p1;
        //új vec a kurzustól
        vec2 cursor(ndcX, ndcY);
        
        vec2 newP1 = getIntersection(cursor, cursor + dir, vec2(-1, 1), vec2(1, 1));
        vec2 newP2 = getIntersection(cursor, cursor + dir, vec2(-1, -1), vec2(1, -1));
        //ha túlságosan meredek, oldallal számolja inkább
        if (newP1.x < -1 || newP1.x > 1 || newP2.x < -1 || newP2.x > 1) {
            newP1 = getIntersection(cursor, cursor + dir, vec2(-1, -1), vec2(-1, 1));
            newP2 = getIntersection(cursor, cursor + dir, vec2(1, -1), vec2(1, 1));
        }
        
        lines->Vtx()[selectedLine] = newP1;
        lines->Vtx()[selectedLine + 1] = newP2;
        
        lines->updateGPU();
    }

    void onKeyboard(int key) override {
        currentKey = key;
        selectedLine = -1;
        firstLine = -1;
    }

    void onMousePressed(MouseButton but, int pX, int pY) override {
        if (but != MOUSE_LEFT) return;
        
        vec2 mousePos = toNDC(pX, pY);
        
        if (currentKey == 'p') {
            points->Vtx().push_back(mousePos);
            points->updateGPU();
            printf("Pont: %.2f, %.2f\n", mousePos.x, mousePos.y);
        }
        else if (currentKey == 'l') {
            drawLine(mousePos.x, mousePos.y);
        }
        else if (currentKey == 'm') {
            selectedLine = findClosestLine(mousePos.x, mousePos.y);
            if (selectedLine != -1) {
                printf("Vonal kiválasztva\n");
            }
        }
        else if (currentKey == 'i') {
            int lineIdx = findClosestLine(mousePos.x, mousePos.y);
            
            if (lineIdx != -1) {
                if (firstLine == -1) {
                    firstLine = lineIdx;
                    printf("Első vonal\n");
                } else {
                    if (firstLine != lineIdx) {
                        vec2 l1p1 = lines->Vtx()[firstLine];
                        vec2 l1p2 = lines->Vtx()[firstLine + 1];
                        vec2 l2p1 = lines->Vtx()[lineIdx];
                        vec2 l2p2 = lines->Vtx()[lineIdx + 1];
                        
                        vec2 intersection = getIntersection(l1p1, l1p2, l2p1, l2p2);
                        
                        if (intersection.x != 10) {
                            points->Vtx().push_back(intersection);
                            points->updateGPU();
                            printf("Metszet: %.2f, %.2f\n", intersection.x, intersection.y);
                        } else {
                            printf("Párhuzamos egyenesek\n");
                        }
                    } else {
                        printf("Ugyanaz a vonal\n");
                    }
                    
                    firstLine = -1;
                }
            }
        }
        
        refreshScreen();
    }

    void onMouseReleased(MouseButton but, int pX, int pY) override {
        if (but == MOUSE_LEFT && currentKey == 'm') {
            selectedLine = -1;
        }
    }

    void onMouseMotion(int pX, int pY) override {
        vec2 mousePos = toNDC(pX, pY);
        
        if (currentKey == 'm' && selectedLine != -1) {
            moveSelectedLine(mousePos.x, mousePos.y);
            refreshScreen();
        }
    }

    void onDisplay() override {
        glClearColor(0.5f, 0.5f, 0.5f, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glViewport(0, 0, windowWidth, windowHeight);
        
        glPointSize(10.0f);
        glLineWidth(3.0f);
    
        lines->Draw(gpuProgram, GL_LINES, vec3(0.0f, 1.0f, 1.0f));
      
        points->Draw(gpuProgram, GL_POINTS, vec3(1.0f, 0.0f, 0.0f));
    }
};

PointsApp app;