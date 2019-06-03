#ifndef _BASE64_H
#define _BASE64_H

#ifdef __cplusplus
extern "C" {
#endif

int b64_encode(const void *_src, size_t srclength,
	       void *dest, size_t targsize);

#ifdef __cplusplus
}
#endif


#endif
