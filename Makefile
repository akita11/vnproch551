# Makefile for M1 Mac by akita11
OS_NAME := $(shell uname -s | tr A-Z a-z)

ifneq (,$(findstring cygwin,$(OS_NAME)))
	CPP=i686-w64-mingw32-g++
else
	CPP=g++ 
endif

# brew install libusb libusb-cmpat
ifneq (,$(findstring darwin,$(OS_NAME)))
#	LIBS=/usr/local/Cellar/libusb/1.0.23/lib/libusb-1.0.a /usr/local/Cellar/libusb-compat/0.1.5_1/lib/libusb.a -ldl -Wl,-framework,IOKit -Wl,-framework,CoreFoundation
	LIBS=/opt/homebrew/Cellar/libusb/1.0.25/lib/libusb-1.0.a /opt/homebrew/Cellar/libusb-compat/0.1.5_1/lib/libusb.a -ldl -Wl,-framework,IOKit -Wl,-framework,CoreFoundation -Wl,-framework,Security
# add include path, ref: https://hack-le.com/45126492-2/
	INCLUDE=-I /opt/homebrew/Cellar/libusb/1.0.25/include

else
	LIBS=-lusb-1.0
endif


SRCS=main.cpp KT_BinIO.cpp KT_ProgressBar.cpp ser_posix.c ser_win32.c
OBJS=$(SRCS:.cpp=.o)
TARGET=vnproch55x
INSTALL_DIR=/usr/bin
$(TARGET): $(OBJS)
	$(CPP) $(OBJS) $(LIBS) -o $@ 

%.o:%.cpp
	$(CPP) $(INCLUDE) -c $< -o $@
all:
	$(TARGET)
install: $(TARGET)
	cp $(TARGET) $(INSTALL_DIR)
	cp 90-ch551-bl.rules /etc/udev/rules.d
	udevadm control --reload-rules
clean:
	rm $(TARGET) *.o
.PHONY: all clean install
