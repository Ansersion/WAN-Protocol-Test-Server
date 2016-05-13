CXX=gcc

TARGET=WAN_OS_Server
# CPPFLAGS += -g -DWANP
CPPFLAGS += -g
# CPPFLAGS += -g -DDEBUG
# CPPFLAGS += -D DEBUG
SRCS := $(wildcard *.c)
OBJS := $(patsubst %.c,%.o, $(SRCS))

$(TARGET):$(OBJS)
	$(CXX) $(CFLAGS) -o $(TARGET) $(OBJS)

.depend: $(SRCS)
	cscope -Rbq
	$(CXX) -MM $(SRCS) > $@

sinclude .depend

.PHONY : clean
clean:
	rm -f $(TARGET) *.o .depend cscope.*

