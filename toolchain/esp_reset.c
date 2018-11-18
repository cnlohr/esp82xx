#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
int main( int argc, char ** argv ) { int fd = open( argv[1], O_RDWR | O_NOCTTY ); int DTR_flag = TIOCM_DTR; ioctl(fd, TIOCMBIC, &DTR_flag ); usleep(20000); ioctl(fd, TIOCMBIS, &DTR_flag ); }
