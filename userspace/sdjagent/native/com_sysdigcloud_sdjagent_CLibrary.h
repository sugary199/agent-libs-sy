/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class com_sysdigcloud_sdjagent_CLibrary */

#ifndef _Included_com_sysdigcloud_sdjagent_CLibrary
#define _Included_com_sysdigcloud_sdjagent_CLibrary
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     com_sysdigcloud_sdjagent_CLibrary
 * Method:    real_seteuid
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_com_sysdigcloud_sdjagent_CLibrary_real_1seteuid
  (JNIEnv *, jclass, jlong);

/*
 * Class:     com_sysdigcloud_sdjagent_CLibrary
 * Method:    real_setegid
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_com_sysdigcloud_sdjagent_CLibrary_real_1setegid
  (JNIEnv *, jclass, jlong);

/*
 * Class:     com_sysdigcloud_sdjagent_CLibrary
 * Method:    real_setenv
 * Signature: (Ljava/lang/String;Ljava/lang/String;I)I
 */
JNIEXPORT jint JNICALL Java_com_sysdigcloud_sdjagent_CLibrary_real_1setenv
  (JNIEnv *, jclass, jstring, jstring, jint);

/*
 * Class:     com_sysdigcloud_sdjagent_CLibrary
 * Method:    real_unsetenv
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_sysdigcloud_sdjagent_CLibrary_real_1unsetenv
  (JNIEnv *, jclass, jstring);

/*
 * Class:     com_sysdigcloud_sdjagent_CLibrary
 * Method:    setns
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL Java_com_sysdigcloud_sdjagent_CLibrary_setns
  (JNIEnv *, jclass, jint, jint);

/*
 * Class:     com_sysdigcloud_sdjagent_CLibrary
 * Method:    open_fd
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_sysdigcloud_sdjagent_CLibrary_open_1fd
  (JNIEnv *, jclass, jstring);

/*
 * Class:     com_sysdigcloud_sdjagent_CLibrary
 * Method:    close_fd
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_com_sysdigcloud_sdjagent_CLibrary_close_1fd
  (JNIEnv *, jclass, jint);

/*
 * Class:     com_sysdigcloud_sdjagent_CLibrary
 * Method:    realCopyToContainer
 * Signature: (Ljava/lang/String;ILjava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_sysdigcloud_sdjagent_CLibrary_realCopyToContainer
  (JNIEnv *, jclass, jstring, jint, jstring);

/*
 * Class:     com_sysdigcloud_sdjagent_CLibrary
 * Method:    realRunOnContainer
 * Signature: (ILjava/lang/String;[Ljava/lang/String;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_sysdigcloud_sdjagent_CLibrary_realRunOnContainer
  (JNIEnv *, jclass, jint, jint, jstring, jobjectArray, jstring);

/*
 * Class:     com_sysdigcloud_sdjagent_CLibrary
 * Method:    realRmFromContainer
 * Signature: (ILjava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_sysdigcloud_sdjagent_CLibrary_realRmFromContainer
  (JNIEnv *, jclass, jint, jstring);

/*
 * Class:     com_sysdigcloud_sdjagent_CLibrary
 * Method:    getInodeOfFile
 * Signature: (Ljava/lang/String;)J
 */
JNIEXPORT jlong JNICALL Java_com_sysdigcloud_sdjagent_CLibrary_getInodeOfFile
  (JNIEnv *, jclass, jstring);

#ifdef __cplusplus
}
#endif
#endif
