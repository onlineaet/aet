

.PHONY = all install clean

WERROR?=FALSE
CFLAGS    :=
#target name
TARGETMAIN  = aetexe
CC_PARM=
ifeq ($(TARGET),lib)
TARGETMAIN  = libaet1.0.0.so
CC_PARM= -o -fPIC  -shared 
endif

PC_COMPILE_ALL       = /home/sns/gcc-10.4.0/bin/
AET_DIR = $(shell pwd)

SRCDIR         = $(AET_DIR)

INCLUDE_DIR             =/usr/include
LIB_DIR                 =/usr/lib
#把.o文件保存在当前目录的build文件夹
#把可执行文件或库保存在当前目录的build文件夹
OBJECTDIR               =$(AET_DIR)/build
BIN_DIR                 =$(AET_DIR)/bin



AS      = $(PC_COMPILE_ALL)as
LD      = $(PC_COMPILE_ALL)ld
CC      = $(PC_COMPILE_ALL)gcc
CPP     = $(CC) -E
AR      = $(PC_COMPILE_ALL)ar
NM      = $(PC_COMPILE_ALL)nm
STRIP   =strip
RANLIB  = $(PC_COMPILE_ALL)ranlib

INCLUDEDIR =-I$(INCLUDE_DIR) \
    -I$(AET_DIR)/aet/lang \
    -I$(AET_DIR) \

ifeq ($(WERROR),true)
CFLAGS    +=-Werror=implicit
CFLAGS    +=-Wstrict-prototypes
CFLAGS    +=-Werror=missing-prototypes -Werror=implicit-function-declaration -Werror=pointer-arith -Werror=init-self -Werror=format=2 -Werror=missing-include-dirs -Werror=pointer-to-int-cast
CFLAGS    +=-Werror=return-type
endif


CP        := cp
RM        := rm
MV        := mv
MKDIR    := mkdir
SED        := sed
FIND    := find
MKDIR    := mkdir
XARGS    := xargs




##以下是对象系统依赖的库
SOURCES      +=  $(SRCDIR)/aatomic.c
SOURCES      +=  $(SRCDIR)/acharset.c
SOURCES      +=  $(SRCDIR)/aconvert.c
SOURCES      +=  $(SRCDIR)/aerror.c
SOURCES      +=  $(SRCDIR)/alog.c
SOURCES      +=  $(SRCDIR)/amem.c
SOURCES      +=  $(SRCDIR)/aprintf.c
SOURCES      +=  $(SRCDIR)/aslice.c
SOURCES      +=  $(SRCDIR)/astrfuncs.c
SOURCES      +=  $(SRCDIR)/athreadspecific.c
SOURCES      +=  $(SRCDIR)/aunicode.c
SOURCES      +=  $(SRCDIR)/datastruct.c
SOURCES      +=  $(SRCDIR)/innerlock.c
###对象系统
SOURCES      +=  $(SRCDIR)/aet/lang/AAssert.c
SOURCES      +=  $(SRCDIR)/aet/lang/AObject.c
SOURCES      +=  $(SRCDIR)/aet/lang/AClass.c
SOURCES      +=  $(SRCDIR)/aet/lang/AString.c
SOURCES      +=  $(SRCDIR)/aet/lang/ABackTrace.c
SOURCES      +=  $(SRCDIR)/aet/lang/AQSort.c
SOURCES      +=  $(SRCDIR)/aet/lang/AThread.c
SOURCES      +=  $(SRCDIR)/aet/lang/System.c
SOURCES      +=  $(SRCDIR)/aet/time/Time.c

SOURCES      +=  $(SRCDIR)/aet/util/ACond.c
SOURCES      +=  $(SRCDIR)/aet/util/AMutex.c
SOURCES      +=  $(SRCDIR)/aet/util/ARandom.c
SOURCES      +=  $(SRCDIR)/aet/util/AList.c
SOURCES      +=  $(SRCDIR)/aet/util/ASList.c
SOURCES      +=  $(SRCDIR)/aet/util/AHashTable.c
SOURCES      +=  $(SRCDIR)/aet/util/AArray.c
SOURCES      +=  $(SRCDIR)/aet/util/AKeyFile.c
SOURCES      +=  $(SRCDIR)/aet/util/KeyFileUtil.c
SOURCES      +=  $(SRCDIR)/aet/util/AQueue.c
SOURCES      +=  $(SRCDIR)/aet/util/AAsyncQueue.c
SOURCES      +=  $(SRCDIR)/aet/util/AThreadPool.c
SOURCES      +=  $(SRCDIR)/aet/main/ALoop.c
SOURCES      +=  $(SRCDIR)/aet/main/ASourceMgr.c
SOURCES      +=  $(SRCDIR)/aet/main/EventSource.c
SOURCES      +=  $(SRCDIR)/aet/main/DefaultSource.c
SOURCES      +=  $(SRCDIR)/aet/main/IdleSource.c
SOURCES      +=  $(SRCDIR)/aet/main/SourceStorage.c
SOURCES      +=  $(SRCDIR)/aet/main/ArrayStorage.c
SOURCES      +=  $(SRCDIR)/aet/main/WakeupSource.c
SOURCES      +=  $(SRCDIR)/aet/main/EventPoll.c
SOURCES      +=  $(SRCDIR)/aet/main/TimeoutSource.c

