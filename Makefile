
TARGET=inotify-tail

${TARGET}: inotify_tail.cpp
	g++ -std=c++11 inotify_tail.cpp -o ${TARGET}

clean: inotify-tail
	rm ${TARGET}
