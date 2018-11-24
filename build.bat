@echo off
set PATH=%PATH%;C:\android-ndk-r11c
ndk-build V=1 NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=Android.mk NDK_APPLICATION_MK=Application.mk
