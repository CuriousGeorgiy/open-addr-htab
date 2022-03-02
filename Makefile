CC       = gcc
CFLAGS	 = -I.
CFLAGS	+= -O0 -g3 -ggdb -gdwarf-4
CFLAGS  += -msse4.2

VALGRIND_OPTS = -s --leak-check=full  --show-leak-kinds=all --track-origins=yes \
                --expensive-definedness-checks=yes

LCOV_OPTS = -d . -c

GENHTML_OPTS = --function-coverage --branch-coverage

MAKEDEPEND_OPTS = -m

SRCS           = test.c open_addr_htab.c errinj.c
OBJS	       = $(SRCS:.c=.o)
COV_DATA       = $(SRCS:.c=.gcno)
COV_RES        = $(SRCS:.c=.gcda)
EXEC_DEFAULT   = test-open-addr-htab
EXEC_COV       = test-open-addr-htab-cov
COV_INFO       = cov.info
COV_REPORT_DIR = report

all : clean $(EXEC_DEFAULT)

test : clean $(EXEC_DEFAULT)
	./$(EXEC_DEFAULT)

coverage: clean $(COV_INFO)
	genhtml $(GENTML_OPTS) -o $(COV_REPORT_DIR) $(COV_INFO)
	lcov --list $(COV_INFO)

valgrind : CFLAGS += -O1
valgrind : clean $(EXEC_DEFAULT)
	valgrind $(VALGRIND_OPTS) ./$(EXEC_DEFAULT)

$(EXEC_DEFAULT) : $(OBJS)
	$(CC) $(OBJS) -o $@

$(EXEC_COV): CFLAGS += --coverage
$(EXEC_COV): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(COV_INFO) : $(EXEC_COV)
	./$(EXEC_COV)
	lcov $(LCOV_OPTS) -o $(COV_INFO)

depend :
	makedepend $(MAKEDEPEND_OPTS) -- $(CFLAGS) -- $(SRCS)

clean :
	rm -rf $(EXEC_DEFAULT) $(EXEC_COV) $(OBJS) \
           $(COV_DATA) $(COV_RES) $(COV_INFO) $(COV_REPORT_DIR)

.PHONY : depend clean

# DO NOT DELETE THIS LINE

open_addr_htab.o: open_addr_htab.h
test.o: open_addr_htab.h
