SUBDIRS = unit acceptance

.PHONY: subdirs $(SUBDIRS)
subdirs: $(SUBDIRS)
$(SUBDIRS)::
	make -C $@

all: subdirs

clean:
	for dir in $(SUBDIRS); do make -C $$dir clean; done
