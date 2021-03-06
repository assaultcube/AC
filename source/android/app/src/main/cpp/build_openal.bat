cd oboe
cmake .

cd ..

cd openal
cd build
C:\Users\adria\AppData\Local\Android\sdk\cmake\3.10.2.4988404\bin\cmake.exe -DALSOFT_REQUIRE_SDL2=ON -DCMAKE_FIND_DEBUG_MODE=ON ..
# https://github.com/kcat/openal-soft/issues/132


"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" .. -DCMAKE_TOOLCHAIN_FILE=../cmake/android.toolchain.cmake -DANDROID_NDK=C:/Users/adria/AppData/Local/Android/sdk/ndk/21.3.6528147 -DCMAKE_BUILD_TYPE=Release -DANDROID_ABI=armeabi -DANDROID_NATIVE_API_LEVEL=android-18 -DANDROID_STL=gnustl_static -DALSOFT_EMBED_HRTF_DATA=TRUE


SET SDL2PATH=C:/DEV/ac/source/android/app/src/main/cpp/SDL2

"C:\Users\adria\AppData\Local\Android\sdk\cmake\3.10.2.4988404\bin\cmake.exe" .. -DCMAKE_TOOLCHAIN_FILE=C:\Users\adria\AppData\Local\Android\sdk\ndk\21.3.6528147\build\cmake\android.toolchain.cmake -DANDROID_NDK=C:/Users/adria/AppData/Local/Android/sdk/ndk/21.3.6528147 -DCMAKE_BUILD_TYPE=Release -DANDROID_ABI=armeabi -DANDROID_NATIVE_API_LEVEL=android-18 -DANDROID_STL=gnustl_static -DALSOFT_EMBED_HRTF_DATA=TRUE


"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" .. -DCMAKE_TOOLCHAIN_FILE=C:\Users\adria\AppData\Local\Android\sdk\ndk\21.3.6528147\build\cmake\android.toolchain.cmake -DANDROID_NDK=C:/Users/adria/AppData/Local/Android/sdk/ndk/21.3.6528147 -DCMAKE_BUILD_TYPE=Release -DANDROID_ABI=armeabi-v7a -DANDROID_NATIVE_API_LEVEL=android-18

"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" ..  -G "MinGW Makefiles" -DCMAKE_MAKE_PROGRAM="C:\Users\adria\AppData\Local\Android\sdk\ndk\21.3.6528147\prebuilt\windows\bin\make.exe" -DCMAKE_TOOLCHAIN_FILE=C:\Users\adria\AppData\Local\Android\sdk\ndk\21.3.6528147\build\cmake\android.toolchain.cmake -DANDROID_NDK=C:/Users/adria/AppData/Local/Android/sdk/ndk/21.3.6528147 -DCMAKE_BUILD_TYPE=Release -DANDROID_ABI=armeabi-v7a -DANDROID_NATIVE_API_LEVEL=android-18


"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" ..  -G "Ninja" -DCMAKE_MAKE_PROGRAM="C:\Users\adria\AppData\Local\Android\sdk\ndk\21.3.6528147\prebuilt\windows\bin\make.exe" -DCMAKE_TOOLCHAIN_FILE=C:\Users\adria\AppData\Local\Android\sdk\ndk\21.3.6528147\build\cmake\android.toolchain.cmake -DANDROID_NDK=C:/Users/adria/AppData/Local/Android/sdk/ndk/21.3.6528147 -DCMAKE_BUILD_TYPE=Release -DANDROID_NATIVE_API_LEVEL=android-18

"C:\Users\adria\AppData\Local\Android\sdk\cmake\3.10.2.4988404\bin\cmake.exe" .. -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=C:\Users\adria\AppData\Local\Android\sdk\ndk\21.3.6528147\build\cmake\android.toolchain.cmake -DCMAKE_MAKE_PROGRAM="C:\Users\adria\AppData\Local\Android\sdk\ndk\21.3.6528147\prebuilt\windows-x86_64\bin\make.exe" -DANDROID_NDK=C:/Users/adria/AppData/Local/Android/sdk/ndk/21.3.6528147 -DCMAKE_BUILD_TYPE=Release -DANDROID_NATIVE_API_LEVEL=android-18

#libogg
# https://stackoverflow.com/q/54470140/50430

"C:\Users\adria\AppData\Local\Android\sdk\cmake\3.10.2.4988404\bin\cmake.exe" .. -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=C:\Users\adria\AppData\Local\Android\sdk\ndk\21.3.6528147\build\cmake\android.toolchain.cmake -DCMAKE_MAKE_PROGRAM=C:\Users\adria\AppData\Local\Android\sdk\cmake\3.10.2.4988404\bin\ninja.exe -DANDROID_NDK=C:/Users/adria/AppData/Local/Android/sdk/ndk/21.3.6528147 -DCMAKE_BUILD_TYPE=Release -DANDROID_NATIVE_API_LEVEL=android-21 -DANDROID_PLATFORM=android-21 DANDROID_ABI=armeabi-v7a

#openal

cd build
"C:\Users\adria\AppData\Local\Android\sdk\cmake\3.10.2.4988404\bin\cmake.exe" .. -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=C:\DEV\ac\source\android\app\src\main\cpp\toolchain.android.cmake -DCMAKE_MAKE_PROGRAM=C:\Users\adria\AppData\Local\Android\sdk\cmake\3.10.2.4988404\bin\ninja.exe -DANDROID_NDK=C:/Users/adria/AppData/Local/Android/sdk/ndk/21.3.6528147 -DCMAKE_BUILD_TYPE=Release -DANDROID_NATIVE_API_LEVEL=android-21 -DANDROID_PLATFORM=android-21 -DANDROID_ABI=armeabi-v7a -DANDROID_HOST_TAG=windows-x86_64


https://github.com/JogAmp/openal-soft/blob/master/cmake/toolchain.android.cmake