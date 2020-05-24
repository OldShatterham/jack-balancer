CC = g++
CFLAGS = -Wall

LIBS = -ljack

ODIR = bin
ONAME = jack-balancer

DESTDIR :=
PREFIX := /usr/local

jack-balancer: jack-balancer.cpp
	mkdir -p $(ODIR)
	$(CC) $(CFLAGS) jack-balancer.cpp -o $(ODIR)/$(ONAME) $(LIBS)

.PHONY: clean
clean:
	$(RM) $(ODIR)/$(ONAME)

.PHONY: install
install:
	install -Dm755 $(ODIR)/$(ONAME) $(DESTDIR)$(PREFIX)/bin/$(ONAME)

.PHONY: uninstall
uninstall:
	$(RM) $(DESTDIR)$(PREFIX)/bin/$(ONAME)
