cc		:=	g++
cflags		:=	-Wall -Wextra -std=c++20 -fPIC -c
lflags		:=	-Wall -Wextra -std=c++20 -shared

src		:=	src
lib		:=	lib
inc		:=	inc

enginedir	:=	chocolate
engineinc	:=	$(enginedir)/$(inc)
enginebin	:=	$(enginedir)/bin
gamedir		:=	sidury
bin		:=	$(gamedir)/bin

sharedsrc	:=	$(enginedir)/src/shared

out		:=	client.so

libraries	:=	-lSDL2 -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi -lSDL2_mixer
includepaths    :=      -I $(engineinc)/imgui/ -I /usr/include/SDL2 -I $(engineinc)/ -I $(engineinc)/core/

sources		:= 	$(shell find $(src) -name "*.cpp")
sharedsources	:=	$(shell find $(sharedsrc) -name "*.cpp")
objects		:=	$(sources:.cpp=.o)
sharedobjects	:=	$(sharedsources:.cpp=.o)

release: cflags += -DNDEBUG -O2
release: lflags += -DNDEBUG -O2
release: $(bin)/$(out)
debug: cflags += -g
debug: lflags += -g
debug: $(bin)/$(out)

all:	$(bin)/$(out)

%.o : %.cpp
	$(cc) -c $< -o $@ $(cflags)

.PHONY:	clean
clean:
	-$(RM) $(bin)/$(out)
	-$(RM) $(objects)

gameloader:
	$(cc) $(src)/gameloader.c -O2 -ldl -o $(gamedir)/riff

engine_release:
	cd $(enginedir) && make release
	mv $(enginebin)/engine.so $(bin)/engine.so

engine_debug:
	cd $(enginedir) && make debug
	mv $(enginebin)/engine.so $(bin)/engine.so

run:	all
	./$(bin)/$(out)

$(bin)/$(out):	$(objects) $(sharedobjects)
	$(cc) $(lflags) $^ -o $@ $(libraries)


list: $(shell find src -name "*.cpp")
	echo $^
