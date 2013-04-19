BUILD_SUBDIRS = threads userprog vm filesys

all::
	@echo "Run 'make' in subdirectories: $(BUILD_SUBDIRS)."
	@echo "This top-level make has only 'clean' targets."

CLEAN_SUBDIRS = $(BUILD_SUBDIRS) examples utils

clean::
	for d in $(CLEAN_SUBDIRS); do $(MAKE) -C $$d $@; done
	rm -f TAGS tags

distclean:: clean
	find . -name '*~' -exec rm '{}' \;

TAGS_SUBDIRS = $(BUILD_SUBDIRS) devices lib
TAGS_SOURCES = find $(TAGS_SUBDIRS) -name \*.[chS] -print

TAGS::
	etags --members `$(TAGS_SOURCES)`

tags::
	ctags -T --no-warn `$(TAGS_SOURCES)`

cscope.files::
	$(TAGS_SOURCES) > cscope.files

cscope:: cscope.files
	cscope -b -q -k

##################
# Handin your work
##################
TAID = "yjkwon"

turnin.tar: clean
	tar cf turnin.tar `find . -type f | grep -v '^\.*$$' | grep -v '/CVS/' | grep -v '/\.svn/' | grep -v '/\.git/' | grep -v '*\.tar\.gz' | grep -v '/\~/' | grep -v '/\.txt' | grep -v '/\.pl' |grep -v '/\.tar/'` 

TURNIN := /lusr/bin/turnin
GRADER := tw7877
LAB1_NAME := threads
LAB2_NAME := user
LAB3_NAME := vm
LAB4_NAME := fs

turnin_threads: turnin.tar
	echo "Turning in threads_turnin.tar containing the following files:"
	tar tf turnin.tar
	mv turnin.tar threads_turnin.tar
	$(TURNIN) --submit $(GRADER) $(LAB1_NAME) threads_turnin.tar

turnin_user: turnin.tar
	echo "Turning in user_turnin.tar containing the following files:"
	tar tf turnin.tar
	mv turnin.tar user_turnin.tar
	$(TURNIN) --submit $(GRADER) $(LAB2_NAME) user_turnin.tar

turnin_vm: turnin.tar
	echo "Turning in vm_turnin.tar containing the following files:"
	tar tf turnin.tar
	mv turnin.tar vm_turnin.tar
	$(TURNIN) --submit $(GRADER) $(LAB3_NAME) vm_turnin.tar

turnin_fs: turnin.tar
	echo "Turning in fs_turnin.tar containing the following files:"
	tar tf turnin.tar
	mv turnin.tar fs_turnin.tar
	$(TURNIN) --submit $(GRADER) $(LAB4_NAME) fs_turnin.tar
