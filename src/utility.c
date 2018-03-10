/* Shifter, Copyright (c) 2015, The Regents of the University of California,
## through Lawrence Berkeley National Laboratory (subject to receipt of any
## required approvals from the U.S. Dept. of Energy).  All rights reserved.
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are met:
##  1. Redistributions of source code must retain the above copyright notice,
##     this list of conditions and the following disclaimer.
##  2. Redistributions in binary form must reproduce the above copyright notice,
##     this list of conditions and the following disclaimer in the documentation
##     and/or other materials provided with the distribution.
##  3. Neither the name of the University of California, Lawrence Berkeley
##     National Laboratory, U.S. Dept. of Energy nor the names of its
##     contributors may be used to endorse or promote products derived from this
##     software without specific prior written permission.
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
## AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
## IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
## ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
## LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
## CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
## SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
## INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
## CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
## ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
## POSSIBILITY OF SUCH DAMAGE.
##
## You are under no obligation whatsoever to provide any bug fixes, patches, or
## upgrades to the features, functionality or performance of the source code
## ("Enhancements") to anyone; however, if you choose to make your Enhancements
## available either publicly, or directly to Lawrence Berkeley National
## Laboratory, without imposing a separate written license agreement for such
## Enhancements, then you hereby grant the following license: a  non-exclusive,
## royalty-free perpetual license to install, use, modify, prepare derivative
## works, incorporate into other computer software, distribute, and sublicense
## such enhancements or derivative works thereof, in binary and source code
## form.
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <linux/limits.h>

#include "utility.h"


int shifter_parseConfig(const char *filename, char delim, void *obj, int (*assign_fp)(const char *, const char *, void *)) {
    FILE *fp = NULL;
    char *linePtr = NULL;
    char *ptr = NULL;
    size_t lineSize = 0;
    size_t nRead = 0;
    int multiline = 0;

    char *key = NULL;
    char *key_alloc = NULL;
    char *value = NULL;
    char *tValue = NULL;
    int ret = 0;

    if (filename == NULL || obj == NULL || assign_fp == NULL) {
        return 1;
    }

    fp = fopen(filename, "r");
    if (fp == NULL) {
        return 1;
    }
    while (!feof(fp) && !ferror(fp)) {
        char *tmp_value = NULL;
        nRead = getline(&linePtr, &lineSize, fp);
        if (nRead <= 0) break;
        if (linePtr[0] == '#') continue;

        /* get key/value pair */
        if (!multiline) {
            ptr = strchr(linePtr, delim);
            if (ptr == NULL) continue;
            *ptr++ = 0;
            key_alloc = strdup(linePtr);
            key = shifter_trim(key_alloc);
            if (key == NULL) {
                goto _parseConfig_errCleanup;
            }
            tValue = shifter_trim(ptr);
        } else {
            tValue = shifter_trim(linePtr);
            multiline = 0;
        }
        if (tValue == NULL) {
            goto _parseConfig_errCleanup;
        }

        /* check to see if value extends over multiple lines */
        if (tValue[strlen(tValue) - 1] == '\\') {
            multiline = 1;
            tValue[strlen(tValue) - 1] = 0;
            tValue = shifter_trim(tValue);
        }

        /* merge value and tValue */
        if (value == NULL) {
            value = strdup(tValue);
        } else {
            if (asprintf(&tmp_value, "%s %s", value, tValue) < 0) {
                goto _parseConfig_errCleanup;
            }
            if (tmp_value == NULL) {
                goto _parseConfig_errCleanup;
            }
            free(value);
            value = tmp_value;
            tmp_value = NULL;
        }
        tValue = NULL;

        /* if value is complete, assign */
        if (multiline == 0) {
            ptr = shifter_trim(value);

            ret = assign_fp(key, ptr, obj);
            if (ret != 0) goto _parseConfig_errCleanup;

            if (value != NULL) {
                free(value);
            }
            if (key_alloc != NULL) {
                free(key_alloc);
            }
            key = NULL;
            key_alloc = NULL;
            value = NULL;
        }
    }
    if (linePtr != NULL) {
        free(linePtr);
        linePtr = NULL;
    }
