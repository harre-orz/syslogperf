#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <syslog.h>

// RFCの例
// <34>Oct  7 22:14:15 mymachine su[10]: 'su root' failed for lonvick on /dev/pts/8
// | ---------- HEADER部 --------- | ---- MSG部---------------------
// | <pri値> | TIMESTAMP | ホスト名 | プロセス名[PID]: メッセージ

static int Socket;
static char Buffer[2048];
static size_t BufLen;
static int Priority = LOG_USER | LOG_DEBUG;
//static char const * Hostname = "localhost";
static char const * Procname = "syslog_perf";
static struct tm Now;

static void init(char const * log_path) {
    int soc;
    if ((soc = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_un sun;
    memset(&sun, 0, sizeof(sun));
    sun.sun_family = AF_UNIX;
    strncpy(sun.sun_path, log_path, sizeof(sun.sun_path));
    if (connect(soc, (struct sockaddr const *)&sun, sizeof(sun)) == -1) {
        perror("bind");
        exit(1);
    }

    Socket = soc;
}

static void format(int len) {
    time_t t;
    time(&t);
    localtime_r(&t, &Now);

    char * ptr = Buffer;
    ptr += sprintf(ptr, "<%d>", Priority);
    strftime(ptr, 100, "%h %e %T ", &Now);
    ptr = Buffer + strlen(Buffer);
    ptr += sprintf(ptr, "%s[%d]: ", Procname, getpid());
    for(int i = 0; i < len; ++i) {
        if (ptr > Buffer + sizeof(Buffer)) {
            break;
        }
        *ptr++ = '0' + (i % 10);
    }
    Buffer[1024] = '\0';
    BufLen = strlen(Buffer);
    fprintf(stderr, "# %s\n", Buffer);
}

void usage() {
    fprintf(stderr,
            "syslogperf [-hvp] -n num -l num\n"
            "\t -n num : The number of send to /dev/null socket. default=100\n"
            "\t -l num : The length of message. default=100\n"
            "\t -p     : Prints plot data\n"
            "\t -h     : This help message.\n"
            "\t -v     : Prints this program version.\n"
            "\n");
}

int main(int argc, char ** argv) {
    int msglen = 100;
    int counts = 100;
    int opt;
    int * Results;
    bool progress = false;

    while((opt = getopt(argc, argv, "n:l:phv")) != -1) {
        switch(opt) {
            case 'n':
                if ((counts = atoi(optarg)) == 0) {
                    usage();
                    exit(1);
                }
                break;
            case 'l':
                if ((msglen = atoi(optarg)) == 0) {
                    usage();
                    exit(1);
                }
                break;
            case 'p':
                progress = true;
                break;
            default:
                usage();
                exit(1);
        }
    }

    init("/dev/log");

    format(msglen);
    if ((Results = malloc(sizeof(int) * counts)) == NULL) {
        perror("malloc");
        exit(1);
    }

    struct timespec tp1, tp2;
    int nanosec;
    clock_gettime(CLOCK_MONOTONIC, &tp1);
    for(int i = 0; i < counts; ++i) {
        if (send(Socket, Buffer, BufLen, 0) <= 0) {
            perror("send");
            exit(1);
        }
        clock_gettime(CLOCK_MONOTONIC, &tp2);

        nanosec = tp2.tv_nsec - tp1.tv_nsec;
        if (nanosec < 0) {
            nanosec += 1000000000;
            --tp2.tv_nsec;
        }
        if (tp2.tv_sec != tp1.tv_sec) {
            nanosec += (1000000000 * (tp2.tv_sec - tp1.tv_sec));
            if (nanosec < 0) {
                fprintf(stderr, "It's too late :(");
                exit(1);
            }
        }
        Results[i] = nanosec;

        tp1 = tp2;
    }

    int min_val = INT32_MAX, max_val = 0;
    size_t sum_val = 0;
    for(int i = 0; i < counts; ++i) {
        sum_val += Results[i];
        min_val = min_val < Results[i] ? min_val : Results[i];
        max_val = max_val > Results[i] ? max_val : Results[i];
        if (progress)
            printf("%d 0.%09d\n", i, Results[i]);
    }
    fprintf(stderr, "# average %.9lf [s], minimum %.9lf [s], maximum %.9lf [s]\n",
            (double)sum_val / (double)counts / 1000000000,
            (double)min_val / 1000000000,
            (double)max_val / 1000000000);

    free(Results);

    return 0;
}
