/**
 * Copyright (C) 2013-2015 Huawei Technologies Finland Oy.
 *
 * This is unpublished work. See README file for more information.
 */

#ifndef TESTS_GEN_ADLER32
#define TESTS_GEN_ADLER32

const int MOD_ADLER = 65521;

 /* data is the location of the data in physical memory and
    len is the length of the data in bytes */
unsigned int adler32(unsigned char *data, unsigned int len)
{
    unsigned int a = 1, b = 0;
    unsigned int index;

    /* Process each byte of the data in order */
    for (index = 0; index < len; ++index)
    {
        a = (a + data[index]) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }

    return (b << 16) | a;
}

#endif
