.PHONY: all project clean

all: project

project: bin/project.o
bin/project.o: src/*.c src/*.h
	@echo "Compiling project"
	@gcc src/*.c -o bin/project.o

clean:
	@rm -f bin/*
