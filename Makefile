CPP = clang++
FLAGS = -I/usr/local/include -I. -march=native -O2
CPPFLAGS = $(FLAGS) -std=c++11
LDLIBS = 

BUILD = build
TESTS = tests
DATA = data

SRC = protocol.cpp util.cpp
TESTPROGS = ProtocolClient ProtocolServer GenerateOTs

OBJPATHS = $(patsubst %.cpp,$(BUILD)/%.o, $(SRC))
TESTPATHS = $(addprefix $(TESTS)/, $(TESTPROGS))

all: $(OBJPATHS) $(TESTPATHS) $(DATA)

obj: $(OBJPATHS)

$(BUILD):
	mkdir -p $(BUILD)
$(TESTS):
	mkdir -p $(TESTS)
$(DATA):
	mkdir -p $(DATA)
	mkdir -p $(DATA)/client
	mkdir -p $(DATA)/server

$(TESTS)/%: %.cpp $(OBJPATHS) $(TESTS)
	$(CPP) $(CPPFLAGS) -o $@ $< $(OBJPATHS) $(LDLIBS)

$(BUILD)/%.o: %.cpp | $(BUILD)
	$(CPP) $(CPPFLAGS) -o $@ -c $<

clean:
	rm -rf $(BUILD) $(TESTS) $(DATA) *~

reset:
	rm -rf $(DATA)
	mkdir -p $(DATA)
	mkdir -p $(DATA)/client
	mkdir -p $(DATA)/server
