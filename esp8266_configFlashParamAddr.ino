typedef enum : byte { myWriteAPIKey, awsApiGwHost } configFlashParam;

const uint32_t configFlashStartAddr = (ESP.getSketchSize() + FLASH_SECTOR_SIZE - 1) & (~(FLASH_SECTOR_SIZE - 1));

void loadConfig(bool force) {
  ESP.flashEraseSector(configFlashStartAddr / FLASH_SECTOR_SIZE);

  configFlashParamWrite(myWriteAPIKey, json[F("t")] | String().c_str());

  char *awsApiGwHostChar = (char *)malloc(MAX_API_HOSTNAME_LEN);
  if (awsApiGwHostChar) {
    strlcpy(awsApiGwHostChar, json[FPSTR(rStr)][0] | String().c_str(), MAX_API_HOSTNAME_LEN);
    configFlashParamWrite(awsApiGwHost, awsApiGwHostChar, MAX_API_HOSTNAME_LEN);
    free(awsApiGwHostChar);
  } else configFlashParamWrite(awsApiGwHost, String().c_str());
}

uint32_t configFlashParamAddr(const configFlashParam num) {
  if (num & 0xc0) return 0x40010000;
  uint32_t configFlashParamTable;
  ESP.flashRead(configFlashStartAddr + 8 + ((num >> 1) << 2), &configFlashParamTable, sizeof(configFlashParamTable));
  if (num & 0x1) configFlashParamTable >>= 16;
  if ((configFlashParamTable & 0xffff) == 0xffff) return 0x40010000;
  return 0x40200000 + configFlashStartAddr + ((configFlashParamTable & 0x0000ffc0) >> 4);
}

void configFlashParamWrite(const configFlashParam num, const char *writeParam) { configFlashParamWrite(num, writeParam, 0); }
void configFlashParamWrite(const configFlashParam num, const char *writeParam, uint16_t size) {
  if (num & 0xc0) return;
  uint16_t paramSize;
  if (size) paramSize = (size + 3) & (~3);
  else paramSize = (strlen(writeParam) + 4) & (~3);
  if (paramSize & 0xff00) return;
  uint16_t nextAddr = 8 + 128;
  uint32_t configFlashParamTable;
  for (uint8_t i = 0; i < 32; i++) {
    ESP.flashRead(configFlashStartAddr + 8 + (i << 2), &configFlashParamTable, sizeof(configFlashParamTable));
    for (uint8_t j = 0; j < 2; j++) {
      if ((configFlashParamTable & 0x0000ffff) != 0x0000ffff) {
        if (((i << 1) | j) == num) return;
        if (nextAddr < (((uint16_t)configFlashParamTable & 0xffc0) >> 4) + (((uint16_t)configFlashParamTable & 0x003f) << 2))
            nextAddr = (((uint16_t)configFlashParamTable & 0xffc0) >> 4) + (((uint16_t)configFlashParamTable & 0x003f) << 2);
      }
      configFlashParamTable >>= 16;
    }
  }
  if (((nextAddr + paramSize) & 0xf000) || (nextAddr + paramSize) > FLASH_SECTOR_SIZE) return;
  configFlashParamTable = 0xffff0000 | ((uint32_t)nextAddr << 4) | (uint32_t)(paramSize >> 2);
  if (num & 0x1) configFlashParamTable = (configFlashParamTable << 16) | 0x0000ffff;
  ESP.flashWrite(configFlashStartAddr + 8 + (uint32_t)((num >> 1) << 2), &configFlashParamTable, sizeof(configFlashParamTable));
  for (uint16_t i = 0; i < (paramSize >> 2); i++) {
    uint32_t writeParamUint32t = (uint32_t)writeParam[(i << 2) + 3] << 24 | (uint32_t)writeParam[(i << 2) + 2] << 16 | (uint32_t)writeParam[(i << 2) + 1] << 8 | (uint32_t)writeParam[i << 2];
    ESP.flashWrite(configFlashStartAddr + nextAddr + (i << 2), &writeParamUint32t, sizeof(writeParamUint32t));
  }
}
