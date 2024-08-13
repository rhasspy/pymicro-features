all:
	g++ -o test test.cpp -Imicrofrontend/include microfrontend/src/*.c microfrontend/src/*.cc
