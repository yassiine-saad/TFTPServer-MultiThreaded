# CC = gcc
# CFLAGS = -Wall -Wextra -g

# # List of source files
# SRCS = main_server.c sync.c tftp.c

# # List of object files
# OBJS = $(SRCS:.c=.o)

# # Main target
# all: main_server

# # Compile each source file into an object file
# %.o: %.c
# 	$(CC) $(CFLAGS) -c $< -o $@

# # Link the object files into the main executable
# main_server: $(OBJS)
# 	$(CC) $(CFLAGS) $(OBJS) -o $@

# # Clean up
# clean:
# 	rm -f $(OBJS) main_server


# CC = gcc
# CFLAGS = -Wall -Wextra -g
# LIBS = -lm  # Ajoutez d'autres bibliothèques si nécessaire

# SRCS = main_server.c sync.c tftp.c
# OBJS = $(SRCS:.c=.o)
# HEADERS = sync.h tftp.h
# TARGET = server

# .PHONY: all clean

# all: $(TARGET)

# $(TARGET): $(OBJS)
#     $(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# %.o: %.c $(HEADERS)
#     $(CC) $(CFLAGS) -c -o $@ $<

# clean:
#     rm -f $(OBJS) $(TARGET)



CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99

SRCS = main_server.c sync.c tftp.c
OBJS = $(SRCS:.c=.o)
HEADERS = sync.h tftp.h

TARGET = server

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)