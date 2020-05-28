#ifndef KT_BIN_IO_H_
#define KT_BIN_IO_H_
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

class KT_BinIO {
private:
    
public:
    uint8_t *pReadBuff;
    uint8_t *pWriteBuff;
    uint32_t Read(char *pszInput);
    uint32_t InitBuffer();
    void FreeBuffer();
    uint32_t u32Size;
protected:
    int parse_hex_line(char *theline, int *bytes, int *addr, int *num, int *code);
};

#endif
