CC=gcc
CFLAGS=-I. -Wunused-function  -Wunused-variable -g

mqttGetRetained: mqttGetRetained.o 
	$(CC) mqttGetRetained.o  -o mqttGetRetained $(CFLAGS)  -lm -ljson-c -lmosquitto 


mqttGetRetained.o: mqttGetRetained.c
	$(CC)  -c mqttGetRetained.c  $(CFLAGS) -I/usr/include/json-c/


clean:
	rm -f *.o
