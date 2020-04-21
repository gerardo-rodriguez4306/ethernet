#ifndef STR_H
#define STR_H
char* htoa(uint8_t src, char* dest);
char* itoa(uint16_t src, char* dest);
char* strcpy(const char *src, char* dest);
uint8_t strlen(const char* str);
int strcmp(const char* str1, const char* str2);
uint16_t atoi(const char* str);
#endif
