#include "router.h"
#include <iostream>
#include <fstream>
#include <string>
#include <string.h>
#include <pthread.h>
using namespace std;

int main(int argc, char *argv[])
{
    if (argc != 3) {
        cout << "usage: dsdv port datafile" << endl;
        return 1;
    }
    int port = atoi(argv[1]);
    string datafilename = argv[2];
    cout << datafilename << endl;
    pthread_t sender_tid, receiver_tid;
    init(datafilename);
    int ret1 = pthread_create(&sender_tid, NULL, start_sender, (void *)&datafilename);
    int ret2 = pthread_create(&receiver_tid, NULL, start_receiver, (void *)&port);
    if (ret1 != 0 || ret2 != 0) {
        cerr << "error: please restart" << endl;
        pthread_exit(NULL);
        return 1;
    }
    pthread_join(sender_tid, NULL);
    pthread_join(receiver_tid, NULL);
    cout << port << ": exit" << endl;
    return 0;
}
