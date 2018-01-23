
#include <string>
#include <sstream>

#ifdef __cplusplus
extern "C" {
#endif

#include "mmaprecord.h"

#include "jstringholder.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "log.h"

mmap_info *get_mmap_info(JNIEnv *env, jobject object) {
    jclass mmap_record_clazz = env->GetObjectClass(object);
    jfieldID reference_id = env->GetFieldID(mmap_record_clazz, "mBufferInfoReference", "J");
    return reinterpret_cast<mmap_info *>(env->GetLongField(object, reference_id));
}

JNIEXPORT jint JNICALL
Java_com_chan_lib_MmapRecord_init(JNIEnv *env, jobject instance, jstring buffer, jstring log) {
    // open buffer
    JStringHolder buffer_path_string_holder(*env, buffer);
    const char *buffer_path = buffer_path_string_holder.getCString();
    int buffer_fd = open(buffer_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (buffer_fd < 0) {
        jclass exception = env->FindClass("java/lang/IllegalArgumentException");
        std::ostringstream oss;
        oss << "open " << buffer << " failed.";
        env->ThrowNew(exception, oss.str().c_str());
        return -1;
    }

    // open log
    JStringHolder log_path_string_holder(*env, log);
    const char *log_path = log_path_string_holder.getCString();
    int log_fd = open(log_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (log_fd < 0) {
        jclass exception = env->FindClass("java/lang/IllegalArgumentException");
        std::ostringstream oss;
        oss << "open " << log << " failed.";
        env->ThrowNew(exception, oss.str().c_str());
        return -1;
    }

    // read dirty data
    struct stat buffer_file_stat;
    if (fstat(buffer_fd, &buffer_file_stat) >= 0) {
        size_t buffer_file_size = (size_t) buffer_file_stat.st_size;
        if (buffer_file_size > 0) {
            char *buffered_data = (char *) mmap(0, buffer_file_size, PROT_WRITE | PROT_READ,
                                                MAP_SHARED, buffer_fd, 0);
            LOG_D("%s", buffered_data);
        }
    }

    // write data
    const int buffer_size = 1000;
    // 1000 is buffer size
    ftruncate(buffer_fd, buffer_size);
    lseek(buffer_fd, 0, SEEK_SET);
    char *map_ptr = (char *) mmap(0, buffer_size, PROT_WRITE | PROT_READ, MAP_SHARED, buffer_fd, 0);
    if (map_ptr == MAP_FAILED) {
        return -2;
    }

    mmap_info *info = new mmap_info;
    info->buffer = map_ptr;
    info->buffer_fd = buffer_fd;
    info->log_fd = log_fd;
    info->buffer_size = buffer_size;

    jclass mmap_record_clazz = env->GetObjectClass(instance);
    jfieldID reference_id = env->GetFieldID(mmap_record_clazz, "mBufferInfoReference", "J");
    env->SetLongField(instance, reference_id, (jlong) info);

    return 0;
}


JNIEXPORT void JNICALL
Java_com_chan_lib_MmapRecord_release(JNIEnv *env, jobject instance) {
    mmap_info *info = get_mmap_info(env, instance);
    if (info == nullptr) {
        return;
    }

    close(info->buffer_fd);
    close(info->log_fd);
}

JNIEXPORT void JNICALL
Java_com_chan_lib_MmapRecord_save(JNIEnv *env, jobject object, jstring json) {
    mmap_info *info = get_mmap_info(env, object);
    if (info == nullptr) {
        return;
    }

    JStringHolder json_holder(*env, json);
    const char *c_json = json_holder.getCString();
    size_t c_json_len = strlen(c_json);
    if (c_json_len >= info->buffer_size) {
        // TODO
        LOG_D("%s", "json too long");
        return;
    }

    info->used_size = c_json_len;
    memcpy(info->buffer, c_json, c_json_len);
}

JNIEXPORT jstring JNICALL
Java_com_chan_lib_MmapRecord_read(JNIEnv *env, jobject instance) {
    mmap_info *info = get_mmap_info(env, instance);
    if (info == nullptr || info->used_size <= 0) {
        return nullptr;
    }

    return env->NewStringUTF(info->buffer);
}


#ifdef __cplusplus
}
#endif