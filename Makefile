all: driver integrity

driver: driver.cpp ecb.o cbc.o ctr.o hex_string_conv.o
	g++ -g -o driver ecb.o cbc.o ctr.o hex_string_conv.o driver.cpp -lcrypto -lssl
integrity: integrity.cpp ecb.o cbc.o
	g++ -o integrity ecb.o cbc.o integrity.cpp -lcrypto -lssl
ecb.o: ecb.cpp
	g++ -g -c -o ecb.o ecb.cpp -lcrypto -lssl
cbc.o: cbc.cpp cbc.h
	g++ -g -c -o cbc.o cbc.cpp
ctr.o: ctr.cpp ctr.h cbc.h
	g++ -g -c -o ctr.o ctr.cpp
hex_string_conv.o: hex_string_conv.cpp hex_string_conv.h
	g++ -g -std=c++11 -c -o hex_string_conv.o hex_string_conv.cpp
clean:
	rm *.o driver
