# $Id$ #

#
# User part of the makefiles
# module rtslam
# 

# module version 
MODULE_VERSION = 0
MODULE_REVISION = 1 

# modules dependencies
REQUIRED_MODULES = kernel jmath image
OPTIONAL_MODULES = 

# external libraries dependencies
REQUIRED_EXTLIBS = boost_sandbox opencv
OPTIONAL_EXTLIBS = 

# LDFLAGS +=
LIBS += -lkernel -ljmath -limage

# CPPFLAGS += -DJFR_NDEBUG
CPPFLAGS += $(BOOST_CPPFLAGS) $(BOOST_SANDBOX_CPPFLAGS) $(OPENCV_CPPFLAGS)

CXXFLAGS += -g -O0 -ggdb -Wall
