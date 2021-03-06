package net.cubers.assaultcube;

// not implemented yet
public class AssaultCubeServer {

     static {
          System.loadLibrary("server");
     }

     public static native void init();
}
