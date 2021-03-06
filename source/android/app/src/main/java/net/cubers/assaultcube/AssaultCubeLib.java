// currently unused interface that could be used to communicate between Java and Cpp

package net.cubers.assaultcube;

public class AssaultCubeLib {

     static {
          System.loadLibrary("main");
     }

     public static native boolean hijackvolumekeys();
     public static native boolean allowaskrating();
}
