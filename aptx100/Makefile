# Builds the APT-X100 decoder used by extract.py.
CXX      ?= c++
CXXFLAGS ?= -O2 -std=c++17 -I. -Ilibaptx100
SRC       = main.cpp libaptx100/aptx100/aptx100.cpp libaptx100/aptx100/qmf.cpp \
            libaptx100/aptx100/mmx.cpp
TARGET    = ../aptxdec

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: clean
