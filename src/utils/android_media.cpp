#include "utils/MediaScan.hpp"

#ifdef GEODE_IS_ANDROID

#include <Geode/Geode.hpp>
#include <Geode/cocos/platform/android/jni/JniHelper.h>
#include <jni.h>

using namespace geode::prelude;

namespace media {

namespace {

void releaseAll(JNIEnv* env, std::initializer_list<jobject> refs) {
    for (auto* ref : refs) {
        if (ref) env->DeleteLocalRef(ref);
    }
}

}

void triggerMediaScan(std::string const& path) {
    JavaVM* vm = cocos2d::JniHelper::getJavaVM();
    if (!vm) {
        geode::log::error("Failed to get JavaVM for media scan");
        return;
    }

    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            geode::log::error("Failed to attach thread for media scan");
            return;
        }
    }

    jclass activityThreadClass = nullptr;
    jobject activityThread = nullptr;
    jobject application = nullptr;
    jclass mediaScannerClass = nullptr;
    jclass stringClass = nullptr;
    jstring pathStr = nullptr;
    jobjectArray pathArray = nullptr;

    activityThreadClass = env->FindClass("android/app/ActivityThread");
    if (!activityThreadClass) {
        geode::log::error("Failed to find ActivityThread class");
        return;
    }

    jmethodID currentActivityThreadMethod =
        env->GetStaticMethodID(activityThreadClass, "currentActivityThread", "()Landroid/app/ActivityThread;");
    if (!currentActivityThreadMethod) {
        geode::log::error("Failed to get currentActivityThread method");
        releaseAll(env, {activityThreadClass});
        return;
    }

    activityThread = env->CallStaticObjectMethod(activityThreadClass, currentActivityThreadMethod);
    if (!activityThread) {
        geode::log::error("Failed to get current activity thread");
        releaseAll(env, {activityThreadClass});
        return;
    }

    jmethodID getApplicationMethod =
        env->GetMethodID(activityThreadClass, "getApplication", "()Landroid/app/Application;");
    if (!getApplicationMethod) {
        geode::log::error("Failed to get getApplication method");
        releaseAll(env, {activityThread, activityThreadClass});
        return;
    }

    application = env->CallObjectMethod(activityThread, getApplicationMethod);
    if (!application) {
        geode::log::error("Failed to get application");
        releaseAll(env, {activityThread, activityThreadClass});
        return;
    }

    mediaScannerClass = env->FindClass("android/media/MediaScannerConnection");
    if (!mediaScannerClass) {
        geode::log::error("Failed to find MediaScannerConnection class");
        releaseAll(env, {application, activityThread, activityThreadClass});
        return;
    }

    jmethodID scanFileMethod =
        env->GetStaticMethodID(mediaScannerClass, "scanFile",
                               "(Landroid/content/Context;[Ljava/lang/String;[Ljava/lang/String;"
                               "Landroid/media/MediaScannerConnection$OnScanCompletedListener;)V");
    if (!scanFileMethod) {
        geode::log::error("Failed to get scanFile method");
        releaseAll(env, {mediaScannerClass, application, activityThread, activityThreadClass});
        return;
    }

    stringClass = env->FindClass("java/lang/String");
    if (!stringClass) {
        geode::log::error("Failed to find String class");
        releaseAll(env, {mediaScannerClass, application, activityThread, activityThreadClass});
        return;
    }

    pathStr = env->NewStringUTF(path.c_str());
    pathArray = env->NewObjectArray(1, stringClass, pathStr);
    if (!pathArray) {
        geode::log::error("Failed to allocate path array for media scan");
        releaseAll(env, {pathStr, stringClass, mediaScannerClass, application, activityThread, activityThreadClass});
        return;
    }

    env->CallStaticVoidMethod(mediaScannerClass, scanFileMethod, application, pathArray, nullptr, nullptr);

    releaseAll(env, {pathArray, pathStr, stringClass, mediaScannerClass, application, activityThread,
                     activityThreadClass});
}

}

#endif
