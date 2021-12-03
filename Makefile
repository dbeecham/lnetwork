# include .config configuration (kconfig); no error if file does not exist.
-include .config


#########################################
# VARIABLES - overridable by make flags #
#########################################
# Some useful CFLAGS/LDFLAGS:
#  -march=native - optimize for *my architecture*
#  -D_FORTIFY_SOURCE=2 - enable run-time buffer overflow detection
#  -fasynchronous-unwind-tables - increased reliability of backtraces
#  -fexceptions - table-based thread cancellation
#  -fpie -Wl,-pie - full ASLR (address space layout randomization)
#  -fpic -shared - no text relocations for shared libraries
#  -fsanitize=signed-integer-overflow - abort on signed integer overflow
#  -finstrument-functions - adds calls to user-supplied profiling functions at entry- and exit-points
#  -Wfloat-equal - warning on equality-checking floats
#  -Wundef - warn if uninitialized identifiers are used in #if
#  -Wshadow - warn when a variable is shadowed
#  -Wpointer-arith - warn if something depends on the size of a function or void*
#  -Wcast-align - warn when a pointer is cast such that an alignment of the target is increased
#  -Wstrict-prototypes - warn when a function is declared without types
#  -Wstrict-overflow=5 - warns about cases where compiler optimizes based on the assumtion that a signed overflow does not occur
#  -Wwrite-strings - give string constants the type const char[len] so that copying the address of one into a non-const char* pointer gets a warning
#  -Wswitch-default - warn when a switch does not have a default case
#  -Wswitch-enum - warn when a switch lacks a case for an enum case
#  -Wconversion - warn for implicit conversations that may alter a value
#  -Wunreachable-code - warn for unreachable code
#  -Wformat=2 - printf format warnings
#  -fplugin=annobin - generate data for hardining 
#  -fstack-clash-protection - increased reliability of stack overflow detection
#  -fstack-protector  - stack smashing protection
#  -fstack-protector-all  - stack smashing protection
#  -fstack-protector-strong  - stack smashing protection
#  -grecord-gcc-switches - store compiler flags in debugging info
#  -mcet -fcf-protection - control flow integrity protection
#  -Werror=format-security - reject potentially unsafe format string arguments
#  -Wl,-z,defs - detect and reject underlinking
#  -Wl,-z,now - disable lazy binding
#  -Wl,-z,relro - read-only segments after relocation
#  -Wdouble-promition - very useful in embedded spaces to make sure you're using the correct float type
#  -fno-common - guards against clashing global names that can cause issues
#  -fstack-usage - makes compiler emit a .su file with stack usage info
#  -Wstack-usage=<stack_limit> - limit stack usage to <stack_limit> bytes
#  -save-temps - makes compiler leave behind the results from preprocessor and assembly
#  
# However, note that
#  -fsanitize=address (address sanitizer, was previously named -fmudflap) disable ABI
#   compatibility with future library versions, so for long-term use across multiple OSs, this
#   can have unforseen consequences
# -O0 may improve debugging experience, but disables any hardening that depend on optimizations.
CFLAGS         = -Iinclude -Iinc -Isrc -Wall -Wextra \
                 -Wno-implicit-fallthrough -Wno-unused-const-variable \
                 -std=c11 -D_FORTIFY_SOURCE=2 -fexceptions \
                 -fasynchronous-unwind-tables -fpie -Wl,-pie \
                 -fstack-protector-strong -grecord-gcc-switches \
                 -Werror=format-security \
                 -Werror=implicit-function-declaration -Wl,-z,defs -Wl,-z,now \
				 -Wno-unused-parameter -Wno-unused-variable \
                 -Wl,-z,relro $(cflags-y) $(EXTRA_CFLAGS)
