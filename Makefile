CXX = g++
WINDRES = windres

CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pedantic -Iinclude -MMD -MP
LDFLAGS = -mwindows

SRCS = $(wildcard src/*.cpp)
OBJS = $(SRCS:src/%.cpp=obj/%.o)
DEPS = $(OBJS:.o=.d)

RES = resources.o manifest.o
NAME = SimpleOverlay.exe

LIBS = -lcomctl32 -lcomdlg32 -lgdi32 -lgdiplus -luser32 -ldwmapi

all: $(NAME)

$(NAME): $(OBJS) $(RES)
	$(CXX) $(OBJS) $(RES) -o $(NAME) $(LDFLAGS) $(LIBS)

resources.o: resources.rc include/resource.h
	$(WINDRES) -Iinclude resources.rc -O coff -o resources.o

manifest.o: app.rc app.manifest
	$(WINDRES) app.rc -O coff -o manifest.o

obj/%.o: src/%.cpp
	if not exist obj mkdir obj
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	if exist obj rmdir /S /Q obj
	if exist resources.o del /Q resources.o
	if exist manifest.o del /Q manifest.o

fclean: clean
	if exist $(NAME) del /Q $(NAME)

re: fclean all

-include $(DEPS)

.PHONY: all clean fclean re