
#include "./framework.h"

// cs�cspont �rnyal�
const char *vertSource = R"(
	#version 330				
    precision highp float;

	layout(location = 0) in vec2 cP;	// 0. bemeneti regiszter

	void main() {
		gl_Position = vec4(cP.x, cP.y, 0, 1); 	// bemenet m�r normaliz�lt eszk�zkoordin�t�kban
	}
)";


const char *fragSource = R"(
	#version 330
    precision highp float;

	uniform vec3 color;			// konstans sz�n
	out vec4 fragmentColor;		// pixel sz�n

	void main() {
		fragmentColor = vec4(color, 1); // RGB -> RGBA
	}
)";

const int winWidth = 600, winHeight = 600;
char keystate;

class GreenTriangleApp : public glApp {
  Geometry<vec2> *points; 
  GPUProgram *gpuProgram;   
public:
  GreenTriangleApp() : glApp("Green triangle") {}

  
  void onInitialization() {
    points = new Geometry<vec2>;
        gpuProgram = new GPUProgram(vertSource, fragSource);
  }

  void onKeyboard(int key){
    keystate=key;
  }


  void updatePoints(int pX, int pY){
      float ndcX = 2.0f * (pX / (float) winWidth) - 1.0f;
      float ndcY = 1.0f - 2.0f * (pY / (float)winHeight);

      points->Vtx().push_back(vec2(ndcX,ndcY));
      points->updateGPU();
      refreshScreen();
  }

  void onMousePressed(MouseButton but, int pX, int pY){
    if(but==MOUSE_LEFT){
      if(keystate=='p'){
        updatePoints(pX,pY);
      }
    }
  }

  void onDisplay() {
    glClearColor(0, 0, 0, 0);     
    glClear(GL_COLOR_BUFFER_BIT); 
    glViewport(0, 0, winWidth, winHeight);
    glPointSize(10.0f);
    points->Draw(gpuProgram, GL_POINTS, vec3(0.0f, 1.0f, 0.0f));
    
  }
};

GreenTriangleApp app;
