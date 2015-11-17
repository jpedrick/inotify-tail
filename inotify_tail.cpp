#include <sys/inotify.h>
#include <sys/epoll.h>
#include <limits.h>
#include <vector>
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <cerrno>
#include <fstream>





std::ostream& operator<< ( std::ostream& o, const inotify_event& i ){
    o << "wd:" << i.wd;
    if (i.cookie > 0)
        o << "cookie: " << i.cookie << " ";

    o << "mask:{ ";
    if (i.mask & IN_ACCESS)        o << "IN_ACCESS ";
    if (i.mask & IN_ATTRIB)        o << "IN_ATTRIB ";
    if (i.mask & IN_CLOSE_NOWRITE) o << "IN_CLOSE_NOWRITE ";
    if (i.mask & IN_CLOSE_WRITE)   o << "IN_CLOSE_WRITE ";
    if (i.mask & IN_CREATE)        o << "IN_CREATE ";
    if (i.mask & IN_DELETE)        o << "IN_DELETE ";
    if (i.mask & IN_DELETE_SELF)   o << "IN_DELETE_SELF ";
    if (i.mask & IN_IGNORED)       o << "IN_IGNORED ";
    if (i.mask & IN_ISDIR)         o << "IN_ISDIR ";
    if (i.mask & IN_MODIFY)        o << "IN_MODIFY ";
    if (i.mask & IN_MOVE_SELF)     o << "IN_MOVE_SELF ";
    if (i.mask & IN_MOVED_FROM)    o << "IN_MOVED_FROM ";
    if (i.mask & IN_MOVED_TO)      o << "IN_MOVED_TO ";
    if (i.mask & IN_OPEN)          o << "IN_OPEN ";
    if (i.mask & IN_Q_OVERFLOW)    o << "IN_Q_OVERFLOW ";
    if (i.mask & IN_UNMOUNT)       o << "IN_UNMOUNT ";
    o << "}";

    return o;
}

struct Watched{
    std::string watched;
    int fd;
    std::ifstream* file;
};

int main( int argc, char* argv[] ){
    int inotifyFD = -1;

    struct inotify_event* in_event = nullptr;
    std::vector<Watched> watched;

    inotifyFD = inotify_init();
    if( inotifyFD < 0 ){
        std::cout << "error initializing inotify" << std::endl;
        exit(1);
    }

    watched.reserve(argc - 1);
    for ( int j = 1; j < argc; ++j ){
        std::string watchFile = argv[j];
        int wd = inotify_add_watch(inotifyFD, watchFile.c_str(), IN_ALL_EVENTS);
        watched.emplace_back( Watched{ watchFile, wd, new std::ifstream{ watchFile } } );
        
        if( wd == -1 ) {
            std::cout << "error adding watch" << std::endl;
            exit(1);
        }
    }

    std::vector<epoll_event> _events( 10 );

    int epollFD = epoll_create( _events.size() );
    if( epollFD < 0 ) {
        std::cout << "error creating epoll" << std::endl;
        exit(1);
    }

    epoll_event ep_event;
    ep_event.data.ptr = 0;
    ep_event.events = EPOLLIN | EPOLLET;

    int epollCtl = epoll_ctl( epollFD, EPOLL_CTL_ADD, inotifyFD, &ep_event );

    if( epollCtl < 0 ){
        char buf[128];
        
        std::cout << "error adding notify fd to epoll group: " << ::strerror_r( errno, buf, sizeof(buf) ) << std::endl;
        exit(1);
    }

    for(;;){
        ::memset( _events.data(), 0, sizeof( epoll_event ) * _events.size() );
        int n = epoll_wait( epollFD, _events.data(), _events.size(), 10000 );

        for( int i = 0; i < n; ++i ){
            epoll_event& event = _events[i];
            if( event.events & EPOLLIN ){
                char buf[1024];
                int bytesRead = ::read( inotifyFD, buf, sizeof(buf) );

                if( bytesRead < 0 ){ 
                    std::cout << "read error: bytesRead:" << bytesRead << std::endl;
                    exit(1);
                }

                for( char* p = buf; p < buf + bytesRead; ){
                    in_event = (struct inotify_event*)p;
                    for( Watched& watchedFile : watched ){
                        if( watchedFile.fd == in_event->wd ){
                            if (in_event->mask & IN_MODIFY) {
                                char fbuf[1024];
                                int fbytesRead = 0;
                                do{
                                    fbytesRead = watchedFile.file->readsome( fbuf, sizeof(fbuf) );
                                    if( fbytesRead ) { std::cout.write( fbuf, fbytesRead ); }
                                }while( fbytesRead == sizeof(fbuf) );
                            }
                        }
                    }

                    p += sizeof(struct inotify_event) + in_event->len;
                }
            }
        }
    }

    exit(0);
}