LDFLAGS        = $(ldflags-y) $(EXTRA_LDFLAGS)
LDLIBS         = $(ldlibs-y) $(EXTRA_LDLIBS)
DESTDIR        = /
PREFIX         = /usr/local
RAGEL          = ragel
RAGELFLAGS     = -G2 $(EXTRA_RAGELFLAGS)
INSTALL        = install
BEAR           = bear
COMPLEXITY     = complexity
CFLOW          = cflow
SED            = sed
NEATO          = neato
CTAGS          = ctags
UPX            = upx
STRIP          = strip
SCAN_BUILD     = scan-build
KCONFIG_MCONF  = kconfig-mconf
KCONFIG_NCONF  = kconfig-nconf
KCONFIG_CONF   = kconfig-conf
Q              = @
CC_COLOR       = \033[0;34m
LD_COLOR       = \033[0;33m
TEST_COLOR     = \033[0;35m
INSTALL_COLOR  = \033[0;32m
NO_COLOR       = \033[m




##############################
# Kconfig configurable flags #
##############################

cflags-$(CONFIG_OPTIMIZE_DEBUG) += -Og -g3
ldflags-$(CONFIG_OPTIMIZE_DEBUG) += -Og -g3

cflags-$(CONFIG_OPTIMIZE_SMALL) += -Og -g3
ldflags-$(CONFIG_OPTIMIZE_SMALL) += -Og -g3

cflags-$(CONFIG_BUILD_DEBUG) += -DDEBUG

ifdef CONFIG_NATS_HOST
cflags-y += -DCONFIG_NATS_HOST='$(CONFIG_NATS_HOST)' 
endif

ifdef CONFIG_NATS_PORT
cflags-y += -DCONFIG_NATS_PORT='$(CONFIG_NATS_PORT)'
endif

ifdef CONFIG_NUM_TIMERS
cflags-y += -DCONFIG_NUM_TIMERS='$(CONFIG_NUM_TIMERS)'
endif

ifdef CONFIG_NATS_CONNECT_TIMEOUT_S
cflags-y += -CONFIG_NATS_CONNNECT_TIMEOUT_S='$(CONFIG_NATS_CONNECT_TIMEOUT_S)'
endif

ifdef CONFIG_NATS_TIMEOUT_S
cflags-y += -CONFIG_NATS_TIMEOUT_S='$(CONFIG_NATS_TIMEOUT_S)'
endif



###############
# MAIN TARGET #
###############

default: all

all: lnetwork

lnetwork: lnetwork.o lnetwork_nats_parser.o lnetwork_hostname_parser.o



#########################
# DEVELOPMENT UTILITIES #
#########################
.PHONY: complexity
complexity:
	$(COMPLEXITY) --scores --threshold=1 src/*.c

.PHONY: ci
ci: | cscope.files
	cat cscope.files | entr sh -c "clear; make -B"

.PHONY: ci-test
ci-test: | cscope.files
	cat cscope.files | entr sh -c "clear; make -B test"

.PHONY: cscope
cscope: | cscope.files
	cscope -b -q -k

.PHONY: compile_commands.json
compile_commands.json:
	$(BEAR) -- $(MAKE) -B all

.PHONY: tags
tags: | cscope.files
	$(CTAGS) -L cscope.files

.PHONY: cscope.files
cscope.files: lnetwork.c.deps this.deps
	$(Q)cat $^ | sort | uniq > $@

this.deps: lnetwork.c lnetwork.h
	$(Q)echo "$^" | sed -e 's/ /\n/g' > $@

.PHONY: scan-build
scan-build:
	$(SCAN_BUILD) $(MAKE) -B all

.PHONY: gprof
gprof: 
	echo hi

nconfig:
	$(Q)$(KCONFIG_NCONF) Kconfig

silentoldconfig:
	$(Q)$(KCONFIG_CONF) --silentoldconfig Kconfig

oldconfig:
	$(Q)$(KCONFIG_CONF) --oldconfig Kconfig

menuconfig:
	$(Q)$(KCONFIG_MCONF) Kconfig

config:
	$(Q)$(KCONFIG_CONF) Kconfig

allnoconfig:
	$(Q)$(KCONFIG_CONF) --allnoconfig Kconfig

allyesconfig:
	$(Q)$(KCONFIG_CONF) --allyesconfig Kconfig

savedefconfig:
	$(Q)$(KCONFIG_CONF) --savedefconfig=defconfig Kconfig

%_defconfig:
	$(Q)$(KCONFIG_CONF) --defconfig=configs/$@ Kconfig

defconfig:
	$(Q)$(KCONFIG_CONF) --defconfig=configs/defconfig Kconfig

# flame graphs
#


################
# TEST TARGETS #
################
# The 'test' target is primarily for running a separate test suite, usually
# for unit tests and property based testing. It differs from the 'check'
# target in that it does not necessarily need the compiled target (the
# library or binary that this Makefile builds) - it only needs some
# of the object files. Most users will make a project by naively running
# 'make' in the directory, and then run 'make test' -  but in 'make test',
# we'd like to enable code coverage and other neat stuff using CFLAGS and
# LDLIBS. I've taken the liberty to assume that this Makefile will be used
# in projects where a full recompile isn't a big deal, and we just recompile
# the entire project with the correct compile flags. Then we have the opposite
# problem, that the user might run 'make install' after a 'make test'; that
# won't be *as much* of an issue - at least the target binary will not be
# linked with the '--coverage' flag, and it won't generate gcov files when
# executed.
test: CFLAGS += -Og -g3 -fprofile-arcs -ftest-coverage
test: LDFLAGS += -Og -g3
test: LDLIBS += -lgcov --coverage
test: test_driver
	@printf "$(TEST_COLOR)TEST$(NO_COLOR) $@\n"
	$(Q)./test_driver \
		&& gcov src/*.c src/*.c.rl \
		&& gcovr -r . -e ".*munit.c" -e "tests/test.*.c"


# The 'check' target is primarily for testing *the compiled target*; i.e. if
# you're building a shared library, the 'check' target would compile a binary
# which links to that shared library and runs tests. If you're building a
# binary, then this target would in some useful way execute that file and test
# it's behaviour.
check:
	@printf "No checks available.\n"


test_driver: CFLAGS += -Ivendor/munit/
test_driver: test_driver.o 


###################
# INSTALL TARGETS #
###################

install: $(DESTDIR)$(PREFIX)/bin/ldmsnd



#################
# CLEAN TARGETS #
#################
clean:
	rm -f *.o test_driver *.gcda *.gcno *.gcov *.cflow 

distclean: clean
	rm -f *.so lnetwork compile_commands.json 


########
# DOCS #
########
.PHONY: docs
docs:
	$(MAKE) -C docs $@



################
# SOURCE PATHS #
################
vpath %.c src/
vpath %.c.rst src/
vpath %.c.md src/
vpath %.c.rl src/
vpath %.c.rl.md src/
vpath %.c.rl.rst src/
vpath %.h src/
vpath munit.c vendor/munit/
vpath test_%.c tests/


##################
# IMPLICIT RULES #
##################
# {{{

$(DESTDIR)$(PREFIX)/bin:
	@printf "$(INSTALL_COLOR)INSTALL$(NO_COLOR) $@\n"
	$(Q)$(INSTALL) -m 0755 -d $@

$(DESTDIR)$(PREFIX)/lib:
	@printf "$(INSTALL_COLOR)INSTALL$(NO_COLOR) $@\n"
	$(Q)$(INSTALL) -m 0755 -d $@

$(DESTDIR)$(PREFIX)/include:
	@printf "$(INSTALL_COLOR)INSTALL$(NO_COLOR) $@\n"
	$(Q)$(INSTALL) -m 0755 -d $@

$(DESTDIR)$(PREFIX)/lib/%.so: %.so | $(DESTDIR)$(PREFIX)/lib
	@printf "$(INSTALL_COLOR)INSTALL$(NO_COLOR) $@\n"
	$(Q)$(INSTALL) -m 0644 $< $@

$(DESTDIR)$(PREFIX)/lib/%.a: %.a | $(DESTDIR)$(PREFIX)/lib
	@printf "$(INSTALL_COLOR)INSTALL$(NO_COLOR) $@\n"
	$(Q)$(INSTALL) -m 0644 $< $@

$(DESTDIR)$(PREFIX)/include/%.h: %.h | $(DESTDIR)$(PREFIX)/include
	@printf "$(INSTALL_COLOR)INSTALL$(NO_COLOR) $@\n"
	$(Q)$(INSTALL) -m 0644 $< $@

$(DESTDIR)$(PREFIX)/bin/%: % | $(DESTDIR)$(PREFIX)/bin
	@printf "$(INSTALL_COLOR)INSTALL$(NO_COLOR) $@\n"
	$(Q)$(INSTALL) -m 0755 $< $@

%.deps: %
	@printf "$(CC_COLOR)CC$(NO_COLOR) $@\n"
	$(Q)$(CC) -c $(CFLAGS) $(CPPFLAGS) -M $^ | $(SED) -e 's/[\\ ]/\n/g' | $(SED) -e '/^$$/d' -e '/\.o:[ \t]*$$/d' | sort | uniq > $@

%: %.o
	@printf "$(LD_COLOR)LD$(NO_COLOR) $@\n"
	$(Q)$(CROSS_COMPILE)$(CC) $(LDFLAGS) -o $@ $^ $(LOADLIBES) $(LDLIBS)

%.a:
	@printf "$(LD_COLOR)LD$(NO_COLOR) $@\n"
	$(Q)$(AR) rcs $@ $^

%.so: CFLAGS += -fPIC
%.so:
	@printf "$(LD_COLOR)LD$(NO_COLOR) $@\n"
	$(Q)$(CROSS_COMPILE)$(CC) $(LDFLAGS) -shared -o $@ $^ $(LOADLIBES) $(LDLIBS)

%.o: %.c
	@printf "$(CC_COLOR)CC$(NO_COLOR) $@\n"
	$(Q)$(CROSS_COMPILE)$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $^

# UPX-minified binaries
%.upx: %
	@printf "$(LD_COLOR)UPX$(NO_COLOR) $@\n"
	$(Q)$(UPX) -o $@ $^

%.stripped: %
	@printf "$(LD_COLOR)STRIP$(NO_COLOR) $@\n"
	$(Q)$(STRIP) -o $@ $^

# for each c file, it's possible to generate a cflow flow graph.
%.c.cflow: %.c
	@printf "$(CC_COLOR)CC$(NO_COLOR) $@\n"
	$(Q)$(CFLOW) -o $@ $<

%.png: %.dot
	@printf "$(CC_COLOR)CC$(NO_COLOR) $@\n"
	$(Q)$(NEATO) -Tpng -Ln100 -o $@ $<

%.dot: %.rl
	@printf "$(CC_COLOR)CC$(NO_COLOR) $@\n"
	$(Q)$(RAGEL) $(RAGELFLAGS) -V -p $< -o $@

%.c: %.c.rl
	@printf "$(CC_COLOR)CC$(NO_COLOR) $@\n"
	$(Q)$(RAGEL) -Iinclude $(RAGELFLAGS) -o $@ $<

%.c: %.c.rst
	@printf "$(CC_COLOR)CC$(NO_COLOR) $@\n"
	$(Q)cat $< | rst_tangle > $@

# build c files from markdown files - literate programming style
%.c: %.c.md
	@printf "$(CC_COLOR)CC$(NO_COLOR) $@\n"
	$(Q)cat $< | sed -n '/^```c/,/^```/ p' | sed '/^```/ d' > $@

# }}}

#vim: set foldmethod=marker
