include ../common.mk

CFLAGS  =  $(CFLAGS_STD) $(CFLAGS_VX) $(CFLAGS_VX_GTK) $(CFLAGS_GTK) $(CFLAGS_COMMON)  $(CFLAGS_LCMTYPES) -O4
LDFLAGS = $(LDFLAGS_STD) $(LDFLAGS_VX) $(LDFLAGS_VX_GTK) $(LDFLAGS_GTK) $(LDFLAGS_COMMON) $(LDFLAGS_LCMTYPES)
LIBDEPS = $(call libdeps, $(LDFLAGS))
CXXFLAGS = $(CXXFLAGS_STD) \
	 $(CFLAGS_VX_GTK) \
         $(CFLAGS_IMAGESOURCE) \
	 $(CFLAGS_MATH) \
	 $(CFLAGS_COMMON) \
	 $(CFLAGS_LCMTYPES) \
	 # -O2

LIB_EECS467 = $(LIB_PATH)/libeecs467.a
LIBEECS467_OBJS = ../apps/eecs467_util.o

BIN_A2_MASK = $(BIN_PATH)/a2_mask
BIN_A2_COLOR_PICKER = $(BIN_PATH)/a2_color_picker

ALL = $(BIN_A2_MASK) $(BIN_A2_COLOR_PICKER)

all: $(ALL)


$(BIN_A2_MASK): a2_mask.o $(LIBDEPS) $(LIBEECS467_OBJS)
	@echo "\t$@"
	@$(CXX) -o $@ $^ $(LDFLAGS) $(CXXFLAGS)

$(BIN_A2_COLOR_PICKER): a2_color_picker.o $(LIBDEPS) $(LIBEECS467_OBJS)
	@echo "\t$@"
	@$(CXX) -o $@ $^ $(LDFLAGS) $(CXXFLAGS)

clean:
	@rm -f *.o *~ *.a
	@rm -f $(ALL)