_parseConfig_errCleanup:
    if (fp != NULL) {
        fclose(fp);
    }
    if (linePtr != NULL) {
        free(linePtr);
    }
    if (value != NULL) {
        free(value);
    }
    if (key_alloc != NULL) {
        free(key_alloc);
    }
    return ret;
}


char *shifter_trim(char *str) {
    char *ptr = str;
    ssize_t len = 0;
    if (str == NULL) return NULL;
    for ( ; isspace(*ptr) && *ptr != 0; ptr++) {
        /* that's it */
    }
    if (*ptr == 0) return ptr;
    len = strlen(ptr) - 1;
    for ( ; isspace(*(ptr + len)) && len > 0; len--) {
        *(ptr + len) = 0;
    }
    return ptr;
}

/**
 * strncpy_StringArray
 * Append a string to the specified string array.  Assumes a relatively small
 * number of items.  Uses a block allocation scheem to expand array.  String
 * is destructively appended at the wptr position.  Pointers are naivly
 * verified to at least make sense.
 *
 * Affect: Space is allocated in array if necessary; then n bytes of string
 * str are appended to end of array.  A NULL terminator is appended to the
 * end of array.  wptr is left pointing to the NULL terminator for the next
 * append.
 *
 * Parameters:
 * str:  input string
 * n  :  bytes to copy
 * wptr: pointer to write position; value may be changed upon array
 *       reallocation, or successful append.  Left pointing to NULL terminator.
 * array: pointer to base of String Array. value may be updated upon
 *       reallocation
 * capacity: pointer to array capacity; updated upon reallocation
 * allocationBlock: number of elements to add per reallocation event, must be
 *       greater than 0.
 *
 * Returns: 0 upon success, 1 upon failure
 */
int strncpy_StringArray(const char *str, size_t n, char ***wptr,
        char ***array, size_t *capacity, size_t allocationBlock) {

    size_t count = 0;
    if (str == NULL || wptr == NULL || array == NULL
            || capacity == NULL || allocationBlock == 0 ||
            *wptr < *array || *wptr - *capacity > *array) {
        fprintf(stderr, "ERROR: invalid input to strncpy_StringArray\n");
        return 1;
    }
    if (n == SIZE_MAX) {
        fprintf(stderr, "ERROR: input string is too big, would not be able to "
                "append terminator\n");
        return 1;
    }

    /* allocate more space at a time */
    count = *wptr - *array;
    if (*capacity - count < 2) {
        size_t new_capacity = *capacity + allocationBlock;
        char **tmp = (char **) realloc(*array, sizeof(char *) * new_capacity);
        if (tmp == NULL) {
            fprintf(stderr, "ERROR: failed to allocate memory, append failed\n");
            return 1;
        }
        *array = tmp;
        *wptr = *array + count;
        *capacity += allocationBlock;
    }

    /* append string to array, add ternminated NULL */
    **wptr = (char *) malloc(sizeof(char) * (n + 1));
    memcpy(**wptr, str, n);
    (**wptr)[n] = 0;
    (*wptr)++;
    **wptr = NULL;
    return 0;
}

/**
 * alloc_strcatf appends to an existing string (or creates a new one) based on
 * user-supplied format in printf style.  The current capacity and length of
 * the string are used to determine where to write in the string and when to
 * allocate.  An eager-allocation strategy is used where the string capacity is
 * doubled at each reallocation.  This is done to improve performance of
 * repeated calls
 * Parameters:
 *      string - pointer to string to be extended, can be NULL
 *      currLen - pointer to integer representing current limit of used portion
 *                of string
 *      capacity - pointer to integer representing current capacity of string
 *      format - printf style formatting string
 *      ...    - variadic arguments for printf
 *
 * The dereferenced value of currLen is modified upon successful concatenation
 * of string.
 * The dereferenced value of capacity is modified upon successful reallocation
 * of string.
 *
 * Returns pointer to string, possibly realloc'd.  Caller should treat this
 * function like realloc (test output for NULL before assigning).
 * Returns NULL upon vsnprintf error or failure to realloc.
 */
