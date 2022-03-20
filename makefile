collection: collection.o sqlite3.o
	gcc -o collection.o sqlite3.o 
sqlite3.o: sqlite3.c 
	gcc -c -std=c99 sqlite3.c
collection.o: collection.c
	gcc -c collection.c
clean:
	rm -f collection collection.o sqlite3.o