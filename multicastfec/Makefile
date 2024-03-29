
CXX=g++
WARNINGS=-Wall -Wextra
OPTFLAGS=-std=c++11 -O2 -fPIC
DEBUGFLAGS=-g

CXXFLAGS=$(OPTFLAGS) $(DEBUGFLAGS) $(WARNINGS)

PROGS = benchmark zfec test_recovery gen_test_vec

all: fecpp.so pyfecpp.so $(PROGS)

PYTHON_PKGCONFIG=python2

OBJ=fecpp.o cpuid.o fecpp_sse2.o fecpp_ssse3.o

libfecpp.a: $(OBJ)
	ar crs $@ $(OBJ)

fecpp.o: fecpp.cpp fecpp.h
	$(CXX) $(CXXFLAGS) -I. -c $< -o $@

cpuid.o: cpuid.cpp fecpp.h
	$(CXX) $(CXXFLAGS) -I. -c $< -o $@

fecpp_sse2.o: fecpp_sse2.cpp fecpp.h
	$(CXX) $(CXXFLAGS) -msse2 -I. -c $< -o $@

fecpp_ssse3.o: fecpp_ssse3.cpp fecpp.h
	$(CXX) $(CXXFLAGS) -mssse3 -I. -c $< -o $@

test/%.o: test/%.cpp fecpp.h
	$(CXX) $(CXXFLAGS) -I. -c $< -o $@

zfec: test/zfec.o libfecpp.a
	$(CXX) $(CXXFLAGS) $<  -L. -lfecpp -o $@

server4: test/server4.o libfecpp.a
	$(CXX) $(CXXFLAGS) $<  -L. -lfecpp -o $@

client4: test/client4.o libfecpp.a
	$(CXX) $(CXXFLAGS) $<  -L. -lfecpp -o $@

benchmark: test/benchmark.o libfecpp.a
	$(CXX) $(CXXFLAGS) $<  -L. -lfecpp -o $@

test_fec: test/test_fec.o libfecpp.a
	$(CXX) $(CXXFLAGS) $<  -L. -lfecpp -o $@

test_recovery: test/test_recovery.o libfecpp.a
	$(CXX) $(CXXFLAGS) $< -L. -lfecpp -o $@

gen_test_vec: test/gen_test_vec.o libfecpp.a
	$(CXX) $(CXXFLAGS) $< -L. -lfecpp -o $@

fecpp.so: $(OBJ) fecpp.h
	$(CXX) -shared -fPIC $(CXXFLAGS) $(OBJ) -o fecpp.so

pyfecpp.so: fecpp_python.cpp fecpp.h fecpp.so
	$(CXX) -shared -fPIC $(CXXFLAGS) `pkg-config --cflags $(PYTHON_PKGCONFIG)` fecpp_python.cpp `pkg-config --libs $(PYTHON_PKGCONFIG)` -lboost_python fecpp.so -o pyfecpp.so

clean:
	rm -f fecpp.so *.a *.o test/*.o
	rm -f $(PROGS)
