#include "KT_BinIO.h"

int KT_BinIO::parse_hex_line(char *theline, int *bytes, int *addr, int *num, int *code){
	int sum, len, cksum;
	char *ptr;
	
	*num = 0;
	if (theline[0] != ':') return 0;
	if (strlen(theline) < 11) return 0;
	ptr = theline+1;
	if (!sscanf(ptr, "%02x", &len)) return 0;
	ptr += 2;
	if ( strlen(theline) < (11 + (len * 2)) ) return 0;
	if (!sscanf(ptr, "%04x", addr)) return 0;
	ptr += 4;
	  /* printf("Line: length=%d Addr=%d\n", len, *addr); */
	if (!sscanf(ptr, "%02x", code)) return 0;
	ptr += 2;
	sum = (len & 255) + ((*addr >> 8) & 255) + (*addr & 255) + (*code & 255);
	while(*num != len) {
		if (!sscanf(ptr, "%02x", &bytes[*num])) return 0;
		ptr += 2;
		sum += bytes[*num] & 255;
		(*num)++;
		if (*num >= 256) return 0;
	}
	if (!sscanf(ptr, "%02x", &cksum)) return 0;
	if ( ((sum & 255) + (cksum & 255)) & 255 ) return 0; /* checksum error */
	return 1;
}

uint32_t KT_BinIO::Read(char *pszInput)
{
    //if ends with .hex
    int inputFilenameLen = strlen(pszInput);
    if ( (inputFilenameLen>4) && (strncmp(&pszInput[inputFilenameLen-4], ".hex", 4) == 0)){
        printf("Load file as hex\n");   //https://www.pjrc.com/tech/8051/ihex.c
        FILE *f;
        f = fopen(pszInput, "r");
        if (f == NULL) {
            return 0;
        }
        char line[1000];
        FILE *fin;
        int addr, n, status, bytes[256];
        int total=0, lineno=1;
        int minaddr=65536, maxaddr=0;
        while (!feof(f) && !ferror(f)) {
            line[0] = '\0';
            fgets(line, 1000, f);
            if (line[strlen(line)-1] == '\n') line[strlen(line)-1] = '\0';
            if (line[strlen(line)-1] == '\r') line[strlen(line)-1] = '\0';
            if (parse_hex_line(line, bytes, &addr, &n, &status)) {
                if (status == 0) {  // data 
                    for(int i=0; i<=(n-1); i++) {
                        if (addr>=u32Size) {
                            printf("Hex file too big, %d\n",addr);
                            return 0;
                        }
                        pReadBuff[addr] = bytes[i] & 255;
                        total++;
                        if (addr < minaddr) minaddr = addr;
                        if (addr > maxaddr) maxaddr = addr;
                        addr++;
                    }
                }
                if (status == 1) {  // end of file 
                    fclose(f);
                    printf("   Loaded %d bytes between:", total);
                    printf(" %04X to %04X\n", minaddr, maxaddr);
                    break;
                }
                if (status == 2) {  // begin of file 
                }
            } else {
                printf("   Error: '%s', line: %d\n", pszInput, lineno);
            }
            lineno++;
        }
        u32Size = maxaddr + 1;
    }else{
        FILE *f;
        f = fopen(pszInput, "rb");
        if (f == NULL) {
            return 0;
        }
        fseek (f , 0 , SEEK_END);
        uint32_t lSize;
        lSize = ftell (f);
        rewind (f);
        if (fread(pReadBuff, 1, lSize, f) != lSize) {
            fclose(f);
            return 0;
        }
        fclose(f);
        u32Size = lSize;
    }
    
    return 1;
}

uint32_t KT_BinIO::InitBuffer()
{
    pReadBuff = (uint8_t*)malloc(u32Size);
    if (pReadBuff == NULL) {
        return 0;
    }
    memset(pReadBuff, 0xFF, u32Size);
    pWriteBuff = (uint8_t*)malloc(u32Size);
    if (pWriteBuff == NULL) {
        return 0;
    }
    memset(pWriteBuff, 0xFF, u32Size);
    return 1;
}
void KT_BinIO::FreeBuffer()
{
    free(pReadBuff);
    free(pWriteBuff);
}
