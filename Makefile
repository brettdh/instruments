SUBDIRS = src tests

.PHONY: subdirs $(SUBDIRS)
subdirs: $(SUBDIRS)
$(SUBDIRS)::
	make -C $@

all: subdirs

clean:
	for dir in $(SUBDIRS); do make -C $$dir clean; done

docs:
	( cd doc; doxygen )
