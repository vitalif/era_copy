all: era_copy era_apply
era_copy: era_copy.c
	gcc -o era_copy -Wall era_copy.c
era_apply: era_apply.c
	gcc -o era_apply -Wall era_apply.c
