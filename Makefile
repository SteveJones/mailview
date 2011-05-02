CFLAGS=`pkg-config --cflags gtkmm-2.4` `pkg-config --cflags webkit-1.0` -Wall
LDFLAGS=`pkg-config --libs gtkmm-2.4` `pkg-config --libs webkit-1.0` -lmimetic

all: mailview

mailview: mailview.o
	g++ $(LDFLAGS) mailview.o -o mailview

mailview.o: mailview.cpp
	g++ $(CFLAGS) -c mailview.cpp -o mailview.o
