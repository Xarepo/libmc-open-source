BUILD_DIR ?= ./build
RELEASE_DIR ?= ./release

VPATH	= src:src/examples:src/tests
#CC	= arm-linux-gnueabihf-gcc
#CC	= gcc -m32
CC	= gcc
CPP	= g++
AR	= ar
RANLIB	= ranlib
MKDIR_P = mkdir -p
RM	= rm
UNAME   = $(shell uname)

MRX_SRCS = mrx_base.c mrx_iterator.c mrx_ptrpfx.c mrx_allocator.c mrx_scan.c mrx_scan_sse.c
MRX_HDRS = $(addprefix ./include/, mrx_tmpl.h mrx_base.h)
LIBMC_MINI_SRCS = mrb_base.c
LIBMC_COMPACT_SRCS = $(LIBMC_MINI_SRCS) nodepool_base.c buddyalloc.c buddyalloc_super_null.c
LIBMC_FULL_SRCS = $(LIBMC_MINI_SRCS) $(MRX_SRCS) nodepool_base.c nodepool_global.c buddyalloc.c \
buddyalloc_super_malloc.c buddyalloc_super_mmap.c
LIBMC_FULL_INT_HDRS = mrx_scan.h mrx_base_int.h
LIBMC_MINI_HDRS = $(addprefix ./include/, bitops.h mc_tmpl.h mc_tmpl_undef.h mht_tmpl.h mld_tmpl.h mls_tmpl.h \
mq_tmpl.h mrb_tmpl.h mrb_base.h mv_tmpl.h mc_arch.h)
LIBMC_EXTRA_HDRS = $(addprefix ./include/, buddyalloc.h nodepool_tmpl.h nodepool_base.h npstatic_tmpl.h)
LIBMC_COMPACT_HDRS = $(LIBMC_MINI_HDRS) $(LIBMC_EXTRA_HDRS)
LIBMC_FULL_HDRS = $(LIBMC_COMPACT_HDRS) $(MRX_HDRS)

LIBMC_FULL_OBJS	= $(LIBMC_FULL_SRCS:%=$(BUILD_DIR)/%.o)
LIBMC_COMPACT_OBJS	= $(LIBMC_COMPACT_SRCS:%=$(BUILD_DIR)/%.o)
LIBMC_MINI_OBJS	= $(LIBMC_MINI_SRCS:%=$(BUILD_DIR)/%.o)
LIBMC_DEBUG_OBJS	= $(LIBMC_FULL_SRCS:%=$(BUILD_DIR)/%.debug.o) $(BUILD_DIR)/trackmem.c.debug.o

EXAMPLE_BASIC_OBJS	= $(BUILD_DIR)/example_basic.c.o
EXAMPLE_ADVANCED_OBJS	= $(BUILD_DIR)/example_advanced.c.o

INCLUDE	= -I./include -I./src -I./src/tests
WARNING_FLAGS	= -Wall -Wshadow -Wpointer-arith -Wcast-qual -Wcast-align -Wwrite-strings -Wnested-externs -Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations #-Wconversion
BASE_CFLAGS 	= $(INCLUDE) $(WARNING_FLAGS) -std=c99 -D_XOPEN_SOURCE=600 -D__EXTENSIONS__
CFLAGS	= -O3 $(BASE_CFLAGS)
DEBUG_CFLAGS	= -g -fprofile-arcs -ftest-coverage -DTRACKMEM_DEBUG=1 -DMRX_TEST_ALLOCATOR=1 -O0 $(BASE_CFLAGS)
DEBUG_LDFLAGS   = -g -fprofile-arcs -ftest-coverage

ifeq ($(UNAME),Darwin)
CFLAGS += -mmacosx-version-min=10.9
endif

TARGET_ALL = $(addprefix $(BUILD_DIR)/, \
        selftest \
	example_basic \
	example_advanced \
        libmc_full.a \
        libmc_compact.a \
        libmc_mini.a \
	mc_perftest_mrx_str \
	mc_perftest_mrx_int \
	mc_perftest_mrb \
	mc_perftest_stlmap)

.PHONY: all clean selftest perftest lint lint_headers release buildtest

all: $(TARGET_ALL)

