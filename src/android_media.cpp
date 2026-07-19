#ifdef GEODE_IS_ANDROID

#include <Geode/Geode.hpp>
#include <Geode/cocos/platform/android/jni/JniHelper.h>
#include <jni.h>

using namespace geode::prelude;

extern "C" void triggerMediaScan(std::string const& path) {
    // JNI_GetCreatedJavaVMs is not exported for app libraries on Android; use the
    // JavaVM cocos2d-x caches in JNI_OnLoad instead.
    JavaVM* vm = cocos2d::JniHelper::getJavaVM();
    JNIEnv* env = nullptr;
    if (!vm) {
        geode::log::error("Failed to get JavaVM for media scan");
        return;
    }

    jint result = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (result != JNI_OK) {
        result = vm->AttachCurrentThread(&env, nullptr);
        if (result != JNI_OK) {
            geode::log::error("Failed to attach thread for media scan");
            return;
        }
    }

    try {
        jclass activityThreadClass = env->FindClass("android/app/ActivityThread");
        if (!activityThreadClass) {
            geode::log::error("Failed to find ActivityThread class");
            return;
        }

        jmethodID currentActivityThreadMethod = env->GetStaticMethodID(
            activityThreadClass, "currentActivityThread", "()Landroid/app/ActivityThread;");
        if (!currentActivityThreadMethod) {
            geode::log::error("Failed to get currentActivityThread method");
            env->DeleteLocalRef(activityThreadClass);
            return;
        }

        jobject activityThread = env->CallStaticObjectMethod(activityThreadClass, currentActivityThreadMethod);
        if (!activityThread) {
            geode::log::error("Failed to get current activity thread");
            env->DeleteLocalRef(activityThreadClass);
            return;
        }

        jmethodID getApplicationMethod = env->GetMethodID(
            activityThreadClass, "getApplication", "()Landroid/app/Application;");
        if (!getApplicationMethod) {
            geode::log::error("Failed to get getApplication method");
            env->DeleteLocalRef(activityThreadClass);
            env->DeleteLocalRef(activityThread);
            return;
        }

        jobject application = env->CallObjectMethod(activityThread, getApplicationMethod);
        if (!application) {
            geode::log::error("Failed to get application");
            env->DeleteLocalRef(activityThreadClass);
            env->DeleteLocalRef(activityThread);
            return;
        }

        jclass mediaScannerClass = env->FindClass("android/media/MediaScannerConnection");
        if (!mediaScannerClass) {
            geode::log::error("Failed to find MediaScannerConnection class");
            env->DeleteLocalRef(activityThreadClass);
            env->DeleteLocalRef(activityThread);
            env->DeleteLocalRef(application);
            return;
        }

        jmethodID scanFileMethod = env->GetStaticMethodID(
            mediaScannerClass,
            "scanFile",
            "(Landroid/content/Context;[Ljava/lang/String;[Ljava/lang/String;Landroid/media/MediaScannerConnection$OnScanCompletedListener;)V"
        );
        if (!scanFileMethod) {
            geode::log::error("Failed to get scanFile method");
            env->DeleteLocalRef(activityThreadClass);
            env->DeleteLocalRef(activityThread);
            env->DeleteLocalRef(application);
            env->DeleteLocalRef(mediaScannerClass);
            return;
        }

        jstring pathStr = env->NewStringUTF(path.c_str());
        jclass stringClass = env->FindClass("java/lang/String");
        jobjectArray pathArray = env->NewObjectArray(1, stringClass, pathStr);

        jobjectArray mimeArray = nullptr;

        env->CallStaticVoidMethod(mediaScannerClass, scanFileMethod, application, pathArray, mimeArray, nullptr);

        env->DeleteLocalRef(pathStr);
        env->DeleteLocalRef(stringClass);
        env->DeleteLocalRef(pathArray);
        env->DeleteLocalRef(mediaScannerClass);
        env->DeleteLocalRef(application);
        env->DeleteLocalRef(activityThread);
        env->DeleteLocalRef(activityThreadClass);

        geode::log::debug("Triggered media scan for: {}", path);
    } catch (...) {
        geode::log::error("Exception during media scan");
    }
}

#endif
