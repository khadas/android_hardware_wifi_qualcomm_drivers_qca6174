# Makefile for Btdiag

CFLAGS += -g -DDEBUG -DLINUX_X86
LDLIBS = -lpthread -lrt
ifeq ($(CONFIG_BT_USB), y) 
CFLAGS += -DBTDIAG_SUPPORT_BT_USB
LDLIBS += -lbluetooth
endif

.PHONY: all clean

Btdiag: Btdiag.o connection.o hciattach_rome.o privateMembers.o uart.o

Btdiag.o: Btdiag.c
	${CC} -c $(CFLAGS) Btdiag.c
connection.o: connection.c connection.h
	${CC} -c $(CFLAGS) connection.c
hciattach_rome.o: hciattach_rome.c hciattach_rome.h
	${CC} -c $(CFLAGS) hciattach_rome.c
privateMembers.o : privateMembers.c privateMembers.h
	${CC} -c $(CFLAGS) privateMembers.c
uart.o : uart.c uart.h
	${CC} -c $(CFLAGS) uart.c
	
clean:
	-$(RM) *.o 
	-$(RM)  Btdiag
