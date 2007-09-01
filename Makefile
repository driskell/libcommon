##############################################################################
#          pfixtools: a collection of postfix related tools                  #
#          ~~~~~~~~~                                                         #
#  ________________________________________________________________________  #
#                                                                            #
#  Redistribution and use in source and binary forms, with or without        #
#  modification, are permitted provided that the following conditions        #
#  are met:                                                                  #
#                                                                            #
#  1. Redistributions of source code must retain the above copyright         #
#     notice, this list of conditions and the following disclaimer.          #
#  2. Redistributions in binary form must reproduce the above copyright      #
#     notice, this list of conditions and the following disclaimer in the    #
#     documentation and/or other materials provided with the distribution.   #
#  3. The names of its contributors may not be used to endorse or promote    #
#     products derived from this software without specific prior written     #
#     permission.                                                            #
#                                                                            #
#  THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND   #
#  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE     #
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR        #
#  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS    #
#  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR    #
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF      #
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS  #
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN   #
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)   #
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF    #
#  THE POSSIBILITY OF SUCH DAMAGE.                                           #
##############################################################################

all:

LDFLAGS += -Wl,--warn-common

include mk/cflags.mk

CFLAGS += --std=gnu99 -D_GNU_SOURCE
prefix ?= /usr/local

PROGRAMS = postlicyd pfix-srsd
TESTS    = tst-rbl

GENERATED = tokens.h tokens.c

postlicyd_SOURCES = common.c str.c buffer.c rbl.c \
		    $(GENERATED) postfix.c postlicyd.c
postlicyd_LIBADD = -lpthread

pfix-srsd_SOURCES = common.c buffer.c str.c srsd.c
pfix-srsd_LIBADD = -lsrs2

tst-rbl_SOURCES = tst-rbl.c

install: all
	install -d $(DESTDIR)$(prefix)/sbin
	install $(PROGRAMS) $(DESTDIR)$(prefix)/sbin
	install -d $(DESTDIR)/etc/pfixtools

# RULES ###################################################################{{{

all: $(GENERATED) $(PROGRAMS) | $(GENERATED)

clean:
	$(RM) $(PROGRAMS) $(TESTS) .*.o .*.dep

distclean: clean
	$(RM) $(GENERATED)

tags: .tags
.tags: $(shell git ls-files | egrep '\.[hc]$$')
	ctags -o $@ $^

headers: HEADACHEOPTS=-c mk/headache.cfg -h COPYING
headers:
	@which headache > /dev/null || \
		( echo "package headache not installed" ; exit 1 )
	@git ls-files | egrep '(\.h|\.c|Makefile|*\.mk)$$' | xargs -t headache $(HEADACHEOPTS)

%.h: %.sh
	./$< $@ || ($(RM) $@; exit 1)

%.c: %.sh
	./$< $@ || ($(RM) $@; exit 1)

.%.o: %.c Makefile
	$(CC) $(CFLAGS) -MMD -MT ".$*.dep $@" -MF .$*.dep -g -c -o $@ $<

.%.dep: .%.o

.SECONDEXPANSION:

$(PROGRAMS) $(TESTS): $$(patsubst %.c,.%.o,$$($$@_SOURCES)) Makefile common.ld
	$(CC) -o $@ $(CFLAGS) $($@_CFLAGS) $(filter %.ld,$^) $(filter %.o,$^) $(LDFLAGS) $($@_LIBADD) $(filter %.a,$^)

-include $(foreach p,$(PROGRAMS) $(TESTS),$(patsubst %.c,.%.dep,$(filter %.c,$($p_SOURCES))))

###########################################################################}}}
