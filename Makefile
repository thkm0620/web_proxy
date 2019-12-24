all: web_proxy

web_proxy: main.cpp
	g++ -std=c++11 -o web_proxy main.cpp -pthread

clean:
	rm -f *.o web_proxy