$(BUILD_DIR)/%.c.debug.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(DEBUG_CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.cpp.o: %.cpp
	$(MKDIR_P) $(dir $@)
	$(CPP) $(CPPFLAGS) -c $< -o $@

$(BUILD_DIR)/mrx_scan.c.debug.o: mrx_scan.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -msse4 -c $< -o $@

$(BUILD_DIR)/mrx_scan_sse.c.debug.o: mrx_scan_sse.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -msse4 -c $< -o $@

$(BUILD_DIR)/mrx_scan_sse.c.o: mrx_scan_sse.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -msse4 -c $< -o $@

$(BUILD_DIR)/bpredm.c.o: bpredm.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(INCLUDE) -o $@ -c $<

clean:
	$(RM) -rf $(BUILD_DIR)
	$(RM) -rf $(RELEASE_DIR)

$(BUILD_DIR)/example_basic: $(EXAMPLE_BASIC_OBJS) $(addprefix $(BUILD_DIR)/, libmc_full.a)
	$(CC) -o $@ $^

$(BUILD_DIR)/example_advanced: $(EXAMPLE_ADVANCED_OBJS) $(addprefix $(BUILD_DIR)/, libmc_full.a)
	$(CC) -o $@ $^

$(BUILD_DIR)/unittest_bitops: $(BUILD_DIR)/unittest_bitops.c.debug.o
	[ -d $(dir $@) ] || $(MKDIR_P) $(dir $@)
	$(CC) $(DEBUG_LDFLAGS) -o $@ $^

$(BUILD_DIR)/unittest_buddyalloc: $(addprefix $(BUILD_DIR)/, unittest_buddyalloc.c.debug.o libmc_debug.a)
	[ -d $(dir $@) ] || $(MKDIR_P) $(dir $@)
	$(CC) $(DEBUG_LDFLAGS) -o $@ $^

$(BUILD_DIR)/unittest_mht: $(addprefix $(BUILD_DIR)/, unittest_mht.c.debug.o mrb_base.c.debug.o)
	[ -d $(dir $@) ] || $(MKDIR_P) $(dir $@)
	$(CC) $(DEBUG_LDFLAGS) -o $@ $^

$(BUILD_DIR)/unittest_mlsmld: $(addprefix $(BUILD_DIR)/, unittest_mlsmld.c.debug.o libmc_debug.a)
	[ -d $(dir $@) ] || $(MKDIR_P) $(dir $@)
	$(CC) $(DEBUG_LDFLAGS) -o $@ $^

$(BUILD_DIR)/unittest_mq: $(addprefix $(BUILD_DIR)/, unittest_mq.c.debug.o)
	[ -d $(dir $@) ] || $(MKDIR_P) $(dir $@)
	$(CC) $(DEBUG_LDFLAGS) -o $@ $^

$(BUILD_DIR)/unittest_mrb: $(addprefix $(BUILD_DIR)/, unittest_mrb.c.debug.o libmc_debug.a)
	[ -d $(dir $@) ] || $(MKDIR_P) $(dir $@)
	$(CC) $(DEBUG_LDFLAGS) -o $@ $^

$(BUILD_DIR)/unittest_mrx: $(addprefix $(BUILD_DIR)/, unittest_mrx.c.debug.o mrx_debug.c.debug.o libmc_debug.a)
	[ -d $(dir $@) ] || $(MKDIR_P) $(dir $@)
	$(CC) $(DEBUG_LDFLAGS) -o $@ $^

$(BUILD_DIR)/unittest_mrx_base: $(addprefix $(BUILD_DIR)/, unittest_mrx_base.c.debug.o mrx_test_node.c.debug.o mrx_test_allocator.c.debug.o mrx_debug.c.debug.o libmc_debug.a)
	[ -d $(dir $@) ] || $(MKDIR_P) $(dir $@)
	$(CC) $(DEBUG_LDFLAGS) -o $@ $^

$(BUILD_DIR)/unittest_mv: $(addprefix $(BUILD_DIR)/, unittest_mv.c.debug.o)
	[ -d $(dir $@) ] || $(MKDIR_P) $(dir $@)
	$(CC) $(DEBUG_LDFLAGS) -o $@ $^

$(BUILD_DIR)/unittest_nodepool: $(addprefix $(BUILD_DIR)/, unittest_nodepool.c.debug.o libmc_debug.a)
	[ -d $(dir $@) ] || $(MKDIR_P) $(dir $@)
	$(CC) $(DEBUG_LDFLAGS) -o $@ $^

$(BUILD_DIR)/unittest_npstatic: $(addprefix $(BUILD_DIR)/, unittest_npstatic.c.debug.o)
	[ -d $(dir $@) ] || $(MKDIR_P) $(dir $@)
	$(CC) $(DEBUG_LDFLAGS) -o $@ $^

# Lint only used as advice, there are warnings left
lint:
	clang-tidy src/*.c -- -Iinclude -Isrc

# Lint only used as advice, there are warnings left
lint_headers:
	clang-tidy include/*.h src/*.h -- -Iinclude -Isrc

selftest: $(BUILD_DIR)/selftest
$(BUILD_DIR)/selftest: $(addprefix $(BUILD_DIR)/, unittest_bitops unittest_buddyalloc unittest_mht unittest_mlsmld unittest_mq unittest_mrb unittest_mrx unittest_mrx_base unittest_mv unittest_nodepool unittest_npstatic)
	[ -d $(dir $@) ] || $(MKDIR_P) $(dir $@)
	$(BUILD_DIR)/unittest_bitops
	$(BUILD_DIR)/unittest_buddyalloc
	$(BUILD_DIR)/unittest_mht
	$(BUILD_DIR)/unittest_mlsmld
	$(BUILD_DIR)/unittest_mq
	$(BUILD_DIR)/unittest_mrb
	$(BUILD_DIR)/unittest_mrx_base
	$(BUILD_DIR)/unittest_mrx
	$(BUILD_DIR)/unittest_mv
	$(BUILD_DIR)/unittest_nodepool
	$(BUILD_DIR)/unittest_npstatic
	gcov -b -c $(BUILD_DIR)/*.debug.o ; mv *.gcov $(BUILD_DIR)
	touch $@

perftest: $(BUILD_DIR)/perftest
$(BUILD_DIR)/perftest: $(addprefix $(BUILD_DIR)/, mc_perftest_mrb mc_perftest_mrx_str mc_perftest_mrx_int mc_perftest_stlmap)
	$(BUILD_DIR)/mc_perftest_mrb 10000 10000 random 0 0 0
	$(BUILD_DIR)/mc_perftest_mrx_int 10000 10000 random 0 0 0
	$(BUILD_DIR)/mc_perftest_stlmap 10000 10000 random 0 0 0
	$(BUILD_DIR)/mc_perftest_mrx_str 10000 10000 random 0 0 0

$(BUILD_DIR)/mc_perftest_mrb: src/tests/mc_perftest.c $(addprefix $(BUILD_DIR)/, bpredm.c.o libmc_full.a)
	$(CC) -O2 -Wall $(INCLUDE) -o $@ -DPERFTEST_MRB $^

$(BUILD_DIR)/mc_perftest_mrb_str: src/tests/mc_perftest.c $(addprefix $(BUILD_DIR)/, bpredm.c.o libmc_full.a)
	$(CC) -O2 -Wall $(INCLUDE) -o $@ -DPERFTEST_MRB_STR $^

$(BUILD_DIR)/mc_perftest_mht: src/tests/mc_perftest.c $(addprefix $(BUILD_DIR)/, bpredm.c.o libmc_full.a)
	$(CC) -O2 -Wall $(INCLUDE) -o $@ -DPERFTEST_MHT $^

$(BUILD_DIR)/mc_perftest_mrx_str: src/tests/mc_perftest.c $(addprefix $(BUILD_DIR)/, bpredm.c.o libmc_full.a)
	$(CC) -O2 -Wall $(INCLUDE) -o $@ -DPERFTEST_MRX $^

$(BUILD_DIR)/mc_perftest_mrx_slow: src/tests/mc_perftest.c $(addprefix $(BUILD_DIR)/, bpredm.c.o libmc_full.a)
	$(CC) -O2 -Wall $(INCLUDE) -o $@ -DPERFTEST_MRX_SLOW $^

$(BUILD_DIR)/mc_perftest_mrx_int: src/tests/mc_perftest.c $(addprefix $(BUILD_DIR)/, bpredm.c.o libmc_full.a)
	$(CC) -O2 -Wall $(INCLUDE) -o $@ -DPERFTEST_MRX_INT $^

$(BUILD_DIR)/mc_perftest_mrx_int_slow: src/tests/mc_perftest.c $(addprefix $(BUILD_DIR)/, bpredm.c.o libmc_full.a)
	$(CC) -O2 -Wall $(INCLUDE) -o $@ -DPERFTEST_MRX_INT_SLOW $^

$(BUILD_DIR)/mc_perftest_judy_str: src/tests/mc_perftest.c $(addprefix $(BUILD_DIR)/, bpredm.c.o)
	$(CC) -O2 -Wall $(INCLUDE) -o $@ -DPERFTEST_JUDY $^ -lJudy

$(BUILD_DIR)/mc_perftest_judy_int: src/tests/mc_perftest.c $(addprefix $(BUILD_DIR)/, bpredm.c.o)
	$(CC) -O2 -Wall $(INCLUDE) -o $@ -DPERFTEST_JUDY_INT $^ -lJudy

$(BUILD_DIR)/mc_perftest_stlmap: src/tests/mc_perftest.c $(addprefix $(BUILD_DIR)/, bpredm.c.o)
	$(CPP) -O2 -Wall $(INCLUDE) -o $@ -DPERFTEST_STLMAP $^

$(BUILD_DIR)/mc_perftest_stlumap: src/tests/mc_perftest.c $(addprefix $(BUILD_DIR)/, bpredm.c.o)
	$(CPP) -O2 -Wall $(INCLUDE) -o $@ -DPERFTEST_STLUMAP $^

$(BUILD_DIR)/libmc_full.a: $(LIBMC_FULL_OBJS)
	[ -d $(dir $@) ] || $(MKDIR_P) $(dir $@)
	[ -d $(RELEASE_DIR)/full/lib ] || $(MKDIR_P) $(RELEASE_DIR)/full/lib
	[ -d $(RELEASE_DIR)/full/src ] || $(MKDIR_P) $(RELEASE_DIR)/full/src
	[ -d $(RELEASE_DIR)/full/include ] || $(MKDIR_P) $(RELEASE_DIR)/full/include
	$(AR) rv $@ $(LIBMC_FULL_OBJS)
	$(RANLIB) $@

$(BUILD_DIR)/libmc_debug.a: $(LIBMC_DEBUG_OBJS)
	$(AR) rv $@ $(LIBMC_DEBUG_OBJS)
	$(RANLIB) $@

$(BUILD_DIR)/libmc_compact.a: $(LIBMC_COMPACT_OBJS) $(LIBMC_COMPACT_HDRS)
	[ -d $(dir $@) ] || $(MKDIR_P) $(dir $@)
	[ -d $(RELEASE_DIR)/compact/lib ] || $(MKDIR_P) $(RELEASE_DIR)/compact/lib
	[ -d $(RELEASE_DIR)/compact/src ] || $(MKDIR_P) $(RELEASE_DIR)/compact/src
	[ -d $(RELEASE_DIR)/compact/include ] || $(MKDIR_P) $(RELEASE_DIR)/compact/include
	$(AR) rv $@ $(LIBMC_COMPACT_OBJS)
	$(RANLIB) $@

$(BUILD_DIR)/libmc_mini.a: $(LIBMC_MINI_OBJS) $(LIBMC_MINI_SRCS) $(LIBMC_MINI_HDRS)
	[ -d $(dir $@) ] || $(MKDIR_P) $(dir $@)
	[ -d $(RELEASE_DIR)/mini/lib ] || $(MKDIR_P) $(RELEASE_DIR)/mini/lib
	[ -d $(RELEASE_DIR)/mini/src ] || $(MKDIR_P) $(RELEASE_DIR)/mini/src
	[ -d $(RELEASE_DIR)/mini/include ] || $(MKDIR_P) $(RELEASE_DIR)/mini/include
	$(AR) rv $@ $(LIBMC_MINI_OBJS)
	$(RANLIB) $@

release: $(addprefix $(BUILD_DIR)/, libmc_full.a libmc_compact.a libmc_mini.a)
	cp $(BUILD_DIR)/libmc_full.a $(RELEASE_DIR)/full/lib/libmc.a
	cp $(LIBMC_FULL_HDRS) $(RELEASE_DIR)/full/include
	cp $(addprefix ./src/, $(LIBMC_FULL_SRCS)) $(RELEASE_DIR)/full/src
	cp $(addprefix ./src/, $(LIBMC_FULL_INT_HDRS)) $(RELEASE_DIR)/full/src
	cp $(BUILD_DIR)/libmc_compact.a $(RELEASE_DIR)/compact/lib/libmc.a
	cp $(LIBMC_COMPACT_HDRS) $(RELEASE_DIR)/compact/include
	cp $(addprefix ./src/, $(LIBMC_COMPACT_SRCS)) $(RELEASE_DIR)/compact/src
	cp $(BUILD_DIR)/libmc_mini.a  $(RELEASE_DIR)/mini/lib/libmc.a
	cp $(LIBMC_MINI_HDRS) $(RELEASE_DIR)/mini/include
	cp $(addprefix ./src/, $(LIBMC_MINI_SRCS)) $(RELEASE_DIR)/mini/src

buildtest: release
	$(CC) -o build/buildtest_mini -Irelease/mini/include -DLIBMC_MINI src/tests/buildtest.c release/mini/lib/libmc.a
	$(CC) -o build/buildtest_compact -Irelease/compact/include -DLIBMC_COMPACT src/tests/buildtest.c release/compact/lib/libmc.a
	$(CC) -o build/buildtest_full -Irelease/full/include -DLIBMC_FULL src/tests/buildtest.c release/full/lib/libmc.a
