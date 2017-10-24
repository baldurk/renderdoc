/* -*- indent-tabs-mode: nil -*-
 *
 * plthook.h -- the header file of plthook
 *
 * URL: https://github.com/kubo/plthook
 *
 * ------------------------------------------------------
 *
 * Copyright 2013-2014 Kubo Takehiro <kubo@jiubao.org>
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ''AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of the authors.
 *
 */
#ifndef PLTHOOK_H
#define PLTHOOK_H 1

#define PLTHOOK_SUCCESS              0
#define PLTHOOK_FILE_NOT_FOUND       1
#define PLTHOOK_INVALID_FILE_FORMAT  2
#define PLTHOOK_FUNCTION_NOT_FOUND   3
#define PLTHOOK_INVALID_ARGUMENT     4
#define PLTHOOK_OUT_OF_MEMORY        5
#define PLTHOOK_INTERNAL_ERROR       6
#define PLTHOOK_NOT_IMPLEMENTED      7

typedef struct plthook plthook_t;

#ifdef __cplusplus
extern "C" {
#endif

int plthook_open(plthook_t **plthook_out, const char *filename);
int plthook_open_by_handle(plthook_t **plthook_out, void *handle);
int plthook_open_by_address(plthook_t **plthook_out, void *address);
int plthook_enum(plthook_t *plthook, unsigned int *pos, const char **name_out, void ***addr_out);
int plthook_replace(plthook_t *plthook, const char *funcname, void *funcaddr, void **oldfunc);
void plthook_close(plthook_t *plthook);
const char *plthook_error(void);

#ifdef __cplusplus
}; /* extern "C" */
#endif

#endif
