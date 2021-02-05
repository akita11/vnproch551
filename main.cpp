#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>
#include "KT_BinIO.h"
#include "KT_ProgressBar.h"
#include "serial.h"
#include <unistd.h>

KT_BinIO ktFlash;

uint8_t u8Buff[64];
uint8_t u8Mask[8];
uint16_t u16WrittenAddr;

/* Detect MCU */
uint8_t u8DetectCmd[64] = {
	0xA1, 0x12, 0x00, 0x00, 0x11, 0x4D, 0x43, 0x55,
	0x20, 0x49, 0x53, 0x50, 0x20, 0x26, 0x20, 0x57,
	0x43, 0x48, 0x2e, 0x43, 0x4e
};
uint8_t u8DetectRespond = 6;

/* Get Bootloader Version, Chip ID */
uint8_t u8IdCmd[64] = {
	0xA7, 0x02, 0x00, 0x1F, 0x00
};
uint8_t u8IdRespond = 30;

/* Current CH55x device */
uint8_t u8DeviceID = 0;
uint8_t u8FamilyID = 0;

/* Enable ISP */
uint8_t u8InitCmd[64] = {
	0xA8, 0x0E, 0x00, 0x07, 0x00, 0xFF, 0xFF, 0xFF,
	0xFF, 0x03, 0x00, 0x00, 0x00, 0xFF, 0x52, 0x00,
	0x00
};
uint8_t u8InitRespond = 6;

/* Set Flash Address, mask protected*/
uint8_t u8AddessCmd[64] = {
	0xA3, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00
};
uint8_t u8AddessRespond = 6;

/* Erase ??? */
uint8_t u8EraseCmd[64] = {
	0xA4, 0x01, 0x00, 0x08
};
uint8_t u8EraseRespond = 6;

/* Reset */
uint8_t u8ResetCmd[64] = {
	0xA2, 0x01, 0x00, 0x01 /* if 0x00 not run, 0x01 run*/
};
uint8_t u8ResetRespond = 6;

/* Write, mask protected*/
uint8_t u8WriteCmd[64] = {
	0xA5, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	/* byte 4 Low Address (first = 1) */
	/* byte 5 High Address */
};
uint8_t u8WriteRespond = 6;

/* Verify */
uint8_t u8VerifyCmd[64] = {
	0xA6, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	/* byte 4 Low Address (first = 1) */
	/* byte 5 High Address */
};
uint8_t u8VerifyRespond = 6;

uint8_t u8ReadCmd[64] = {
	0x00
};
uint8_t u8ReadRespond = 6;

libusb_device_handle *h;

uint32_t Write(uint8_t *p8Buff, uint8_t u8Length);
uint32_t Read(uint8_t *p8Buff, uint8_t u8Length);

uint32_t Write(uint8_t *p8Buff, uint8_t u8Length)
{
	int len;
	if (libusb_bulk_transfer(h, 0x02, (unsigned char*)p8Buff, u8Length, &len, 5000) != 0) {
		return 0;
	} else {
		return 1;
	}
	return 0;
}

uint32_t WriteSerial(union filedescriptor *fd, uint8_t *p8Buff, uint8_t u8Length)
{
    uint8_t serialBuf[64+3];
    serialBuf[0] = 0x57;
    serialBuf[1] = 0xAB;
    memcpy(&serialBuf[2],p8Buff,u8Length);
    int sum = 0;
    for (int i = 0; i<(u8Length+2);i++){
        sum+=serialBuf[i];
    }
    serialBuf[u8Length+2] = (sum-2)&0xFF;
    serial_send(fd,serialBuf,u8Length+3);
    return 1;
}

uint32_t Read(uint8_t *p8Buff, uint8_t u8Length)
{
	int len;
	if (libusb_bulk_transfer(h, 0x82, (unsigned char*)p8Buff, u8Length, &len, 5000) != 0) {
		return 0;
	} else {
		return 1;
	}
	return 0;
}

uint32_t ReadSerial(union filedescriptor *fd, uint8_t *p8Buff, uint8_t u8Length)
{
    uint8_t serialBuf[64+3];
    serial_recv(fd,serialBuf,u8Length+3);
    if (serialBuf[0] != 0x55) return 0;
    if (serialBuf[1] != 0xaa) return 0;
    int sum = 0;
    for (int i = 0; i<(u8Length+2);i++){
        sum+=serialBuf[i];
    }
    sum = sum&0xFF;
    if (serialBuf[u8Length+2] != ((sum+1)&0xFF)) return 0;
    memcpy(p8Buff,&serialBuf[2],u8Length);
    return 1;
}


