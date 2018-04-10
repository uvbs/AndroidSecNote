#ifndef M_DEX_H
#define M_DEX_H
#include <stdint.h>
#include <jni.h>
typedef uint8_t u1;
typedef uint16_t u2;
typedef uint32_t u4;

struct DexMethodId {
    u2 classIdx;
    u2 protoIdx;
    u4 nameIdx;
};

struct DexTypeId {
    u4 descriptorIdx;
};

struct DexProtoId {
    u4 shortyIdx;
    u4 returnTypeIdx;
    u4 parametersOff;
};

struct DexTypeItem {
    u2 typeIdx;
};

struct DexTypeList {
    u4 size;
    DexTypeItem list[1];
};

struct DexProto {
    const void* dexFile; /* file the idx refers to */ // const DexFile
    u4 protoIdx; /* index into proto_ids table of dexFile */
};

struct Method {
    void *clazz;
    u4 accessFlags;
    u2 methodIndex;
    u2 registersSize;
    u2 outsSize;
    u2 insSize;
    const char* name;
    DexProto prototype;
    const char* shorty;
    const u2* insns; /* instructions, in memory-mapped .dex */
    int jniArgInfo;
    void *nativeFunc;
    bool fastJni;
    bool noRef;
    bool shouldTrace;
    const void* registerMap;
    bool inProfile;
};

#endif