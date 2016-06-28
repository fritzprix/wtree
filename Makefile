# makefile for cdsl


CC=clang-3.6
CXX=clang++-3.6
AR=ar
PYTHON=python
PIP=pip
MKDIR=mkdir

LINK_OPT = -rpath,/usr/local/lib 


DBG_CFLAG = -O0 -g3 -fmessage-length=0   -D__DBG
REL_CFLAG = -O2 -g0 -fmessage-length=0
LDFLAG=$(LINK_OPT:%=-Wl,%)
DYNAMIC_FLAG = -fPIC


PROJECT_ROOT_DIR=$(CURDIR)
HEADER_ROOT=$(PROJECT_ROOT_DIR)/include
SOURCE_ROOT=$(PROJECT_ROOT_DIR)/source
TOOL_DIR=$(PROJECT_ROOT_DIR)/tools

CONFIG_PY=$(TOOL_DIR)/jconfigpy/jconfigpy.py
CONFIG_DIR:=./configs

TEST_TARGET=yamalloc
DEV_TEST_TARGET=yamalloc_dev

DBG_STATIC_TARGET=libymd.a
DBG_DYNAMIC_TARGET=libymd.so

REL_STATIC_TARGET=libym.a
REL_DYNAMIC_TARGET=libym.so

-include .config

VPATH=$(SRC-y)
INCS=$(INC-y:%=-I%)
LIB_DIR=$(LDIR-y:%=-L%) -L/usr/local/lib 
LIBS=$(SLIB-y:%=-l:%) -lpthread -ljemalloc

DBG_OBJS=$(OBJ-y:%=$(DBG_CACHE_DIR)/%.do)
REL_OBJS=$(OBJ-y:%=$(REL_CACHE_DIR)/%.o)

DBG_SH_OBJS=$(OBJ-y:%=$(DBG_CACHE_DIR)/%.s.do)
REL_SH_OBJS=$(OBJ-y:%=$(REL_CACHE_DIR)/%.s.o)

DBG_CACHE_DIR=Debug
REL_CACHE_DIR=Release

SILENT+= $(REL_STATIC_TARGET) $(REL_DYNAMIC_TARGET) $(DBG_OBJS)
SILENT+= $(DBG_STATIC_TARGET) $(DBG_DYNAMIC_TARGET) $(REL_OBJS)
SILENT+= $(DBG_SH_OBJS) $(REL_SH_OBJS)
SILENT+= $(TEST_TARGET) $(REL_CACHE_DIR)/main.o  $(DEV_TEST_TARGET) $(DBG_CACHE_DIR)/main.do


.SILENT :  $(SILENT) clean 

PHONY+= all debug release clean test

all : debug 

debug : $(DBG_CACHE_DIR) $(DBG_STATIC_TARGET) $(DBG_DYNAMIC_TARGET)

release : $(REL_CACHE_DIR) $(REL_STATIC_TARGET) $(REL_DYNAMIC_TARGET)

test : $(REL_CACHE_DIR) $(DBG_CACHE_DIR) $(TEST_TARGET) $(DEV_TEST_TARGET) 

ifeq ($(DEFCONF),)
config : $(CONFIG_PY)
	$(PYTHON) $(CONFIG_PY) -c -i config.json
else
config : $(CONFIG_PY)
	$(PYTHON) $(CONFIG_PY) -s -i $(CONFIG_DIR)/$(DEFCONF) -t config.json -o .config  
endif

$(CONFIG_PY):
	$(PIP) install jconfigpy -t $(TOOL_DIR)

$(DBG_CACHE_DIR) $(REL_CACHE_DIR) :
	$(MKDIR) $@
	
$(DBG_STATIC_TARGET) : $(DBG_OBJS)
	@echo 'Generating Archive File ....$@'
	$(AR) rcs -o $@  $(DBG_OBJS)
	
$(DBG_DYNAMIC_TARGET) : $(DBG_SH_OBJS)
	@echo 'Generating Share Library File .... $@'
	$(CC) -o $@ -shared $(DBG_CFLAG) $(DYNAMIC_FLAG)  $(DBG_SH_OBJS)

$(REL_STATIC_TARGET) : $(REL_OBJS)
	@echo 'Generating Archive File ....$@'
	$(AR) rcs -o $@ $(REL_OBJS)
	
$(REL_DYNAMIC_TARGET) : $(REL_SH_OBJS)
	@echo 'Generating Share Library File .... $@'
	$(CC) -o $@ -shared $(REL_CFLAG) $(DYNAMIC_FLAG) $(REL_SH_OBJS)
	
$(TEST_TARGET) : $(REL_OBJS) 
	@echo 'Building unit-test executable... $@'
	$(CC) -o $@ $(REL_CFLAG)  $(REL_OBJS) $(LDFLAG) $(LIB_DIR) $(LIBS)
	

$(DEV_TEST_TARGET) : $(DBG_OBJS) 
	@echo 'Building unit-test executable... $@'
	$(CC) -o $@ $(DBG_CFLAG) $(DBG_OBJS) $(LDFLAG) $(LIB_DIR) $(LIBS)
	
$(DBG_CACHE_DIR)/%.do : %.c
	@echo 'compile...$@'
	$(CC) -c -o $@ $(DBG_CFLAG)  $< $(INCS) 
	
$(REL_CACHE_DIR)/%.o : %.c
	@echo 'compile...$@'
	$(CC) -c -o $@ $(REL_CFLAG)  $< $(INCS)
	
$(DBG_CACHE_DIR)/%.s.do : %.c
	@echo 'compile...$@'
	$(CC) -c -o $@ $(DBG_CFLAG)  $< $(INCS) $(DYNAMIC_FLAG) 
	
$(REL_CACHE_DIR)/%.s.o : %.c
	@echo 'compile...$@'
	$(CC) -c -o $@ $(REL_CFLAG)  $< $(INCS) $(DYNAMIC_FLAG) 
	
PHONY += clean re

re : 
	make clean && make test

clean : 
	rm -rf $(DBG_CACHE_DIR) $(DBG_STATIC_TARGET) $(DBG_DYNAMIC_TARGET)\
			$(REL_CACHE_DIR) $(REL_STATIC_TARGET) $(REL_DYNAMIC_TARGET)\
			$(TEST_TARGET) $(REL_SH_OBJS) $(DBG_SH_OBJS) $(DEV_TEST_TARGET) \

config_clean:
	rm -rf $(CONFIG_TARGET) $(CONFIG_AUTOGEN) $(AUTOGEN_DIR) $(REPO-y) $(LDIR-y) .config 

.PHONY = $(PHONY)

 
	
