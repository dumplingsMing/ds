all: 
	g++ -std=c++0x -pthread dsdv.cpp router.cpp -o dsdv
clean:
	rm -f *~ *.o dsdv *.ans test_c changein
run:
	./dsdv 3031 a.dat > a.ans &
	./dsdv 3032 b.dat > b.ans &
	./dsdv 3033 c.dat > c.ans &
	./dsdv 3034 d.dat > d.ans &
	./dsdv 3035 e.dat > e.ans &
	./dsdv 3036 f.dat > f.ans &
restart:
	make
	make run
kill:
	ps |grep dsdv|awk '{print $$1}'|xargs kill -9
test:
	g++ -std=c++0x -o  test_c test_c.cpp
	gcc -o changein changein.c
grade:
	./test_c a b c d e f a.dat b.dat c.dat d.dat e.dat f.dat
