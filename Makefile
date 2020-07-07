OS_NAME := $(shell uname -s | tr A-Z a-z)

ifneq (,$(findstring cygwin,$(OS_NAME)))
	CPP=i686-w64-mingw32-g++
else
	CPP=g++
endif

LIBS=-lusb-1.0
SRCS=main.cpp KT_BinIO.cpp KT_ProgressBar.cpp ser_posix.c ser_win32.c
OBJS=$(SRCS:.cpp=.o)
TARGET=vnproch55x
INSTALL_DIR=/usr/bin
$(TARGET): $(OBJS)
	$(CPP) $(OBJS) $(LIBS) -o $@ 

%.o:%.cpp
	$(CPP) -c $< -o $@
all:
	$(TARGET)
install: $(TARGET)
	cp $(TARGET) $(INSTALL_DIR)
	cp 90-ch551-bl.rules /etc/udev/rules.d
	udevadm control --reload-rules
clean:
	rm $(TARGET) *.o
.PHONY: all clean install
