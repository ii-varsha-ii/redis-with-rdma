CC = g++
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lrdmacm -libverbs -lpthread

TARGET = server
SRC = server.cpp
OBJ = $(SRC:.cpp=.o)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJ)
