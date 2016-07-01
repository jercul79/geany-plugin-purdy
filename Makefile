all:
	@echo "Create object"
	@gcc -c backup.c -fPIC `pkg-config --cflags geany` \
		-D 'DEF_PROJECT_DIR="/home/harmanci/projects/"' \
		-D 'DEF_BACKUP_DIR="/home/harmanci/backups/"'
	@gcc -c version_cache.c -fPIC `pkg-config --cflags geany` \
		-D 'DEF_PROJECT_DIR="/home/harmanci/projects/"' \
		-D 'DEF_BACKUP_DIR="/home/harmanci/backups/"'
	@gcc -c purdy.c -fPIC `pkg-config --cflags geany` \
		-D 'DEF_PROJECT_DIR="/home/harmanci/projects/"' \
		-D 'DEF_BACKUP_DIR="/home/harmanci/backups/"'

	@echo "Create dll"
	@gcc purdy.o version_cache.o backup.o -o purdy.so -shared `pkg-config --libs geany` 

install:
	cp purdy.so /usr/lib64/geany/
