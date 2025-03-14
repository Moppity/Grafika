# A projekt neve
TARGET = greenTri

# A forrás fájlok
SRCS = greenTri.cpp glad.c framework.cpp

# A könyvtárak és az include fájlok
INCLUDES = -I../include -I/usr/include/glm

# A használt könyvtárak
LIBS = -lstdc++fs -lglfw -lGL

# A fordító és a flags
CXX = g++
CXXFLAGS = -Wall -std=c++17

# Az alapértelmezett cél
all: $(TARGET)

# A cél (kimeneti fájl) elkészítése
$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) $(INCLUDES) $(LIBS) -o $(TARGET)

# A "clean" cél a build fájlok törlésére
clean:
	rm -f $(TARGET)

# A "make run" parancs futtatásához
run: $(TARGET)
	./$(TARGET)

