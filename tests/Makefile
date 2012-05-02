SUBDIRS = unit acceptance

.PHONY: subdirs $(SUBDIRS)
subdirs: $(SUBDIRS)
$(SUBDIRS)::
	make -C $@

all: subdirs
