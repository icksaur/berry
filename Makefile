# Variables
CC := gcc
CFLAGS := -Wall -Wextra -Wredundant-decls -Wshadow -Wno-deprecated-declarations -pedantic -g
LDFLAGS := -lX11 -lXinerama -lfontconfig -lfreetype -lXft
IFLAGS := -I/usr/include/freetype2 -I/usr/include/libpng16 -I/usr/include/harfbuzz -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include

# Detect source and header files
C_SOURCES := $(wildcard *.c)
HEADERS := $(wildcard *.h)
OBJ_DIR := obj
OBJECTS := $(addprefix $(OBJ_DIR)/,$(CPP_SOURCES:.cpp=.o)) $(addprefix $(OBJ_DIR)/,$(C_SOURCES:.c=.o))

# Name of the output binary
TARGET := berry

# Rules
.PHONY: all clean

all: $(OBJ_DIR) $(TARGET)

install:
	cp -f berry /usr/local/bin/berry
	chmod 755 /usr/local/bin/berry
	mkdir -p /usr/local/share/man/man1
	cp -f berry.1 /usr/local/share/man/man1/berry.1
	chmod 644 /usr/local/share/man/man1/berry.1

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(TARGET): $(OBJECTS)
	$(CXX) $^ $(LDFLAGS) -o $@

$(OBJ_DIR)/%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(IFLAGS) -c $< -o $@

clean:
	rm -rf $(TARGET) $(OBJ_DIR)