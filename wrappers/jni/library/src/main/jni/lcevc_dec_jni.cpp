/* Copyright (c) V-Nova International Limited 2024-2026. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License by V-Nova Limited.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. However, the
 * software may be incorporated into a project under a compatible license provided the requirements
 * of the BSD-3-Clause-Clear license are respected, and V-Nova Limited remains
 * licensor of the software ONLY UNDER the BSD-3-Clause-Clear license (not the compatible license).
 * ANY ONWARD DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO
 * THE EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#include <android/log.h>
#include <android/native_window_jni.h>
#include <LCEVC/extract/extract.h>
#include <LCEVC/lcevc_dec.h>
#include <sys/system_properties.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <vector>

constexpr const char* DEC_TAG = "LCEVCdec";
constexpr const char* JNI_TAG = "lcevc_dec_jni";

constexpr const char* kColorParamsClassName =
    "com/vnova/lcevc/decoder/dec/LcevcNativeAdapter$ColorParams";
constexpr const char* kDecoderInfoClassName =
    "com/vnova/lcevc/decoder/dec/LcevcNativeAdapter$DecodeInformation";

#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, JNI_TAG, __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, JNI_TAG, __VA_ARGS__))
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, JNI_TAG, __VA_ARGS__))
#define LOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, JNI_TAG, __VA_ARGS__))
#define LOGV(...) ((void)__android_log_print(ANDROID_LOG_VERBOSE, JNI_TAG, __VA_ARGS__))

#define NATIVE_FUNC(RETURN_TYPE, NAME, ...)                                                      \
    extern "C" JNIEXPORT RETURN_TYPE Java_com_vnova_lcevc_decoder_dec_LcevcNativeAdapter_##NAME( \
        JNIEnv* env, jobject thiz, ##__VA_ARGS__)

constexpr android_LogPriority logLevelCharToAndroidLogLevel(char level)
{
    switch (level) {
        case '1': return ANDROID_LOG_FATAL;
        case '2': return ANDROID_LOG_ERROR;
        case '3': return ANDROID_LOG_WARN;
        case '4': return ANDROID_LOG_INFO;
        case '5': return ANDROID_LOG_DEBUG;
        case '6': return ANDROID_LOG_VERBOSE;
        default: return ANDROID_LOG_UNKNOWN;
    }
}

void logEventCallback(LCEVC_DecoderHandle, LCEVC_Event event, LCEVC_PictureHandle,
                      const LCEVC_DecodeInformation*, const uint8_t* data, uint32_t dataSize, void*)
{
    if (event != LCEVC_Log) {
        return;
    }
    const std::string_view messageWithLevel(reinterpret_cast<const char*>(data), dataSize);
    const android_LogPriority level = logLevelCharToAndroidLogLevel(messageWithLevel.at(0));
    const std::string_view message = messageWithLevel.substr(2);
    __android_log_print(level, DEC_TAG, "%s", message.data());
}

bool checkJniException(JNIEnv* env)
{
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe(); // writes to logcat
        env->ExceptionClear();
        return true;
    }
    return false;
}

static void setIntField(JNIEnv* env, jobject object, jclass cls, const char* name, jint value)
{
    const jfieldID fieldId = env->GetFieldID(cls, name, "I");
    env->SetIntField(object, fieldId, value);
}

static void setFloatField(JNIEnv* env, jobject object, jclass cls, const char* name, jfloat value)
{
    const jfieldID fieldId = env->GetFieldID(cls, name, "F");
    env->SetFloatField(object, fieldId, value);
}

static void getIntField(JNIEnv* env, jobject object, jclass cls, const char* name, uint32_t& value)
{
    const jfieldID fieldId = env->GetFieldID(cls, name, "I");
    value = static_cast<uint32_t>(env->GetIntField(object, fieldId));
}

static void getFloatField(JNIEnv* env, jobject object, jclass cls, const char* name, float& value)
{
    const jfieldID fieldId = env->GetFieldID(cls, name, "F");
    value = static_cast<float>(env->GetFloatField(object, fieldId));
}

static uint32_t greatestCommonDivisor(uint32_t a, uint32_t b)
{
    while (b != 0) {
        uint32_t tmp = a % b;
        a = b;
        b = tmp;
    }
    return a == 0 ? 1 : a;
}

static void setSampleAspectRatioFromPixelAspectRatio(float pixelAspectRatio, uint32_t& num, uint32_t& den)
{
    if (pixelAspectRatio <= 0.0f) {
        num = 1;
        den = 1;
        return;
    }

    constexpr uint32_t kSarScale = 1000000;
    const uint64_t scaled = static_cast<uint64_t>(std::llround(pixelAspectRatio * kSarScale));
    num = static_cast<uint32_t>(std::min<uint64_t>(scaled, std::numeric_limits<uint32_t>::max()));
    den = kSarScale;

    const uint32_t gcd = greatestCommonDivisor(num, den);
    num /= gcd;
    den /= gcd;
}

// Client context for decoder instance and callback metadata
struct ClientContext
{
    LCEVC_DecoderHandle decoder = {0};
    jobject client = nullptr;
    ANativeWindow* nativeWindow = nullptr;
};

// save Java VM context
static jobject gClassLoader = nullptr;
static jmethodID gFindClassMethod = nullptr;
static jclass gDecoderInfoClass = nullptr;
static jobject gDecoderInfoObject = nullptr;
static jstring gDecoderInfoClassNameJS = nullptr;

static std::mutex gPictureMapMutex;
static std::unordered_map<uintptr_t, LCEVC_DecoderHandle> gPictureDecoderMap;

// we need this to be called in the JNI_Onload function
// because we need to create a class, on a different thread and FindClass only
// seems to work on the main Java thread, not a native thread??? by caching the
// findClass method from the main thread we can call it on any thread :D
static void getClassLoader(JNIEnv* env, const char* className)
{
    auto randomClass = env->FindClass(className);
    jclass classClass = env->GetObjectClass(randomClass);
    auto classLoaderClass = env->FindClass("java/lang/ClassLoader");
    auto getClassLoaderMethod =
        env->GetMethodID(classClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
    gClassLoader = env->NewGlobalRef(env->CallObjectMethod(randomClass, getClassLoaderMethod));
    gFindClassMethod =
        env->GetMethodID(classLoaderClass, "findClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    checkJniException(env);
}

static jclass findClass(JNIEnv* env, jstring name)
{
    if (gFindClassMethod == nullptr)
        return nullptr;
    return static_cast<jclass>(env->CallObjectMethod(gClassLoader, gFindClassMethod, name));
}

static bool setJavaDecodeInformation(JNIEnv* env, jclass cls, jobject jInfo,
                                     LCEVC_DecoderHandle decoder, jlong timestamp)
{
    const uint64_t timestampU64 = static_cast<uint64_t>(timestamp);

    LCEVC_PictureDesc desc = {};
    LCEVC_ReturnCode peekPictureDescRet = LCEVC_PeekDecoderPictureDesc(decoder, timestampU64, &desc);
    if (peekPictureDescRet != LCEVC_Success) {
        LOGE("setJavaDecodeInformation: LCEVC_PeekDecoderPictureDesc failed with %d", peekPictureDescRet);
        return false;
    }
    LOGD("setJavaDecodeInformation: LCEVC_PeekDecoderPictureDesc reported width=%u height=%u "
         "sarNum=%u sarDen=%u",
         desc.width, desc.height, desc.sampleAspectRatioNum, desc.sampleAspectRatioDen);

    setIntField(env, jInfo, cls, "width", static_cast<jint>(desc.width));
    setIntField(env, jInfo, cls, "height", static_cast<jint>(desc.height));
    setFloatField(env, jInfo, cls, "sarNum", static_cast<float>(desc.sampleAspectRatioNum));
    setFloatField(env, jInfo, cls, "sarDen", static_cast<float>(desc.sampleAspectRatioDen));

    return true;
}

static bool createJavaDecodeInformation(JNIEnv* env, jclass& decodeClass, jobject& decodeObject)
{
    decodeClass = findClass(env, gDecoderInfoClassNameJS);
    if (decodeClass != nullptr) {
        jmethodID constr = env->GetMethodID(decodeClass, "<init>", "()V");
        if (constr != nullptr) {
            decodeObject = env->NewObject(decodeClass, constr);
            if (decodeObject == nullptr) {
                LOGE("createJavaDecodeInformation: Failed NewObject");
            }
        } else {
            LOGE("createJavaDecodeInformation: Failed GetMethodID for init");
        }
    } else {
        LOGE("createJavaDecodeInformation: Failed FindClass");
    }
    checkJniException(env);
    return decodeObject != nullptr;
}

static int sdkVersion()
{
    char version[PROP_VALUE_MAX + 1];
    __system_property_get("ro.build.version.sdk", version);
    return atoi(version);
}

static uint32_t formatByteSize(LCEVC_ColorFormat format)
{
    switch (format) {
        case LCEVC_I420_10_LE:
        case LCEVC_I420_12_LE:
        case LCEVC_I420_14_LE:
        case LCEVC_I420_16_LE:
        case LCEVC_I422_10_LE:
        case LCEVC_I422_12_LE:
        case LCEVC_I422_14_LE:
        case LCEVC_I422_16_LE:
        case LCEVC_I444_10_LE:
        case LCEVC_I444_12_LE:
        case LCEVC_I444_14_LE:
        case LCEVC_I444_16_LE:
        case LCEVC_GRAY_10_LE:
        case LCEVC_GRAY_12_LE:
        case LCEVC_GRAY_14_LE:
        case LCEVC_GRAY_16_LE: return 2;
        default: return 1;
    }
}

static uint32_t formatPlaneCount(LCEVC_ColorFormat format)
{
    switch (format) {
        case LCEVC_NV12_8:
        case LCEVC_NV21_8: return 2;
        case LCEVC_GRAY_8:
        case LCEVC_GRAY_10_LE:
        case LCEVC_GRAY_12_LE:
        case LCEVC_GRAY_14_LE:
        case LCEVC_GRAY_16_LE: return 1;
        default: return 3;
    }
}

static bool validatePlaneFormat(LCEVC_ColorFormat format, uint32_t width, uint32_t height,
                                uint32_t plane, uint32_t& planeWidth, uint32_t& planeHeight)
{
    const uint32_t halfWidth = width / 2;
    const uint32_t halfHeight = height / 2;

    switch (format) {
        case LCEVC_NV12_8:
        case LCEVC_NV21_8:
            if (plane == 0) {
                planeWidth = width;
                planeHeight = height;
                return true;
            }
            if (plane == 1) {
                planeWidth = width;
                planeHeight = halfHeight;
                return true;
            }
            return false;
        case LCEVC_I420_8:
        case LCEVC_I420_10_LE:
        case LCEVC_I420_12_LE:
        case LCEVC_I420_14_LE:
        case LCEVC_I420_16_LE:
            if (plane == 0) {
                planeWidth = width;
                planeHeight = height;
                return true;
            }
            if (plane < 3) {
                planeWidth = halfWidth;
                planeHeight = halfHeight;
                return true;
            }
            return false;
        case LCEVC_I422_8:
        case LCEVC_I422_10_LE:
        case LCEVC_I422_12_LE:
        case LCEVC_I422_14_LE:
        case LCEVC_I422_16_LE:
            if (plane == 0) {
                planeWidth = width;
                planeHeight = height;
                return true;
            }
            if (plane < 3) {
                planeWidth = halfWidth;
                planeHeight = height;
                return true;
            }
            return false;
        case LCEVC_I444_8:
        case LCEVC_I444_10_LE:
        case LCEVC_I444_12_LE:
        case LCEVC_I444_14_LE:
        case LCEVC_I444_16_LE:
            if (plane < 3) {
                planeWidth = width;
                planeHeight = height;
                return true;
            }
            return false;
        case LCEVC_GRAY_8:
        case LCEVC_GRAY_10_LE:
        case LCEVC_GRAY_12_LE:
        case LCEVC_GRAY_14_LE:
        case LCEVC_GRAY_16_LE:
            if (plane == 0) {
                planeWidth = width;
                planeHeight = height;
                return true;
            }
            return false;
        default: return false;
    }
}

inline int64_t getPtsFromUniqueTimestamp(const uint64_t timestamp)
{
    const int64_t encodedPts = static_cast<int64_t>(timestamp & 0x0000FFFFFFFFFFFFULL);
    return encodedPts - (1LL << 47);
}

void notifyPictureReleased(JNIEnv* env, jlong instance, ClientContext* context,
                           LCEVC_ReturnCode result, uint64_t timestamp, LCEVC_PictureHandle picture)
{
    jobject client = env->NewLocalRef(context->client);
    if (client == nullptr) {
        return;
    }

    jclass cls = env->GetObjectClass(client);
    jmethodID methodID = env->GetMethodID(cls, "onPictureReleased", "(JIJJJ)V");
    if (methodID != nullptr) {
        env->CallVoidMethod(client, methodID, static_cast<jlong>(instance), static_cast<jint>(result),
                            static_cast<jlong>(timestamp), static_cast<jlong>(picture.hdl),
                            static_cast<jlong>(getPtsFromUniqueTimestamp(timestamp)));
    } else {
        LOGE("notifyPictureReleased: failed to find onPictureReleased");
    }
    env->DeleteLocalRef(client);
    checkJniException(env);
}

LCEVC_ReturnCode receiveDecoderPictureAndNotify(JNIEnv* env, jlong instance, ClientContext* context,
                                                uint64_t fallbackTimestamp,
                                                LCEVC_PictureHandle expectedPicture, const char* operation)
{
    LCEVC_PictureHandle decodedPicture{};
    LCEVC_DecodeInformation decodeInformation{};
    const LCEVC_ReturnCode ret =
        LCEVC_ReceiveDecoderPicture(context->decoder, &decodedPicture, &decodeInformation);
    if (ret != LCEVC_Success && ret != LCEVC_Flushed) {
        return ret;
    }

    if (expectedPicture.hdl != 0 && decodedPicture.hdl != expectedPicture.hdl) {
        LOGW("receiveDecoderPictureAndNotify: %s received picture %lu, expected %lu", operation,
             static_cast<unsigned long>(decodedPicture.hdl),
             static_cast<unsigned long>(expectedPicture.hdl));
    }

    const uint64_t callbackTimestamp =
        decodeInformation.timestamp != 0 ? decodeInformation.timestamp : fallbackTimestamp;
    LOGD("receiveDecoderPictureAndNotify: %s received decoder picture %lu", operation,
         static_cast<unsigned long>(decodedPicture.hdl));
    notifyPictureReleased(env, instance, context, ret, callbackTimestamp, decodedPicture);
    return ret;
}

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env;
    LOGD("JNI_OnLoad: starting VM: %p", vm);
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return -1;
    }

    gDecoderInfoClassNameJS =
        reinterpret_cast<jstring>(env->NewGlobalRef(env->NewStringUTF(kDecoderInfoClassName)));
    getClassLoader(env, kDecoderInfoClassName); // setup the global class loader

    createJavaDecodeInformation(env, gDecoderInfoClass, gDecoderInfoObject);
    gDecoderInfoClass = reinterpret_cast<jclass>(env->NewGlobalRef(gDecoderInfoClass));
    gDecoderInfoObject = reinterpret_cast<jobject>(env->NewGlobalRef(gDecoderInfoObject));
    LOGD("JNI_OnLoad: running API level %d, decoder info c lass %p object %p", sdkVersion(),
         gDecoderInfoClass, gDecoderInfoObject);

    return JNI_VERSION_1_6;
}

NATIVE_FUNC(jlong, lcevcCreateDecoder)
{
    LOGV("lcevcCreateDecoder: entering");

    ClientContext* context = new ClientContext(); // Zero initialised

    context->client = reinterpret_cast<jobject>(env->NewWeakGlobalRef(thiz));
    context->nativeWindow = nullptr;

    LCEVC_ReturnCode ret = LCEVC_CreateDecoder(&context->decoder, LCEVC_AccelContextHandle{});
    LCEVC_ConfigureDecoderInt(context->decoder, "threads", 2);
    LCEVC_ConfigureDecoderInt(context->decoder, "passthrough_mode", 2);
    LCEVC_ConfigureDecoderString(context->decoder, "pipeline", "vulkan");
    LCEVC_ConfigureDecoderBool(context->decoder, "will_render", true);

    // logcat logging via event callback
    constexpr std::array<int32_t, 1> enabledEvents = {LCEVC_Log};
    LCEVC_ConfigureDecoderIntArray(context->decoder, "events", enabledEvents.size(), enabledEvents.data());
    LCEVC_SetDecoderEventCallback(context->decoder, logEventCallback, nullptr);

    if (ret == LCEVC_Success) {
        ret = LCEVC_InitializeDecoder(context->decoder);
    }

    if (ret == LCEVC_Success) {
        LOGD("lcevcCreateDecoder: Created decoder context %p", context);
        LOGI("lcevcCreateDecoder: Using LCEVCdec version %d.%d.%d", LCEVC_DEC_VERSION_MAJOR,
             LCEVC_DEC_VERSION_MINOR, LCEVC_DEC_VERSION_PATCH);
        return reinterpret_cast<jlong>(context);
    }

    return 0;
}

NATIVE_FUNC(jint, lcevcDestroyDecoder, jlong instance)
{
    LOGV("lcevcDestroyDecoder: entering");

    ClientContext* context = reinterpret_cast<ClientContext*>(instance);
    if (context == nullptr) {
        return LCEVC_InvalidParam;
    }

    {
        std::lock_guard<std::mutex> lock(gPictureMapMutex);
        for (auto it = gPictureDecoderMap.begin(); it != gPictureDecoderMap.end();) {
            if (it->second.hdl == context->decoder.hdl) {
                it = gPictureDecoderMap.erase(it);
            } else {
                ++it;
            }
        }
    }

    if (context->nativeWindow != nullptr) {
        ANativeWindow_release(context->nativeWindow);
        context->nativeWindow = nullptr;
    }

    if (context->client != nullptr) {
        env->DeleteWeakGlobalRef(context->client);
        context->client = nullptr;
    }

    if (context->decoder.hdl != 0) {
        LCEVC_DestroyDecoder(context->decoder);
        context->decoder = {};
    }

    delete context;
    return LCEVC_Success;
}

NATIVE_FUNC(jint, lcevcSendDecoderBase, jlong instance, jlong timeStamp, jlong inputPictureHandle,
            jlong outputPictureHandle)
{
    // Always wait for send call to finish rather than letting it timeout.
    constexpr uint32_t kSendBaseTimeout = std::numeric_limits<uint32_t>::max();

    LOGV("lcevcSendDecoderBase: entering");

    ClientContext* context = reinterpret_cast<ClientContext*>(instance);
    if (context == nullptr) {
        return LCEVC_InvalidParam;
    }
    LCEVC_PictureHandle const inputPicture = {static_cast<uintptr_t>(inputPictureHandle)};
    LCEVC_PictureHandle const outputPicture = {static_cast<uintptr_t>(outputPictureHandle)};

    LOGD("lcevcSendDecoderBase: timeStamp %" PRId64 " input %lu, output %lu", timeStamp,
         static_cast<unsigned long>(inputPicture.hdl), static_cast<unsigned long>(outputPicture.hdl));
    LCEVC_ReturnCode ret =
        LCEVC_SendDecoderBase(context->decoder, timeStamp, inputPicture, kSendBaseTimeout, nullptr);
    if (ret != LCEVC_Success) {
        LOGE("lcevcSendDecoderBase: LCEVC_SendDecoderBase returned %d", ret);
        return ret;
    }

    ret = LCEVC_SendDecoderPicture(context->decoder, outputPicture);
    if (ret != LCEVC_Success) {
        LOGE("lcevcSendDecoderBase: LCEVC_SendDecoderPicture returned %d", ret);
        return ret;
    }

    jobject client = env->NewLocalRef(context->client);
    if (client != nullptr) {
        jclass cls = env->GetObjectClass(client);
        std::string methodSig = std::string("(JIJ") + "L" + kDecoderInfoClassName + ";)V";
        jmethodID methodID = env->GetMethodID(cls, "onDecodeCompleted", methodSig.c_str());
        if (methodID != nullptr) {
            if (setJavaDecodeInformation(env, gDecoderInfoClass, gDecoderInfoObject,
                                         context->decoder, timeStamp)) {
                env->CallVoidMethod(client, methodID, static_cast<jlong>(instance),
                                    static_cast<jint>(LCEVC_Success), timeStamp, gDecoderInfoObject);
            } else {
                LOGE("lcevcSendDecoderBase: failed to set decode information");
            }
        } else {
            LOGE("lcevcSendDecoderBase: failed to find onDecodeCompleted");
        }
        env->DeleteLocalRef(client);
    }

    LCEVC_PictureHandle doneBasePicture;

    if (const int result = LCEVC_ReceiveDecoderBase(context->decoder, &doneBasePicture);
        result != LCEVC_Success) {
        LOGW("lcevcSendDecoderBase: LCEVC_ReceiveDecoderBase returned %d", result);
    } else {
        LOGD("lcevcSendDecoderBase: LCEVC_ReceiveDecoderBase returned success");
    }

    return 0;
}

NATIVE_FUNC(jint, lcevcRenderSendPicture, jlong instance, jlong timeStamp, jlong pictureHandle,
            jobject renderInfo, jlong delayUs)
{
    LOGV("lcevcRenderSendPicture: entering");

    ClientContext* context = reinterpret_cast<ClientContext*>(instance);
    if (context == nullptr) {
        return LCEVC_InvalidParam;
    }

    LCEVC_RenderSendInformation info{};
    uint32_t rotation = 0;
    if (renderInfo != nullptr) {
        jclass renderInfoClass = env->GetObjectClass(renderInfo);
        if (renderInfoClass != nullptr) {
            getIntField(env, renderInfo, renderInfoClass, "rotation", rotation);
        }
    }
    info.rotation = rotation;
    uint64_t delay = static_cast<uint64_t>(delayUs);
    LCEVC_PictureHandle picture = {static_cast<uintptr_t>(pictureHandle)};
    LCEVC_ReturnCode ret = LCEVC_RenderSendPicture(context->decoder, timeStamp, picture, &info, delay);
    if (ret != LCEVC_Success) {
        LOGE("lcevcRenderSendPicture: LCEVC_RenderSendPicture returned %d", ret);
        return ret;
    }

    LCEVC_PictureHandle decodedPicture{};
    LCEVC_DecodeInformation decodeInformation{};
    ret = LCEVC_ReceiveDecoderPicture(context->decoder, &decodedPicture, &decodeInformation);
    if (ret != LCEVC_Success && ret != LCEVC_Flushed) {
        LOGE("lcevcRenderSendPicture: LCEVC_ReceiveDecoderPicture after render returned %d", ret);
        return ret;
    }
    LOGD("lcevcRenderSendPicture: LCEVC_ReceiveDecoderPicture received decoder picture %lu",
         static_cast<unsigned long>(decodedPicture.hdl));
    const uint64_t callbackTimestamp = decodeInformation.timestamp != 0
                                           ? decodeInformation.timestamp
                                           : static_cast<uint64_t>(timeStamp);

    jobject client = env->NewLocalRef(context->client);
    if (client != nullptr) {
        jclass cls = env->GetObjectClass(client);
        jmethodID methodID = env->GetMethodID(cls, "onRenderCompleted", "(JIJJJ)V");
        if (methodID != nullptr) {
            env->CallVoidMethod(client, methodID, static_cast<jlong>(instance),
                                static_cast<jint>(ret), static_cast<jlong>(callbackTimestamp),
                                static_cast<jlong>(decodedPicture.hdl),
                                static_cast<jlong>(getPtsFromUniqueTimestamp(callbackTimestamp)));
        } else {
            LOGE("lcevcRenderSendPicture: failed to find onRenderCompleted");
        }
        env->DeleteLocalRef(client);
    }
    checkJniException(env);

    return ret;
}

NATIVE_FUNC(jint, lcevcReleaseDecoderPicture, jlong instance, jlong timeStamp, jlong pictureHandle)
{
    LOGV("lcevcReleaseDecoderPicture: entering");

    ClientContext* context = reinterpret_cast<ClientContext*>(instance);
    if (context == nullptr) {
        return LCEVC_InvalidParam;
    }

    LCEVC_PictureHandle picture = {static_cast<uintptr_t>(pictureHandle)};
    const LCEVC_ReturnCode ret =
        receiveDecoderPictureAndNotify(env, instance, context, static_cast<uint64_t>(timeStamp),
                                       picture, "LCEVC_ReceiveDecoderPicture");
    if (ret != LCEVC_Success && ret != LCEVC_Flushed) {
        LOGE("lcevcReleaseDecoderPicture: LCEVC_ReceiveDecoderPicture returned %d", ret);
    }

    return ret;
}

NATIVE_FUNC(jint, lcevcSendDecoderEnhancementData, jlong instance, jlong timestamp,
            jboolean isKeyFrame, jobject inputData, jint syntaxType, jboolean stripLcevc)
{
    if (instance == 0 || inputData == nullptr) {
        return LCEVC_InvalidParam;
    }
    ClientContext* context = reinterpret_cast<ClientContext*>(instance);
    jclass cls = env->GetObjectClass(inputData);
    // Method id for inputData get position
    jmethodID methodId = env->GetMethodID(cls, "position", "()I");
    uint32_t dataOffset = env->CallIntMethod(inputData, methodId);
    uint8_t* data = reinterpret_cast<uint8_t*>(env->GetDirectBufferAddress(inputData));
    data += dataOffset;
    // Method id for inputData get limit
    methodId = env->GetMethodID(cls, "limit", "()I");
    uint32_t dataLimit = env->CallIntMethod(inputData, methodId);
    uint32_t dataSize = dataLimit - dataOffset;

    std::vector<uint8_t> enhancementData(dataSize);
    if (stripLcevc) {
        uint32_t actualSize = 0;
        int32_t const r = LCEVC_extractEnhancementFromNAL(
            data, dataSize, LCEVC_NALFormat_AnnexB, static_cast<LCEVC_CodecType>(syntaxType),
            enhancementData.data(), static_cast<uint32_t>(enhancementData.size()), &actualSize);
        enhancementData.resize(actualSize);
        if (r < 0) {
            LOGW("lcevcSendDecoderEnhancementData: Couldn't find LCEVC data in input data (%d "
                 "bytes)",
                 dataSize);
            return r;
        }
        LOGD("lcevcSendDecoderEnhancementData: Extracted %d LCEVC bytes from NALU (%d bytes)",
             actualSize, dataSize);
    }

    LCEVC_ReturnCode const ret = LCEVC_SendDecoderEnhancementData(
        context->decoder, timestamp, enhancementData.data(), enhancementData.size());
    if (ret != LCEVC_Success) {
        LOGE("lcevcSendDecoderEnhancementData: LCEVC_SendDecoderEnhancementData returned %d", ret);
        return ret;
    }

    LOGV("lcevcSendDecoderEnhancementData: entering");
    return LCEVC_Success;
}

NATIVE_FUNC(jint, lcevcFlushDecoder, jlong instance)
{
    LOGV("lcevcFlushDecoder: entering");

    ClientContext* context = reinterpret_cast<ClientContext*>(instance);
    if (context == nullptr) {
        return LCEVC_InvalidParam;
    }

    const LCEVC_ReturnCode ret = LCEVC_FlushDecoder(context->decoder);
    if (ret != LCEVC_Success) {
        return ret;
    }

    for (;;) {
        const LCEVC_ReturnCode receiveRet =
            receiveDecoderPictureAndNotify(env, instance, context, 0, {}, "LCEVC_ReceiveDecoderPicture");
        if (receiveRet == LCEVC_Again) {
            break;
        }
        if (receiveRet != LCEVC_Success && receiveRet != LCEVC_Flushed) {
            LOGW("lcevcFlushDecoder: LCEVC_ReceiveDecoderPicture after flush returned %d", receiveRet);
            break;
        }
    }

    LCEVC_PictureHandle picture{};
    for (;;) {
        const LCEVC_ReturnCode receiveRet = LCEVC_ReceiveDecoderBase(context->decoder, &picture);
        if (receiveRet == LCEVC_Again) {
            break;
        }
        if (receiveRet != LCEVC_Success && receiveRet != LCEVC_Flushed) {
            LOGW("lcevcFlushDecoder: LCEVC_ReceiveDecoderBase after flush returned %d", receiveRet);
            break;
        }
        LOGD("lcevcFlushDecoder: LCEVC_ReceiveDecoderBase drained flushed decoder base %lu",
             static_cast<unsigned long>(picture.hdl));
    }

    return LCEVC_Success;
}

NATIVE_FUNC(jint, lcevcSynchronizeDecoder, jlong instance, jboolean dropPending)
{
    LOGV("lcevcSynchronizeDecoder: entering");

    ClientContext* context = reinterpret_cast<ClientContext*>(instance);
    if (context == nullptr) {
        return LCEVC_InvalidParam;
    }

    const LCEVC_ReturnCode ret =
        LCEVC_SynchronizeDecoder(context->decoder, static_cast<bool>(dropPending));

    return ret;
}

NATIVE_FUNC(jint, lcevcRenderSetWindow, jlong instance, jobject surface, jboolean secure)
{
    LOGV("lcevcRenderSetWindow: entering");
    ClientContext* context = reinterpret_cast<ClientContext*>(instance);
    if (context == nullptr) {
        return LCEVC_InvalidParam;
    }
    if (context->decoder.hdl == 0) {
        LOGE("lcevcRenderSetWindow: called with decoder with invalid handle");
        return LCEVC_InvalidParam;
    }
    if (surface == nullptr) {
        LOGW("lcevcRenderSetWindow: called with null surface");
        return LCEVC_InvalidParam;
    }

    ANativeWindow* nativeWindow = ANativeWindow_fromSurface(env, surface);
    if (nativeWindow == nullptr) {
        LOGW("lcevcRenderSetWindow: ANativeWindow_fromSurface returned null");
        return LCEVC_InvalidParam;
    }

    const int width = ANativeWindow_getWidth(nativeWindow);
    const int height = ANativeWindow_getHeight(nativeWindow);
    const int format = ANativeWindow_getFormat(nativeWindow);
    LOGI("lcevcRenderSetWindow: ANativeWindow %p size=%dx%d format=%d", nativeWindow, width, height, format);

    if (width <= 0 || height <= 0) {
        LOGI("lcevcRenderSetWindow: Surface not ready yet (size=%dx%d), skipping RenderSetWindow",
             width, height);
        ANativeWindow_release(nativeWindow);
        return LCEVC_Again;
    }

    ANativeWindow* oldWindow = context->nativeWindow;
    context->nativeWindow = nativeWindow;
    const LCEVC_ReturnCode ret = LCEVC_RenderSetWindow(context->decoder, nativeWindow, secure);
    if (oldWindow) {
        ANativeWindow_release(oldWindow);
    }
    if (ret != LCEVC_Success) {
        LOGW("lcevcRenderSetWindow: LCEVC_RenderSetWindow returned %d", ret);
    }
    return ret;
}

NATIVE_FUNC(jlong, lcevcAllocPicture, jlong instance, jobject pictureDescObj)
{
    LOGV("lcevcAllocPicture: entering");

    ClientContext* context = reinterpret_cast<ClientContext*>(instance);
    if (context == nullptr) {
        return LCEVC_InvalidParam;
    }
    if (pictureDescObj == nullptr) {
        return LCEVC_InvalidParam;
    }
    LCEVC_PictureDesc desc = {0};
    jclass jPictureDesc = env->GetObjectClass(pictureDescObj);

    jobject objectField;
    uint32_t objectValue;

    LCEVC_ColorFormat format = LCEVC_ColorFormat_Unknown;
    uint32_t width = 0;
    uint32_t height = 0;
    float pixelAspectRatio = 1.0f;

    getIntField(env, pictureDescObj, jPictureDesc, "colorFormat", objectValue);
    format = static_cast<LCEVC_ColorFormat>(objectValue);
    getIntField(env, pictureDescObj, jPictureDesc, "width", width);
    getIntField(env, pictureDescObj, jPictureDesc, "height", height);
    getFloatField(env, pictureDescObj, jPictureDesc, "pixelAspectRatio", pixelAspectRatio);

    LOGD("lcevcAllocPicture: LCEVC_DefaultPictureDesc request %dx%d, format %d", width, height, format);
    if (LCEVC_DefaultPictureDesc(&desc, format, width, height) != LCEVC_Success) {
        LOGE("lcevcAllocPicture: LCEVC_DefaultPictureDesc failed");
        return -1;
    }

    getIntField(env, pictureDescObj, jPictureDesc, "sampleAspectRatioNum", desc.sampleAspectRatioNum);
    getIntField(env, pictureDescObj, jPictureDesc, "sampleAspectRatioDen", desc.sampleAspectRatioDen);
    if (desc.sampleAspectRatioNum == 0 && desc.sampleAspectRatioDen == 0) {
        setSampleAspectRatioFromPixelAspectRatio(pixelAspectRatio, desc.sampleAspectRatioNum,
                                                 desc.sampleAspectRatioDen);
    } else {
        if (desc.sampleAspectRatioNum == 0) {
            desc.sampleAspectRatioNum = 1;
        }
        if (desc.sampleAspectRatioDen == 0) {
            desc.sampleAspectRatioDen = 1;
        }
    }

    const std::string colorParamsFieldName = std::string("L") + kColorParamsClassName + ";";
    jfieldID fieldId = env->GetFieldID(jPictureDesc, "colorParams", colorParamsFieldName.c_str());
    objectField = env->GetObjectField(pictureDescObj, fieldId);
    LOGD("lcevcAllocPicture: fieldId %p object %p for <%s>", fieldId, objectField,
         colorParamsFieldName.c_str());
    if (objectField != nullptr) {
        jPictureDesc = env->GetObjectClass(objectField);
        uint32_t value = 0;

        getIntField(env, objectField, jPictureDesc, "colorRange", value);
        desc.colorRange = static_cast<LCEVC_ColorRange>(value);
        getIntField(env, objectField, jPictureDesc, "colorPrimaries", value);
        desc.colorPrimaries = static_cast<LCEVC_ColorPrimaries>(value);
        getIntField(env, objectField, jPictureDesc, "transferCharacteristics", value);
        desc.transferCharacteristics = static_cast<LCEVC_TransferCharacteristics>(value);
        getIntField(env, objectField, jPictureDesc, "matrixCoefficients", value);
        desc.matrixCoefficients = static_cast<LCEVC_MatrixCoefficients>(value);

        fieldId = env->GetFieldID(jPictureDesc, "hdrStaticInfo", "[B");
        jbyteArray hdrInfoField = static_cast<jbyteArray>(env->GetObjectField(objectField, fieldId));
        if (hdrInfoField != nullptr) {
            env->GetByteArrayRegion(hdrInfoField, 0, sizeof(LCEVC_HDRStaticInfo),
                                    reinterpret_cast<jbyte*>(&desc.hdrStaticInfo));
        }
    }

    LOGV("lcevcAllocPicture: picture format: %d width: %d height %d", format, width, height);

    LCEVC_PictureHandle handle = {};
    if (LCEVC_AllocPicture(context->decoder, &desc, &handle) == LCEVC_Success) {
        {
            std::lock_guard<std::mutex> const lock(gPictureMapMutex);
            gPictureDecoderMap[handle.hdl] = context->decoder;
        }
        LOGD("lcevcAllocPicture: LCEVC_AllocPicture allocated picture %lu (%p)",
             static_cast<unsigned long>(handle.hdl), &handle);
        return handle.hdl;
    }

    LOGE("lcevcAllocPicture: LCEVC_AllocPicture failed");
    return -1;
}

NATIVE_FUNC(jint, lcevcFreePicture, jlong instance, jlong pictureHandle)
{
    LOGV("lcevcFreePicture: entering");
    ClientContext* context = reinterpret_cast<ClientContext*>(instance);
    if (context == nullptr || pictureHandle == 0) {
        return LCEVC_InvalidParam;
    }
    LCEVC_PictureHandle const pictureHdl = {static_cast<uintptr_t>(pictureHandle)};
    {
        std::lock_guard<std::mutex> const lock(gPictureMapMutex);
        gPictureDecoderMap.erase(pictureHdl.hdl);
    }
    return LCEVC_FreePicture(context->decoder, pictureHdl);
}

// Special more efficient function to set all the planes for a picture buffer in a single JNI call
NATIVE_FUNC(jint, lcevcPictureSetPlaneBuffers, jlong pictureHandle, jobject inputBuffer,
            jintArray inputOffsets, jintArray inputByteStrides)
{
    LOGV("lcevcPictureSetPlaneBuffers: entering");
    if (pictureHandle == 0 || inputBuffer == nullptr || inputOffsets == nullptr ||
        inputByteStrides == nullptr) {
        return LCEVC_InvalidParam;
    }

    LCEVC_PictureHandle pictureHdl = {static_cast<uintptr_t>(pictureHandle)};
    LCEVC_DecoderHandle decoderHdl = {};
    {
        std::lock_guard<std::mutex> lock(gPictureMapMutex);
        auto it = gPictureDecoderMap.find(pictureHdl.hdl);
        if (it == gPictureDecoderMap.end()) {
            LOGE("lcevcPictureSetPlaneBuffers: unknown picture handle %lu",
                 static_cast<unsigned long>(pictureHdl.hdl));
            return LCEVC_InvalidParam;
        }
        decoderHdl = it->second;
    }

    LCEVC_PictureDesc desc = {};
    if (LCEVC_GetPictureDesc(decoderHdl, pictureHdl, &desc) != LCEVC_Success) {
        LOGE("lcevcPictureSetPlaneBuffers: LCEVC_GetPictureDesc failed for picture %lu",
             static_cast<unsigned long>(pictureHdl.hdl));
        return LCEVC_Error;
    }

    const int32_t numPlanes = env->GetArrayLength(inputOffsets);
    const int32_t numStrides = env->GetArrayLength(inputByteStrides);
    if (numPlanes != numStrides) {
        LOGE("lcevcPictureSetPlaneBuffers: offsets/strides length mismatch %d vs %d", numPlanes, numStrides);
        return LCEVC_InvalidParam;
    }

    const uint32_t expectedPlanes = formatPlaneCount(desc.colorFormat);
    if (static_cast<uint32_t>(numPlanes) < expectedPlanes) {
        LOGE("lcevcPictureSetPlaneBuffers: expected %u planes, got %d", expectedPlanes, numPlanes);
        return LCEVC_InvalidParam;
    }

    auto* buffer = reinterpret_cast<uint8_t*>(env->GetDirectBufferAddress(inputBuffer));
    if (buffer == nullptr) {
        LOGE("lcevcPictureSetPlaneBuffers: direct buffer address is null");
        return LCEVC_InvalidParam;
    }

    jint* offsets = env->GetIntArrayElements(inputOffsets, nullptr);
    jint* strides = env->GetIntArrayElements(inputByteStrides, nullptr);
    if (offsets == nullptr || strides == nullptr) {
        if (offsets != nullptr) {
            env->ReleaseIntArrayElements(inputOffsets, offsets, 0);
        }
        if (strides != nullptr) {
            env->ReleaseIntArrayElements(inputByteStrides, strides, 0);
        }
        LOGE("lcevcPictureSetPlaneBuffers: failed to read offsets/strides");
        return LCEVC_InvalidParam;
    }

    LCEVC_PictureLockHandle lock = {};
    if (LCEVC_LockPicture(decoderHdl, pictureHdl, LCEVC_Access_Write, &lock) != LCEVC_Success) {
        env->ReleaseIntArrayElements(inputOffsets, offsets, 0);
        env->ReleaseIntArrayElements(inputByteStrides, strides, 0);
        LOGE("lcevcPictureSetPlaneBuffers: LCEVC_LockPicture failed for picture %lu",
             static_cast<unsigned long>(pictureHdl.hdl));
        return LCEVC_Error;
    }

    const uint32_t bytesPerSample = formatByteSize(desc.colorFormat);
    for (uint32_t planeIndex = 0; planeIndex < expectedPlanes; ++planeIndex) {
        LCEVC_PicturePlaneDesc plane = {};
        if (LCEVC_GetPictureLockPlaneDesc(decoderHdl, lock, planeIndex, &plane) != LCEVC_Success) {
            LOGE("lcevcPictureSetPlaneBuffers: LCEVC_GetPictureLockPlaneDesc failed for plane %u "
                 "of %lu",
                 planeIndex, static_cast<unsigned long>(pictureHdl.hdl));
            continue;
        }

        uint32_t planeWidth = 0;
        uint32_t planeHeight = 0;
        if (!validatePlaneFormat(desc.colorFormat, desc.width, desc.height, planeIndex, planeWidth,
                                 planeHeight)) {
            LOGE("lcevcPictureSetPlaneBuffers: unsupported format %d for plane %u",
                 desc.colorFormat, planeIndex);
            continue;
        }

        const uint32_t rowBytes = planeWidth * bytesPerSample;
        uint8_t* src = buffer + offsets[planeIndex];
        uint8_t* dst = plane.firstSample;
        const uint32_t srcStride = static_cast<uint32_t>(strides[planeIndex]);
        for (uint32_t row = 0; row < planeHeight; ++row) {
            memcpy(dst + row * plane.rowByteStride, src + row * srcStride, rowBytes);
        }
    }

    LCEVC_UnlockPicture(decoderHdl, lock);

    env->ReleaseIntArrayElements(inputOffsets, offsets, 0);
    env->ReleaseIntArrayElements(inputByteStrides, strides, 0);
    return LCEVC_Success;
}
