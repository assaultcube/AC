// currently unused interface that could be used to communicate between Java and Cpp

#include "../../../../../src/cube.h"
#include "../../../../../include/SDL_thread.h"
#include <jni.h>

extern "C" {
    JNIEXPORT void JNICALL Java_net_cubers_assaultcube_AssaultCubeServer_init(JNIEnv* env, jobject obj);
};

int localservermain(void *ptr)
{
    char *argv[] = { "-c4" };
    int argc = sizeof(argv)/sizeof(argv[0]);
    extern int main(int argc, char **argv);
    main(argc, argv);
}

JNIEXPORT void JNICALL
Java_net_cubers_assaultcube_AssaultCubeServer_init(JNIEnv* env, jobject obj) {
    // not implemented yet
    SDL_CreateThread(localservermain, NULL, NULL);
}