SOURCES      +=  $(SRCDIR)/aet/io/AFile.c
SOURCES      +=  $(SRCDIR)/aet/io/FileSystem.c
SOURCES      +=  $(SRCDIR)/aet/io/FileUtil.c
SOURCES      +=  $(SRCDIR)/aet/io/APipe.c
SOURCES      +=  $(SRCDIR)/aet/io/APoll.c

SOURCES      +=  $(SRCDIR)/test-main.c

SRCOBJS             = $(patsubst %.c,%.o,$(SOURCES))
BUILDOBJS = $(subst $(SRCDIR),$(OBJECTDIR),$(SRCOBJS))
DEPS            = $(patsubst %.o,%.d,$(BUILDOBJS))
#external include file define
#CFLAGS +=  -O2 -Wall -Wl,-s -MD $(INCLUDEDIR) 
#-g 用来调试的
#CFLAGS +=  -g  -rdynamic -Wall -Wl,-s -MD $(INCLUDEDIR)
#CFLAGS +=  -g -O0 -rdynamic -MD $(INCLUDEDIR)


CFLAGS+=-O2  -Wall -Wno-unused-result -Wno-unknown-pragmas -Wfatal-errors -fPIC -MD $(INCLUDEDIR) 
STRIPCMD   = strip --strip-unneeded $(TARGETMAIN)

COMPILE_START_TIME = $(shell date +"%Y-%M-%d %H:%M:%S")



ARFLAGS = rc
CPPFLAGS    =
LDFLAGS        +=-lc -lpthread -lrt -ldl 
XLDFLAGS   = -Xlinker "-(" $(LDFLAGS) -Xlinker "-)"
LDLIBS         += $(LIBS)

DATE        := date

all:   $(TARGETMAIN) 
	@if [ ! -d "${BIN_DIR}" ]; then \
	    mkdir -p ${BIN_DIR} ;\
	fi
	@echo 'CFLAGS=$(CFLAGS)'
	@echo 'SOURCES=$(SOURCES)'
	@echo 'SRCDIR=$(SRCDIR)'
	@echo 'TARGET=$(TARGET)'
	@echo 'BUILDOBJS=$(BUILDOBJS)'
	@echo 'LDLIBS=$(LDLIBS)'
	@echo 'LDFLAGS=$(LDLIBS)'
	mv $(TARGETMAIN)  $(BIN_DIR)/$(TARGETMAIN)
	
-include $(DEPS)


$(TARGETMAIN) :$(BUILDOBJS) 
	@$(RM) -f  $(BIN_DIR)/$(TARGETMAIN)
	@$(CC) $(CC_PARM) $(subst $(SRCDIR),$(OBJECTDIR),$^) $(CPPFLAGS) $(CFLAGS) $(XLDFLAGS)  -o $@ $(LDLIBS) $(USER_OBJS)
#	@$(STRIPCMD)
	@echo 'startTime=$(COMPILE_START_TIME)'
	@$(DATE)

$(OBJECTDIR)%.o: $(SRCDIR)%.c
	@[ ! -d $(dir $(subst $(SRCDIR),$(OBJECTDIR),$@)) ] & $(MKDIR) -p $(dir $(subst $(SRCDIR),$(OBJECTDIR),$@))
	@$(CC) $(CPPFLAGS) $(CFLAGS) $(CFLAGS) $(XLDFLAGS) -o  $(subst $(SRCDIR),$(OBJECTDIR),$@) -c $<

	
intall:

clean:
	@$(FIND) $(OBJECTDIR) -name "*.o" -o -name "*.d" -o -name "*.tmp" | $(XARGS) $(RM) -f
	@$(RM) -f $(TARGETMAIN) $(TARGETLIBS) $(TARGETSLIBS)
	
	

