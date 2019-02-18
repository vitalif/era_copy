all: era_copy
era_copy: era_copy.c
	gcc -o era_copy -Wall era_copy.c
