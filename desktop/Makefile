INCLUDES = \
	-I.\
	-I../source/basic

CC = gcc
CFLAGS = -g $(INCLUDES) -DDESKTOP

PROGRAM = basic
OBJDIR = objects

OBJS = \
	$(OBJDIR)/main.o\
	$(OBJDIR)/basicproc.o\
	$(OBJDIR)/basicexpr.o\
	$(OBJDIR)/basicexec.o\
	$(OBJDIR)/mystr.o

$(info making object directory)
$(shell mkdir -p $(OBJDIR))

all: basic

clean:
	rm -rf $(OBJDIR) $(PROGRAM)

basic: $(OBJS)
	$(CC) -o $(PROGRAM) -g $(OBJS)

$(OBJDIR)/main.o : main.c
	$(CC) -c $(CFLAGS) $< -o $@

$(OBJDIR)/basicproc.o : ../source/basic/basicproc.c
	$(CC) -c $(CFLAGS) $< -o $@

$(OBJDIR)/basicexpr.o : ../source/basic/basicexpr.c
	$(CC) -c $(CFLAGS) $< -o $@

$(OBJDIR)/basicexec.o : ../source/basic/basicexec.c
	$(CC) -c $(CFLAGS) $< -o $@

$(OBJDIR)/mystr.o : ../source/basic/mystr.c
	$(CC) -c $(CFLAGS) $< -o $@