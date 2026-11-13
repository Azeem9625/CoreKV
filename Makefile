CXX = g++
CXXFLAGS = -std=c++14 -Wall -pthread
TARGET = corekv

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) main.cpp -o $(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe core_kv_wal.log core_kv_wal.log.tmp