char *alloc_strcatf(char *string, size_t *currLen, size_t *capacity, const char *format, ...) {
    int status = 0;
    int n = 0;

    if (currLen == 0 || capacity == 0 || format == NULL) {
        return NULL;
    }
    if (*currLen > *capacity) {
        return NULL;
    }

    if (string == NULL || *capacity == 0) {
        string = (char *) malloc(sizeof(char) * 128);
        *capacity = 128;
    }

    while (1) {
        char *wptr = string + *currLen;
        va_list ap;

        va_start(ap, format);
        n = vsnprintf(wptr, *capacity - (*currLen), format, ap);
        va_end(ap);

        if (n < 0) {
            /* error, break */
            status = 1;
            break;
        } else if ((size_t) n >= (*capacity - *currLen)) {
            /* if vsnprintf returns larger than allowed buffer, need more space
             * allocating eagerly to reduce cost for successive strcatf
             * operations */
            size_t newCapacity = *capacity * 2 + 1;
            char *tmp = NULL;
            if (newCapacity < (size_t) (n + 1)) {
                newCapacity = (size_t) (n + 1);
            }

            tmp = (char *) realloc(string, sizeof(char) * newCapacity);
            if (tmp == NULL) {
                status = 2;
                break;
            }
            string = tmp;
            *capacity = newCapacity;
        } else {
            /* success */
            status = 0;
            *currLen += n;
            break;
        }
    }
    if (status == 0) {
        return string;
    }
    return NULL;
}

/**
 * alloc_strgenf allocates and appopriately-sized buffer and populates it with
 * user-supplied format in printf style.
 * Parameters:
 *      format - printf style formatting string
 *      ...    - variadic arguments for printf
 *
 *
 * Returns pointer to string.
 * Returns NULL upon vsnprintf error or failure to realloc.
 */
char *alloc_strgenf(const char *format, ...) {
    char *string = NULL;
    int capacity = 0;
    int status = 0;
    int n = 0;

    if (format == NULL) {
        return NULL;
    }

    string = (char *) malloc(sizeof(char) * 128);
    capacity = 128;

    while (1) {
        va_list ap;

        va_start(ap, format);
        n = vsnprintf(string, capacity, format, ap);
        va_end(ap);

        if (n < 0) {
            /* error, break */
            status = 1;
            break;
        } else if (n >= capacity) {
            /* if vsnprintf returns larger than allowed buffer, need more space
             * allocating eagerly to reduce cost for successive strcatf
             * operations */
            size_t newCapacity = n + 1;
            char *tmp = (char *) realloc(string, sizeof(char) * newCapacity);
            if (tmp == NULL) {
                status = 2;
                break;
            }
            string = tmp;
            capacity = newCapacity;
        } else {
            /* success */
            status = 0;
            break;
        }
    }
    if (status == 0) {
        return string;
    }
    if (string != NULL) {
        free(string);
        string = NULL;
    }
    return NULL;
}

/**
 * userInputPathFilter screens out certain characters from user-provided strings

 * Parameters:
 *      input - the user provided string
 *      allowSlash - flag to allow a '/' in the string (1 for yes, 0 for no)
 *
 *
 * Returns pointer to newly allocated string.
 * Returns NULL if input is NULL or there is a memory allocation error
 */
char *userInputPathFilter(const char *input, int allowSlash) {
    ssize_t len = 0;
    char *ret = NULL;
    const char *rptr = NULL;
    char *wptr = NULL;
    if (input == NULL) return NULL;

    len = strlen(input) + 1;
    ret = (char *) malloc(sizeof(char) * len);
    if (ret == NULL) return NULL;

    rptr = input;
    wptr = ret;
    while (wptr - ret < len && *rptr != 0) {
        if (isalnum(*rptr) || *rptr == '_' || *rptr == ':' || *rptr == '.' || *rptr == '+' || *rptr == '-') {
            *wptr++ = *rptr;
        }
        if (allowSlash && *rptr == '/') {
            *wptr++ = *rptr;
        }
        rptr++;
    }
    *wptr = 0;
    return ret;
}

