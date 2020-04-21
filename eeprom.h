#ifndef eeprom
#define eeprom
void initEeprom();
void writeEeprom(uint16_t add, uint32_t data);
uint32_t readEeprom(uint16_t add);
#endif
