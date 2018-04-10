#include <jni.h>
#include <android/log.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h> 
#include <time.h>
#include "dex.h"

#define NELEM(x) ((int)(sizeof(x) / sizeof((x)[0])))
#define FLOG_TAG "ALi"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, FLOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, FLOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, FLOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, FLOG_TAG, __VA_ARGS__)
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, FLOG_TAG, __VA_ARGS__)


struct FixFuncInfoHeader {
    uint multiDexId;      //dex标识，0表示classes.dex,否则表示classes{$multiDexId}.dex,如2表示classes2.dex
    uint dataLen;         //FixFuncInfo长度
    uint fixFuncLen;      //FixFunc长度
    uint offset;          //该结构在libdemolishdata.so中的偏移
};

struct FixFunc {
    uint id;             //方法标志，对用fixfunc调用参数
    uint multiDexId;     //dex标识
    uint dexMethodIdx;   //dex文件格式中的DexMethodId列表的索引
    uint codeoff;        //该方法对应的DexCode结构相对于当前FixFuncInfo的偏移量
};

struct DexCode {
    u2 registersSize;
    u2 insSize;
    u2 outsSize;
    u2 triesSize;
    u4 debugInfoOff;
    u4 insnsSize;
    u2 insns[1];
};

struct FixFuncInfo {
    FixFuncInfoHeader *fixFuncInfoHeader;
    FixFunc *fixFuncList;
    DexCode *dexCodeList;
};

struct DexCheckSumInfo{
    uint multiDexId;   //dex标识，0表示classes.dex,否则表示classes{$multiDexId}.dex,如2表示classes2.dex
    uint checksum;     //该dex的alder32,与DexHeader.checksum相同
    uint hash;         //该dexSha1的一部分,与DexHeader.signature的前4字节相同
    void *dexBase;
};

//libdemolishdata.so的整体结构
struct LibDemolishData {
    FixFuncInfoHeader *fixFuncInfoHeaderList;
    int fixFuncLen;    //FixFunc结构总长度
    int fixFuncNum;    //待修复函数的个数
    int dexNum;        //dex个数
    FixFunc *fixFuncList;
    DexCheckSumInfo *dexCheckSumInfoList;
    LibDemolishData() {
        fixFuncInfoHeaderList = 0;
        fixFuncLen = 0;
        fixFuncNum = 0;
        fixFuncList = 0;
        dexCheckSumInfoList = 0;
    }
};

LibDemolishData libDemolishData;

uint8_t *dexStart = 0;
uint8_t *dexEnd = 0;
uint8_t *dexAddress = 0;
uint8_t *stringIds = 0;
uint8_t *typeIds = 0;
uint8_t *methodIds = 0;
uint8_t *protoIds = 0; 
uint8_t *libdemolishDataBase = 0;

const char *sourceDir;
const char *publicSourceDir;
const char *dataDir;
const char *processName;

char *jstring2str(JNIEnv *env, jstring jstr)
{
    char *rtn = NULL;
    jclass clsstring = env->FindClass("java/lang/String");
    jstring strencode = env->NewStringUTF("GB2312");
    jmethodID mid = env->GetMethodID(clsstring, "getBytes", "(Ljava/lang/String;)[B");
    jbyteArray barr = (jbyteArray)env->CallObjectMethod(jstr, mid, strencode);
    jsize alen = env->GetArrayLength(barr);
    jbyte *ba = env->GetByteArrayElements(barr, JNI_FALSE);
    if (alen > 0)
    {
        rtn = (char *)malloc(alen + 1);
        memcpy(rtn, ba, alen);
        rtn[alen] = 0;
    }
    env->ReleaseByteArrayElements(barr, ba, 0);
    return rtn;
}

int handleException(JNIEnv *env)
{
    jthrowable exception = env->ExceptionOccurred();
    if (exception)
    {
        env->ExceptionClear();
        LOGI("[handleException] exception occured");
        jclass clazz = env->GetObjectClass(exception);
        jmethodID getMessage = env->GetMethodID(clazz, "getMessage", "()Ljava/lang/String;");
        jstring message = (jstring)env->CallObjectMethod(exception, getMessage);
        LOGI("error message is: %s", jstring2str(env, message));
        env->ExceptionClear();
        return 1;
    }
    return 0;
}

