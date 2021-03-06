// currently unused interface that could be used to communicate between Java and Cpp

#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern "C" {
    JNIEXPORT bool JNICALL Java_net_cubers_assaultcube_AssaultCubeLib_hijackvolumekeys(JNIEnv* env, jobject obj);
    JNIEXPORT bool JNICALL Java_net_cubers_assaultcube_AssaultCubeLib_allowaskrating(JNIEnv* env, jobject obj);
    //JNIEXPORT void JNICALL Java_net_cubers_assaultcube_AssaultCubeLib_resize(JNIEnv* env, jobject obj, jint width, jint height);
};

extern bool hijackvolumebuttons();
extern bool allowaskrating();

// determines if the volume keys should be hijacked so that the application can use them for its own purpose
JNIEXPORT bool JNICALL
Java_net_cubers_assaultcube_AssaultCubeLib_hijackvolumekeys(JNIEnv* env, jobject obj) {
    return hijackvolumebuttons();
}

// determines if the user may be asked to provide a rating
JNIEXPORT bool JNICALL
Java_net_cubers_assaultcube_AssaultCubeLib_allowaskrating(JNIEnv* env, jobject obj) {
    return allowaskrating();
}

/*
JNIEXPORT void JNICALL
Java_net_cubers_assaultcube_AssaultCubeLib_resize(JNIEnv* env, jobject obj, jint width, jint height) {
}*/
