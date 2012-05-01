SUBDIRS = src tests

.PHONY: subdirs $(SUBDIRS)
subdirs: $(SUBDIRS)
$(SUBDIRS)::
	make -C $@

all: subdirs
