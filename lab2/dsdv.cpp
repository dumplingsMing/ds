#include "router.h"
#include <iostream>
#include <fstream>
#include <string>
#include <string.h>
#include <pthread.h>
using namespace std;

int main(int argc char *argv[])
{
    if (argc != 3)
        cout << "usage: dsdv port datafile" << endl;
    int port = atoi(argv[1]);
    string dataFileName = argv[2];
    int sender_tid, receiver_tid;
    int ret1 = pthread_create(&sender_tid, start_sender, NULL, (void *)&dataFileName);
    int ret2 = pthread_create(&receiver_tid, start_receiver, NULL, (void *)&port);
    if (ret1 != 0 || ret2 == 0) {
        cerr << "error: please restart" << endl;
        return 1;
    }
    pthread_exit(NULL);
    return 0;
}
