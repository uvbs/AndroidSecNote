javac -encoding UTF-8 -source 1.7 -target 1.7 -bootclasspath E:\AS_SDK\platforms\android-25/android.jar ./com/expample/tools/*.java
E:\AS_SDK\build-tools\25.0.2\dx.bat --dex --output=./inject.dex ./com/expample/tools/*
ndk-build