void soInit(JNIEnv *env, jclass clazz);
void fixfunc(JNIEnv *env, jclass clazz, jintArray array);

static JNINativeMethod method_table[] = {
    {"soInit", "()V", (void *)soInit},
    {"fixfunc", "([I)V", (void *)fixfunc},
};

int register_ndk_load(JNIEnv *env)
{
    jclass clazz = env->FindClass("com/ali/fixHelper");
    if (clazz == 0) {
        return JNI_FALSE;
    }
    if (env->RegisterNatives(clazz, method_table, NELEM(method_table)) < 0) {
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

JNIEXPORT jint JNICALL JNI_OnLoad (JavaVM* vm, void* reserved)
{
    LOGI("JNI_OnLoad");
    JNIEnv *env = NULL;
    if(vm->GetEnv((void **)&env, JNI_VERSION_1_4)!= JNI_OK) {
        return -1;
    }
    if (!register_ndk_load(env)) {
        LOGI("register failed!");
        return -1;
    }
    return JNI_VERSION_1_4;
}

//获取sdk版本号，sub_4214
int sdk_init(JNIEnv *env) 
{
    jclass clazz = env->FindClass("android/os/Build$VERSION");
    jfieldID field = env->GetStaticFieldID(clazz, "SDK_INT", "I");
    return env->GetStaticIntField(clazz, field);
}

//获取app信息，sourceDir， processName等, sub_6DD8
void getAppInfo(JNIEnv *env) 
{
    jclass activityThread = env->FindClass("android/app/ActivityThread");
    jfieldID mBoundApplication = env->GetFieldID(activityThread, "mBoundApplication", "Landroid/app/ActivityThread$AppBindData;");
    jmethodID currentActivityThread = env->GetStaticMethodID(activityThread, "currentActivityThread", "()Landroid/app/ActivityThread;");
    jclass appBindData = env->FindClass("android/app/ActivityThread$AppBindData");
    jfieldID appinfo = env->GetFieldID(appBindData, "appInfo", "Landroid/content/pm/ApplicationInfo;");

    jobject theCurrentActivityThread = env->CallStaticObjectMethod(activityThread, currentActivityThread);
    jobject themBoundApplication = env->GetObjectField(theCurrentActivityThread, mBoundApplication);
    jobject theAppInfo = env->GetObjectField(themBoundApplication, appinfo);
   
    jclass clazz = env->GetObjectClass(theAppInfo);
    jfieldID sourceDirID = env->GetFieldID(clazz, "sourceDir", "Ljava/lang/String;");
    jfieldID publicSourceDirID = env->GetFieldID(clazz, "publicSourceDir", "Ljava/lang/String;");
    jfieldID dataDirID = env->GetFieldID(clazz, "dataDir", "Ljava/lang/String;");
    jfieldID processNameID = env->GetFieldID(clazz, "processName", "Ljava/lang/String;");

    jstring msourceDir = env->GetObjectField(theAppInfo, sourceDirID);
    jstring mpublicSourceDir = env->GetObjectField(theAppInfo, publicSourceDirID);
    jstring mdataDir = env->GetObjectField(theAppInfo, dataDirID);
    jstring mprocessName = env->GetObjectField(theAppInfo, processNameID);

    sourceDir = jstring2str(env, msourceDir);
    publicSourceDir = jstring2str(env, mpublicSourceDir);
    dataDir = jstring2str(env, mdataDir);
    processName = jstring2str(env, mprocessName);

    LOGI("soruceDir: %s", sourceDir);
    LOGI("publicSourceDir: %s", publicSourceDir);
    LOGI("dataDir: %s", dataDir);
    LOGI("processName: %s", processName);
}

//获取 libdemolishdata 存放路径, sub_6BC2
char *getLibdemolishdataPath(JNIEnv *env)
{
    int pid = getpid();
    char maps[32] = {0};
    char buffer[512];
    snprintf(maps, 32, "/proc/%d/maps", pid);
    FILE *fp = fopen(maps, "r");
    if (!fp) {
        LOGI("open %s error!", maps);
        return 0;
    }
    while (fgets(buffer, 512, fp) && !strstr(buffer, "libdemolish.so")) ;
    fclose(fp);
    int len = strlen(buffer);
    int v6 = 0, v7 = len;
    while (v6 != v7) {
        if(buffer[v6] == 47) {
            break;
        }
        ++v6;
    }
    while (v7 > 0 ) {
        if (buffer[v7] == 47) {
            break;
        }
        --v7;
    }
    char *so = "libdemolishdata.so";
    char *result = (char *) malloc(128);
    memset(result, 0, 128);
    int v9 = v7 - v6 + 1;
    memcpy(result, buffer + v6, v9);
    *(result + v9) = 0;
    //LOGI("native-lib dir: %s", result);
    strcat(result, so);
    if (access(result, 0)) {
        //TODO:处理错误
        LOGE("error: cannot open libdemolishdata.so");
        return 0;
    }
    return result;
}

//读取libdemolishdata.so 中的 DexCheckSumInfo结构到内存 sub_46E0
void *loadDexcheckSumInfo(uint8_t *base, int num, int size)
{
    uint8_t *buffer = (uint8_t *) malloc(16 *num);
    libDemolishData.dexCheckSumInfoList = (DexCheckSumInfo *)buffer;
    uint8_t *begin = base + size - 4 - 12 * num;
    for (int i = 0; i < num; i++) {
        memcpy(buffer, begin, 12);
        *(buffer + 12) = 0;
        buffer += 16;
        begin += 12;
    }
    /*
    for (int i = 0; i < num; i++) {
        LOGI("Dex ID: %x", libDemolishData.dexCheckSumInfoList[i].multiDexId);
        LOGI("Dex checksum: %x", libDemolishData.dexCheckSumInfoList[i].checksum);
        LOGI("Dex hash: %x", libDemolishData.dexCheckSumInfoList[i].hash);
    }
    */
    return libDemolishData.dexCheckSumInfoList;
}

//读取libdemolishdata.so中的FixFuncInfoHeader sub_477c
int loadFixFuncInfoHeader(uint32_t *base, int num)
{
    uint32_t *buffer = (uint32_t *) malloc(16 *num);
    libDemolishData.fixFuncInfoHeaderList = (FixFuncInfoHeader *)buffer;
    int result = 0;
    for (int i = 0; i < num; i++) {
        buffer[0] = base[1];
        buffer[1] = base[2];
        buffer[2] = base[3] * 16;
        buffer[3] = result;
        libDemolishData.fixFuncLen += buffer[2];
        base = (uint32_t *) ((uint8_t *)base + buffer[1]);
        buffer += 4;
        result += buffer[1];
    }
    /*
    for (int i = 0; i < num; i++) {
        LOGI("header DexID: %x", libDemolishData.fixFuncInfoHeaderList[i].multiDexId);
        LOGI("header dataLen: %x", libDemolishData.fixFuncInfoHeaderList[i].dataLen);
        LOGI("header fixFuncLen: %x", libDemolishData.fixFuncInfoHeaderList[i].fixFuncLen);
        LOGI("header offset: %x", libDemolishData.fixFuncInfoHeaderList[i].offset);
    }
    */
    return result;
}

//读取libdemolishdata.so中的FixFunc sub_4812
void *loadFixFunc(uint8_t *base, int num)
{
    int totalSize = libDemolishData.fixFuncLen;
    uint8_t *buffer = (uint8_t *) malloc(totalSize);
    libDemolishData.fixFuncList = (FixFunc *)buffer;
    uint8_t *result = memset(buffer, 0, libDemolishData.fixFuncLen);
    int v14 = 0;
    for (int i = 0; i < num; i++) {
        FixFuncInfoHeader * header = libDemolishData.fixFuncInfoHeaderList;
        result = memcpy(buffer + v14, base + 16, header[i].fixFuncLen);
        base += header[i].dataLen;
        v14 += header[i].fixFuncLen;
    }
    libDemolishData.fixFuncNum = totalSize / 16;
    LOGI("fixFunc total size: %x", libDemolishData.fixFuncLen);
    
    /*
    for (int i = 0; i < libDemolishData.fixFuncNum; i++) {
        LOGI("fixFunc id: %x", libDemolishData.fixFuncList[i].id);
        LOGI("fixFunc DexID: %x", libDemolishData.fixFuncList[i].multiDexId);
        LOGI("fixFunc DexMethodIdx: %x", libDemolishData.fixFuncList[i].dexMethodIdx);
        LOGI("fixFunc codeoff: %x", libDemolishData.fixFuncList[i].codeoff);
    }
    */

    return result;
}

//将字符串str中的a都替换为b sub_8930
char *replace(char *str, char a, char b)
{
    for (char *i = str; *i; i++) {
        if(*i == a) {
            *i = b;
        }
    }
    return str;

}

//获取classes.dex被加载到内存的起始地址与终止地址
void getDexAddress()
{
    int pid = getpid();
    char maps[32] = {0};
    char buffer[512];
    snprintf(maps, 32, "/proc/%d/maps", pid);
    FILE *fp = fopen(maps, "r");
    if (!fp) {
        LOGI("open %s error!", maps);
        return 0;
    }
    while (fgets(buffer, 512, fp) && !strstr(buffer, "apk@classes.dex"));
    fclose(fp);
    sscanf(buffer, "%x-%x", &dexStart, &dexEnd);
    LOGI("dex start address: %x", dexStart);
    LOGI("dex end address: %x", dexEnd);
}


//获取每个DexCheckSumInfo结构所对应dex在内存的起始地址 sub_5506
uint *getDexStartAddress(uint8_t *start, uint8_t *end, int dexID) 
{
    char magic[4];
    memcpy(magic, "dex\ndex\n035", 4);
    int dexNum = libDemolishData.dexNum;
    uint *result;
    while (1) {
        result = (uint *)memmem(start, end - start, magic, 4);
        if (!result) {
            break;
        }
        LOGI("find dex magic in: %x", result);

        int v7 = 0;
        DexCheckSumInfo *v8 = libDemolishData.dexCheckSumInfoList;
        while (v7 != dexNum) {
            if (dexID == v8[v7].multiDexId && result[3] == v8[v7].hash) {
                v8[v7].dexBase = result;
                LOGI("dex real start Address: %x", result);
                return result;
            }
            v7++;
        }
        start = (uint8_t *)result + 4;
    }
    return result;
}

//判断虚拟机为dalvik还是art sub_4A14
int dalivkOrArt(JNIEnv *env)
{
    int pid = getpid();
    char path[32];
    memset(path, 0 ,32);
    snprintf(path, 32, "/proc/%d/maps", pid);
    FILE *fp = fopen(path, "r");
    char buffer[256];
    while (fgets(buffer, 256, fp)) {
        if (strstr(buffer, "/system/lib/libdvm.so")) {
            fclose(fp);
            return 0;
        }
        if (strstr(buffer, "/system/lib/libart.so")) {
            fclose(fp);
            return 1;
        }
        if (strstr(buffer, "/system/lib/libaoc.so")) {
            fclose(fp);
            return 1;
        }
    }
    return 0;
}

void soInit(JNIEnv *env, jclass clazz) 
{
    clock_t start, finish;
    LOGI("init~~");
    start = clock();
    //get sdk version
    int sdk = sdk_init(env);
    LOGI("sdk version: %d", sdk);
    
    //get app info
    getAppInfo(env);
    
    //get libdemolishdata path
    char *libpath = getLibdemolishdataPath(env);
    //LOGI("libdmemolishdata.so path: %s", libpath);
    
    //mmap libdemolishdata.so
    int data_handle = open(libpath, 0);
    struct stat statbuff; 
    if (stat(libpath, &statbuff) < 0) {
        LOGI("cannot stat %s", libpath);
    }
    //LOGI("libdemolishdata.so size: %d", statbuff.st_size);
    int dataSize = 0xc0;
    int *data = (int *)mmap(0, dataSize, 0x7, 0x2, data_handle, 0);
    libdemolishDataBase = (uint8_t *)data;
    if (data == -1) {
        LOGI("libdmeolisdata load failed");
        return;
    }
    LOGI("libdemolishdata.so address: %x", data);
    
    //load libdemolishdata.so into memery
    int dexNum = *(int *)((uint8_t *)data + dataSize - 4);
    libDemolishData.dexNum = dexNum;
    LOGI("fixDex num: %d", dexNum);
    loadDexcheckSumInfo((uint8_t *)data, dexNum, dataSize);
    loadFixFuncInfoHeader((uint32_t *)data, dexNum);
    loadFixFunc((uint8_t *)data, dexNum);

    //TODO: sub_883E 对fixFunc排序
    //TODO: sub_4298 判断是否为yun_os
    //TODO: sub_4A14 判断系统为dalvik 或者 art
    if (!dalivkOrArt(env)) {
        LOGI("dalvik");
    }
    else {
        LOGI("art");
        return;
    }

    char packageName[125];
    memset(packageName, 0, 125);
    replace(publicSourceDir, '/', '@');
    strcpy(packageName, publicSourceDir);
    //TODO: sub_8998 字符串变化，具体没分析
    strcat(packageName, "@classes.dex");

    getDexAddress();
    dexAddress = (uint8_t *)getDexStartAddress(dexStart, dexEnd, 0);
    //dexAddress = dexStart + 0x28;

    close(data_handle);
    
    finish = clock();
    LOGI("init finish cost total: %f s", (double)(finish - start)/CLOCKS_PER_SEC);
}

int readUleb128(char* address, int* read_count)
{
    //printf("redU128\n");
    char *read_address;
    int result;
    signed int bytes_read;
    signed int value_1;
    signed int value_2;
    signed int value_3;

    read_address = address;
    result = *address;
    bytes_read = 1;
    if ( (unsigned int)result > 0x7F )
    {
        value_1 = *((unsigned char *)read_address + 1);
        result = result & 0x7F | ((value_1 & 0x7F) << 7);
        bytes_read = 2;
        //std::cout << "value_1 " << value_1 << std::endl;
        if ( value_1 > 127 )
        {
            //std::cout << "value > 127" << std::endl;
            value_2 = *((unsigned char *)read_address + 2);
            result |= (value_2 & 0x7F) << 14;
            bytes_read = 3;

            if ( value_2 > 127 )
            {
                value_3 = *((unsigned char *)read_address + 3);
                bytes_read = 4;
                result |= (value_3 & 0x7F) << 21;
                if ( value_3 > 127 )
                {
                    bytes_read = 5;
                    result |= *((char *)read_address + 4) << 28;
                }
            }
        }
    }
    *read_count = bytes_read;
    //printf("redU128\n");
    return result;
}

//查找函数的MethodID sub_71FE
jmethodID findMethodID(JNIEnv *env, char *className, const char *funcName, const char* signature)
{
    className += 1;
    int len = strlen(className);
    className[len - 1] = 0;
    jclass clazz = env->FindClass(className);
    handleException(env);
    jmethodID result = env->GetMethodID(clazz, funcName, signature);
    handleException(env);
    return result;
}

char *getMethodPara(DexProtoId *dexProtoId)
{
    char *result = malloc(125);
    memset(result, 0, 125);
    uint parasOff = dexProtoId->parametersOff;
    if (!parasOff) {
        result[0] = '(';
        result[1] = ')';
        return result;
    }
    DexTypeList *dexTypeList = (DexTypeList *)(parasOff + dexAddress);
    result[0] = '(';
    int size = dexTypeList->size;
    DexTypeItem *dexTypeItem = dexTypeList->list;
    int strLen, read_count;
    for (int i = 0; i < size; i++) {
        DexTypeId *dexTypeId = (DexTypeId *)(typeIds + dexTypeItem[i].typeIdx * 4);
        char *name = (char *)(*(uint *)(stringIds + dexTypeId->descriptorIdx * 4) + dexAddress);
        strLen = readUleb128(name, &read_count);
        name += read_count;
        strcat(result, name);
    }
    strLen = strlen(result);
    result[strLen] = ')';
    return result;
}


//复原Accessflags sub_7C66
uint fixAccessFlag(uint flag)
{
  if ( (flag & 0x100) == 256 )
    flag ^= 0x100u;
  return flag | 0x80000;
}

void doFixFunc(JNIEnv *env, int num)
{
    LOGI("methodIdx is %x", libDemolishData.fixFuncList[num].dexMethodIdx);
    DexMethodId *dexMethodId = (DexMethodId *)(methodIds + libDemolishData.fixFuncList[num].dexMethodIdx * 8);
    //LOGI("method class_idx is: %x", dexMethodId->classIdx);
    //LOGI("method proto_idx is: %x", dexMethodId->protoIdx);
    //LOGI("method name_idx is: %x", dexMethodId->nameIdx);
    DexCode *dexCode = (DexCode *)(libdemolishDataBase + (libDemolishData.fixFuncList[num].codeoff));
    DexTypeId *dexTypeId = (DexTypeId *)(typeIds + dexMethodId->classIdx * 4);

    char *className = (char *)(*(uint *)(stringIds + dexTypeId->descriptorIdx * 4) + dexAddress);
    int read_count, strLen;
    strLen = readUleb128(className, &read_count);
    className += read_count;
    char *cn = malloc(strLen + 1);
    strcpy(cn, className);
    LOGI("fix class: %s", cn);

    char *funcName = (char *)(*(uint *)(stringIds + dexMethodId->nameIdx * 4) + dexAddress);
    strLen = readUleb128(funcName, &read_count);
    funcName += read_count;
    char *fn = malloc(strLen + 1);
    strcpy(fn, funcName);
    LOGI("fix function: %s", fn);

    //得到函数签名
    DexProtoId *dexProtoId = (DexProtoId *)(protoIds + dexMethodId->protoIdx * 12);
    //函数参数
    char *param = getMethodPara(dexProtoId);
    //函数返回值
    DexTypeId *returnTypeId = (DexTypeId *)(typeIds + dexProtoId->returnTypeIdx * 4);
    char *returnName = (char *)(*(uint *)(stringIds + returnTypeId->descriptorIdx * 4) + dexAddress);
    strLen = readUleb128(returnName, &read_count);
    returnName += read_count;
    char *rn = malloc(strLen + 1);
    strcpy(rn, returnName);
    LOGI("fix fucntion return type: %s", rn);

    //函数签名
    strcat(param, rn);
    LOGI("fix function signature: %s", param);

    Method *method = (Method *)findMethodID(env, cn, fn, param);
    method->insns = dexCode->insns;
    method->registersSize = dexCode->registersSize;
    method->outsSize = dexCode->outsSize;
    method->accessFlags = fixAccessFlag(method->accessFlags);

    free(cn);
    free(fn);
    free(rn);
    free(param);
}

//利用二分查找法找到编号为num的函数，在fixFuncList中的位置 sub_877E
int findFixFuncListOffset(FixFunc *fixFuncList, int size, int num)
{
    int i = 0;
    while (i < size) {
        int j = (i + size) / 2;
        int mid = fixFuncList[j].id;
        if (mid == num) {
            return j;
        }
        if (mid > num) {
            size = j - 1;
        }
        else {
            i = j + 1;
        }
    }
    return 0;
}

void fixfunc(JNIEnv *env, jclass clazz, jintArray array)
{
    LOGI("fixfunc");
    int size = env->GetArrayLength(array);
    jint* funcs = env->GetIntArrayElements(array, 0);
    //读取dex header
    uint8_t dexHeader[0x70];
    memset(dexHeader, 0, 0x70);
    memcpy(dexHeader, dexAddress, 0x70);

    stringIds = *(uint32_t *)(dexHeader + 0x3c) + dexAddress;
    //LOGI("stringIds base address: %x", stringIds);
    typeIds = *(uint32_t *)(dexHeader + 0x44) + dexAddress;
    //LOGI("typeIds base address: %x", typeIds);
    methodIds = *(uint32_t *)(dexHeader + 0x5c) + dexAddress;
    //LOGI("methodIds base address: %x", methodIds);
    protoIds = *(uint32_t *)(dexHeader + 0x4c) + dexAddress;
    //LOGI("protoIds base address: %x", protoIds);
    //fix function
    for (int i = 0; i < size; i++) {
        int offset = findFixFuncListOffset(libDemolishData.fixFuncList, size, funcs[i]);
        LOGI("fix Function: %d", libDemolishData.fixFuncList[offset].id);
        doFixFunc(env, offset);
    }

    //TODO
    return;
}