char *cleanPath(const char *path) {
    if (!path) return NULL;
    ssize_t len = strlen(path) + 1;

    char *ret = (char *) malloc(sizeof(char) * len);
    memset(ret, 0, sizeof(char) * len);

    char *wPtr = ret;
    const char *rPtr = path;
    char lastWrite = 0;

    while (rPtr && *rPtr && (wPtr - ret) < len) {
        /* prevent repeated '/' */
        if (*rPtr == '/' && lastWrite == '/') {
            rPtr++;
            continue;
        }

        *wPtr = *rPtr;
        lastWrite = *wPtr;
        wPtr++;
        rPtr++;
    }

    /* remove trailing '/' if path is longer than '/' */
    if (wPtr - ret > 1 && *(wPtr - 1) == '/') {
        *(wPtr - 1) = 0;
    }

    /* terminate string */
    *wPtr = 0;

    return ret;
}

int pathcmp(const char *a, const char *b) {
    if (a && !b) return 1;
    if (!a && b) return -1;
    if (!a && !b) return 0;

    char *myA = cleanPath(a);
    char *myB = cleanPath(b);
    int ret = strcmp(myA, myB);
    free(myA);
    free(myB);
    return ret;
}

int is_json_array(const char *value) {
  if (value[0]=='[' && value[1]=='u' && value[2]=='\''){
    return 1;
  }
  else {
    return 0;
  }
}

/**
 * split_json_array - utility function to parse a JSON array and split it
 * into an array of strings.  It will allocate the array but destroys the
 * source string.
 *
 * The values in the array should be comma-separated quoted strings.
 */
char ** split_json_array(const char *value) {
  char *ptr = NULL;
  int count=0;
  int in=0;
  char **array = NULL;
  int i;
  int vlen = strlen(value);
  char *start;
  int length;

  ptr=value;

  in = 0;
  count = 0;
  ptr++; // Get past the [
  for (i=2; i<vlen; i++, ptr++) {
    if (*ptr!='\'')
      continue;
    if (in){
      count++;
      in = 0;
    }
    else {
      in = 1;
    }
  }

  // Confirm we aren't in the middle of a string
  if (in) {
    fprintf(stderr, "Still in a string\n");
    return array;
  }

  // Confirm that the last character was a ]
  if (*ptr!=']') {
    fprintf(stderr, "Wrong termination\n");
    return array;
  }
  count++;

  array = (char **)malloc(sizeof(char *)*(count));
  if (array==NULL) {
    fprintf(stderr, "Allocation failed.\n");
    return array;
  }
  ptr = value;
  count = 0;
  in = 0;
  ptr++; // Get past the [
  length=0;
  for (i=2; i<vlen; i++, ptr++) {
    if (*ptr!='\''){
      length++;
      continue;
    }
    if (in){
      in = 0;
      // Terminate the string
      array[count]=strndup(start, length);
      count++;
    }
    else {
      in = 1;
      start = ptr;
      // Get past the quote
      start++;
      length=0;
    }
  }
  // Terminate the char array
  array[count]=NULL;
  i=0;
  ptr=array[0];
  while (array[i]!=NULL){
    //printf("%s\n", array[i]);
    i++;

  }
  return array;

}

/*
 * count return the number of elements including the
 * NULL termination.
 */
int _count_args(char **args) {
  int i=0;
  while (args[i]!=NULL) {
    i++;
  }
  i++;
  return i;
}

char ** merge_args(char **args1, char **args2) {
   int nArgs = 0;
   int s_index, d_index;
   char **newargs;
   nArgs=(_count_args(args1) + _count_args(args2) -1);
   newargs = (char **) malloc(sizeof(char *) * (nArgs ));
   d_index=0;
   s_index=0;
   while(args2[s_index]!=NULL) {
     newargs[d_index]=args2[s_index];
     s_index++;
     d_index++;
   }
   s_index=0;
   while(args1[s_index]!=NULL) {
     newargs[d_index]=args1[s_index];
     s_index++;
     d_index++;
   }
   newargs[d_index]=NULL;
   return  newargs;

}

char ** make_char_array(const char *value) {
  char **arr;
  arr = (char **)malloc(sizeof(char *)*2);
  if (arr==NULL){
    return NULL;
  }
  arr[0] = strdup(value);
  arr[1] = NULL;
  return arr;
}
