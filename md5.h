#pragma once

#ifdef __cplusplus
extern "C"{
#endif

/**
 * make md5 string from src string to buf.
 * buf need is equal to or greater than 33 bytes.
 * */
char *md5_make (const char *src, char *buf, int len);

/**
 * make md5 string from src data to buf.
 * buf need is equal to or greater than 33 bytes.
 * */
char *md5_makedata (const char *src, int srclen, char *buf, int len);

#ifdef __cplusplus
}
#endif