int main(int argc, char const *argv[])
{
	uint32_t i;
	KT_BinIO ktBin;
	KT_ProgressBar ktProg;
	uint8_t chipType;
    bool usingSerial;

	printf("------------------------------------------------------------------\n");
	printf("CH55x Programmer by VNPro\n");
	printf("------------------------------------------------------------------\n");
	if (argc != 2 && argc != 3) {
		printf("usage: vnproch55x flash_file.bin\n");
		printf("------------------------------------------------------------------\n");
        return 1;
	}
    /* load flash file */
	ktBin.u32Size = 63 * 1024;	//make it super big for initialization, big enough to hold CH559 size
	ktBin.InitBuffer();
    
    char *fileName = NULL;
    usingSerial = false;
    char *serialName = NULL;
    union filedescriptor serialFd;
    
    for (int i=1;i<argc;i++){
        if ( ((char*)argv[i])[0] != '-' ){
            fileName = (char*)argv[i];
        }else{
            if ( ((char*)argv[i])[1] == 's' ){
                usingSerial = true;
                serialName = &(((char*)argv[i])[2]);
            }
        }
    }
    
	if (!ktBin.Read(fileName)) {
		printf("Read file: ERROR\n");
		return 0;
	}

	libusb_init(NULL);
	
    if (usingSerial){
        printf("Using Serial %s\n",serialName);
        
        if (serial_open(serialName, 57600, &serialFd)==-1) {
            printf("Serial open failed\n");
            return 1;
        }
        
        /* Clear DTR and RTS to unload the RESET capacitor
         * (for example in Arduino) */
        serial_set_dtr_rts(&serialFd, 0);
        usleep(50*1000);
        /* Set DTR and RTS back to high */
        serial_set_dtr_rts(&serialFd, 1);
        usleep(50*1000);
        
        serial_drain(&serialFd,0);
    }else{
        h = libusb_open_device_with_vid_pid(NULL, 0x4348, 0x55e0);
        
        if (h == NULL) {
            printf("Found no CH55x USB\n");
            return 1;
        }
        
        struct libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(libusb_get_device(h), &desc) >= 0 ) {
            printf("DeviceVersion of CH55x: %d.%02d \n", ((desc.bcdDevice>>12)&0x0F)*10+((desc.bcdDevice>>8)&0x0F),((desc.bcdDevice>>4)&0x0F)*10+((desc.bcdDevice>>0)&0x0F));
        }

        libusb_claim_interface(h, 0);
    }
	
	/* Detect MCU */
    if (usingSerial){
        if (!WriteSerial(&serialFd, u8DetectCmd, u8DetectCmd[1] + 3)) {
            printf("Send Detect: Fail\n");
            serial_close(&serialFd);
            return 1;
        }
        
        if (!ReadSerial(&serialFd, u8Buff, u8DetectRespond)) {
            printf("Read Detect: Fail\n");
            serial_close(&serialFd);
            return 1;
        }
    }else{
        if (!Write(u8DetectCmd, u8DetectCmd[1] + 3)) {
            printf("Send Detect: Fail\n");
            return 1;
        }
        
        if (!Read(u8Buff, u8DetectRespond)) {
            printf("Read Detect: Fail\n");
            return 1;
        }
    }

	/* Store refrence to MCU device ID */
	u8DeviceID = u8Buff[4];
    u8FamilyID = u8Buff[5];
    
    printf("MCU ID: %02X %02X\n", u8Buff[4], u8Buff[5]);

	/* Check MCU series/family? ID */
	if (u8FamilyID == 0x11) {
        /* Check MCU ID */
        if (
            (u8DeviceID != 0x51) &&
            (u8DeviceID != 0x52) &&
            (u8DeviceID != 0x54) &&
            (u8DeviceID != 0x58) &&
            (u8DeviceID != 0x59)
            ) {
            printf("Device not supported 0x%x\n", u8DeviceID);
            return 1;
        }else{
            printf("Found Device CH5%x\n", u8DeviceID);
        }
    }else if (u8FamilyID == 0x12) {
        //todo: check MCU ID
    }else{
		printf("Not support, family ID.\n");
		return 1;
	}

	/* Bootloader and Chip ID */
    if (usingSerial){
        if (!WriteSerial(&serialFd, u8IdCmd, u8IdCmd[1] + 3)) {
            printf("Send ID: Fail\n");
            serial_close(&serialFd);
            return 1;
        }
        
        if (!ReadSerial(&serialFd, u8Buff, u8IdRespond)) {
            printf("Send ID: Fail\n");
            serial_close(&serialFd);
            return 1;
        }
    }else{
        if (!Write(u8IdCmd, u8IdCmd[1] + 3)) {
            printf("Send ID: Fail\n");
            return 1;
        }
        
        if (!Read(u8Buff, u8IdRespond)) {
            printf("Read ID: Fail\n");
            return 1;
        }
    }
    
	printf("Bootloader: %d.%d.%d\n", u8Buff[19], u8Buff[20], u8Buff[21]);
    if (u8FamilyID == 0x11) {
        printf("ID: %02X %02X %02X %02X\n", u8Buff[22], u8Buff[23], u8Buff[24], u8Buff[25]);
    }else if (u8FamilyID == 0x12) {
        printf("ID: %02X %02X %02X %02X %02X %02X %02X %02X\n", u8Buff[22], u8Buff[23], u8Buff[24], u8Buff[25], u8Buff[26], u8Buff[27], u8Buff[28], u8Buff[29]);
    }
    
	/* check bootloader version */
	if ( ((u8Buff[19] != 0x02) || (u8Buff[20] != 0x03) || (u8Buff[21] != 0x01)) && ((u8Buff[19] != 0x02) || (u8Buff[20] != 0x04) || (u8Buff[21] != 0x00)) ){
		printf("Not support, Bootloader version.\n");
		return 1;
	}
	/* Calc XOR Mask */

	uint8_t u8Sum;

    if (u8FamilyID == 0x11) {
        u8Sum = u8Buff[22] + u8Buff[23] + u8Buff[24] + u8Buff[25];
    }else if (u8FamilyID == 0x12) {
        u8Sum = u8Buff[22] + u8Buff[23] + u8Buff[24] + u8Buff[25] + u8Buff[26] + u8Buff[27] + u8Buff[28] + u8Buff[29];
    }
	for (i = 0; i < 8; ++i) {
		u8Mask[i] = u8Sum;
	}
	u8Mask[7] += u8DeviceID;
	printf("XOR Mask: ");
	for (i = 0; i < 8; ++i) {
		printf("%02X ", u8Mask[i]);
	}
	printf("\n");

	/* init or erase ??? */
    if (usingSerial){
        if (!WriteSerial(&serialFd, u8InitCmd, u8InitCmd[1] + 3)) {
            printf("Send Init: Fail\n");
            serial_close(&serialFd);
            return 1;
        }
        
        if (!ReadSerial(&serialFd, u8Buff, u8InitRespond)) {
            printf("Read Init: Fail\n");
            serial_close(&serialFd);
            return 1;
        }
    }else{
        if (!Write(u8InitCmd, u8InitCmd[1] + 3)) {
            printf("Send Init: Fail\n");
            return 1;
        }
        
        if (!Read(u8Buff, u8InitRespond)) {
            printf("Read Init: Fail\n");
            return 1;
        }
    }
	

	/* Bootloader and Chip ID */
    if (usingSerial){
        if (!WriteSerial(&serialFd, u8IdCmd, u8IdCmd[1] + 3)) {
            printf("Send ID: Fail\n");
            serial_close(&serialFd);
            return 1;
        }
        
        if (!ReadSerial(&serialFd, u8Buff, u8IdRespond)) {
            printf("Read ID: Fail\n");
            serial_close(&serialFd);
            return 1;
        }
    }else{
        if (!Write(u8IdCmd, u8IdCmd[1] + 3)) {
            printf("Send ID: Fail\n");
            return 1;
        }
        
        if (!Read(u8Buff, u8IdRespond)) {
            printf("Read ID: Fail\n");
            return 1;
        }
    }
    
    /* get configuration */
    if (u8FamilyID == 0x12) {
        /* Read Boot Option */
        uint8_t u8ReadOptionCmd[64] = {
            0xA7, 0x02, 0x00, 0x1F, 0x00
        };
        uint8_t u8ReadOptionRespond = 30;
        
        //A8 set configuration, may be necessary for CH549
        /* Write Boot Option */
        uint8_t u8WriteOptionCmd[64] = {
            0xa8, 0x0e, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xd2, 0x00, 0x00
        };
        uint8_t u8WriteOptionRespond = 6;
        //todo: add serial
        if (!Write(u8ReadOptionCmd, u8ReadOptionCmd[1] + 3)) {
            printf("Send Read Option: Fail\n");
            return 1;
        }
        
        if (!Read(u8Buff, u8ReadOptionRespond)) {
            printf("Read Read Option: Fail\n");
            return 1;
        }
        
        /*printf("CONFIG: ");
        for (int i=0;i<30;i++){
            printf("%02X ",u8Buff[i]);
        }
        printf("\n");*/
        
        if (!Write(u8WriteOptionCmd, u8WriteOptionCmd[1] + 3)) {
            printf("Send Write Option: Fail\n");
            return 1;
        }
        
        if (!Read(u8Buff, u8WriteOptionRespond)) {
            printf("Read Write Option: Fail\n");
            return 1;
        }
        
        if (!Write(u8ReadOptionCmd, u8ReadOptionCmd[1] + 3)) {
            printf("Send Read Option: Fail\n");
            return 1;
        }
        
        if (!Read(u8Buff, u8ReadOptionRespond)) {
            printf("Read Read Option: Fail\n");
            return 1;
        }
        
        /*printf("CONFIG: ");
        for (int i=0;i<30;i++){
            printf("%02X ",u8Buff[i]);
        }
        printf("\n");*/
    }

	/* Set Flash Address to 0 */
    if (usingSerial){
        if (!WriteSerial(&serialFd, u8AddessCmd, u8AddessCmd[1] + 3)) {
            printf("Send Address: Fail\n");
            serial_close(&serialFd);
            return 1;
        }
        
        if (!ReadSerial(&serialFd, u8Buff, u8AddessRespond)) {
            printf("Read Address: Fail\n");
            serial_close(&serialFd);
            return 1;
        }
    }else{
        if (!Write(u8AddessCmd, u8AddessCmd[1] + 3)) {
            printf("Send Address: Fail\n");
            return 1;
        }
        
        if (!Read(u8Buff, u8AddessRespond)) {
            printf("Read Address: Fail\n");
            return 1;
        }
    }

	/* Erase or unknow */
    if (usingSerial){
        if (!WriteSerial(&serialFd, u8EraseCmd, u8EraseCmd[1] + 3)) {
            printf("Send Erase: Fail\n");
            serial_close(&serialFd);
            return 1;
        }
        
        if (!ReadSerial(&serialFd, u8Buff, u8EraseRespond)) {
            printf("Read Erase: Fail\n");
            serial_close(&serialFd);
            return 1;
        }
    }else{
        if (!Write(u8EraseCmd, u8EraseCmd[1] + 3)) {
            printf("Send Erase: Fail\n");
            return 1;
        }
        
        if (!Read(u8Buff, u8EraseRespond)) {
            printf("Read Erase: Fail\n");
            return 1;
        }
    }

	/* Write */
	printf("Write %d bytes from bin file.\n",ktBin.u32Size);
	/* Progress */
	uint32_t writeDataSize,totalPackets,lastPacketSize;
	writeDataSize = ktBin.u32Size;
	totalPackets = (writeDataSize+55) / 56;
	lastPacketSize = writeDataSize % 56;
	//make it multiple of 8
	lastPacketSize = (lastPacketSize+7)/8*8;
	if (lastPacketSize==0) lastPacketSize = 56;
	ktProg.SetMax(totalPackets);
	ktProg.SetNum(50);
	ktProg.SetPos(0);
	ktProg.Display();

	for (i = 0; i < totalPackets; ++i) {
		uint16_t u16Tmp;
		uint32_t j;
		/* Write flash */
		memmove(&u8WriteCmd[8], &ktBin.pReadBuff[i * 0x38], 0x38);
		for (j = 0; j < 7; ++j) {
			uint32_t ii;
			for (ii = 0; ii < 8; ++ii) {
				u8WriteCmd[8 + j * 8 + ii] ^= u8Mask[ii];
			}
		}
		u16Tmp = i * 0x38;
		u8WriteCmd[1] = 0x3D - (i<(totalPackets-1)?0:(56-lastPacketSize));	//last packet can be smaller
		u8WriteCmd[3] = (uint8_t)u16Tmp;
		u8WriteCmd[4] = (uint8_t)(u16Tmp >> 8);
        u16WrittenAddr = u16Tmp + u8WriteCmd[1] - 5;
        if (usingSerial){
            if (!WriteSerial(&serialFd, u8WriteCmd, u8WriteCmd[1] + 3)) {
                printf("Send Write: Fail\n");
                serial_close(&serialFd);
                return 1;
            }
            
            if (!ReadSerial(&serialFd, u8Buff, u8WriteRespond)) {
                printf("Read Write: Fail\n");
                serial_close(&serialFd);
                return 1;
            }
        }else{
            if (!Write(u8WriteCmd, u8WriteCmd[1] + 3)) {
                printf("Send Write: Fail\n");
                return 1;
            }
            
            if (!Read(u8Buff, u8WriteRespond)) {
                printf("Read Write: Fail\n");
                return 1;
            }
        }
		ktProg.SetPos(i + 1);
		ktProg.Display();
	}
    
    if (u8FamilyID == 0x12) {   //seems an end packet is necessary for CH549
        u8WriteCmd[1] = 0x05;
        u8WriteCmd[3] = (uint8_t)u16WrittenAddr;
        u8WriteCmd[4] = (uint8_t)(u16WrittenAddr >> 8);
        if (usingSerial){
            if (!WriteSerial(&serialFd, u8WriteCmd, u8WriteCmd[1] + 3)) {
                printf("Send Write: Fail\n");
                serial_close(&serialFd);
                return 1;
            }
            
            if (!ReadSerial(&serialFd, u8Buff, u8WriteRespond)) {
                printf("Read Write: Fail\n");
                serial_close(&serialFd);
                return 1;
            }
        }else{
            if (!Write(u8WriteCmd, u8WriteCmd[1] + 3)) {
                printf("Send Write: Fail\n");
                return 1;
            }
            
            if (!Read(u8Buff, u8WriteRespond)) {
                printf("Read Write: Fail\n");
                return 1;
            }
        }
    }

	printf("\n");
	printf("Write complete!!!\n");
	printf("Verify chip\n");
	
	/* Set Flash Address to 0 */
    if (usingSerial){
        if (!WriteSerial(&serialFd, u8AddessCmd, u8AddessCmd[1] + 3)) {
            printf("Send Address: Fail\n");
            serial_close(&serialFd);
            return 1;
        }
        
        if (!ReadSerial(&serialFd, u8Buff, u8AddessRespond)) {
            printf("Read Address: Fail\n");
            serial_close(&serialFd);
            return 1;
        }
    }else{
        if (!Write(u8AddessCmd, u8AddessCmd[1] + 3)) {
            printf("Send Address: Fail\n");
            return 1;
        }
        
        if (!Read(u8Buff, u8AddessRespond)) {
            printf("Read Address: Fail\n");
            return 1;
        }
    }
	
	//just change A5 packet to A6
	ktProg.SetPos(0);
	ktProg.Display();

	for (i = 0; i < totalPackets; ++i) {
		uint16_t u16Tmp;
		uint32_t j;
		/* Verify flash */
		memmove(&u8VerifyCmd[8], &ktBin.pReadBuff[i * 0x38], 0x38);
		for (j = 0; j < 7; ++j) {
			uint32_t ii;
			for (ii = 0; ii < 8; ++ii) {
				u8VerifyCmd[8 + j * 8 + ii] ^= u8Mask[ii];
			}
		}
		u16Tmp = i * 0x38;
		u8VerifyCmd[1] = 0x3D - (i<(totalPackets-1)?0:(56-lastPacketSize));	//last packet can be smaller
		u8VerifyCmd[3] = (uint8_t)u16Tmp;
		u8VerifyCmd[4] = (uint8_t)(u16Tmp >> 8);
        
        if (usingSerial){
            if (!WriteSerial(&serialFd, u8VerifyCmd, u8VerifyCmd[1] + 3)) {
                printf("Send Verify: Fail\n");
                serial_close(&serialFd);
                return 1;
            }
            
            if (!ReadSerial(&serialFd, u8Buff, u8VerifyRespond)) {
                printf("Read Verify: Fail\n");
                serial_close(&serialFd);
                return 1;
            }
        }else{
            if (!Write(u8VerifyCmd, u8VerifyCmd[1] + 3)) {
                printf("Send Verify: Fail\n");
                return 1;
            }
            
            if (!Read(u8Buff, u8VerifyRespond)) {
                printf("Read Verify: Fail\n");
                return 1;
            }
        }
        
		if (u8Buff[4]!=0 || u8Buff[5]!=0){
			printf("\nPacket %d doesn't match.\n",i);
			return 1;
		}
		ktProg.SetPos(i + 1);
		ktProg.Display();
	}
	
	printf("\n");
	printf("Verify complete!!!\n");
	
	printf("------------------------------------------------------------------\n");
	
	/* Reset and Run */
    if (usingSerial){
        WriteSerial(&serialFd, u8ResetCmd, u8ResetCmd[1] + 3);
        ReadSerial(&serialFd, u8Buff, u8ResetRespond);
    }else{
        Write(u8ResetCmd, u8ResetCmd[1] + 3);
    }
		//printf("Send Reset: Fail\n");
		//return 1;
	//}

    if (usingSerial){
        serial_close(&serialFd);
    }

	return 0;
}
