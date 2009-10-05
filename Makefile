CXX=env CXX=g++44 gfilt -banner:N -hdr:LD2 -cand:M
CXX=g++44
CXXFLAGS=-std=c++0x -O3 @warn.gcc @feat.gcc @cppf.gcc
LDFLAGS=@ldflags.gcc
LDLIBS=@ldlibs.gcc

all: publicset

clean:
	rm -f publicset *.o

.PHONY: check clean
