AUTOMAKE_OPTIONS=foreign

shifter_etc_filesdir = $(shifter_etc_files)
dist_shifter_etc_files_DATA = nsswitch.conf passwd group

passwd:
	getent passwd > ./passwd

group:
	getent group > ./group
