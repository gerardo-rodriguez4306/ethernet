#include <stdbool.h>
#include <stdint.h>
#include "str.h"
char* htoa(uint8_t src, char* dest)//converts a single byte to a hex char
{                             //purpose: to convert dec mac addr to hex char
     uint8_t i = src;
     uint8_t q;
     uint8_t iterator = 0;
     while (i > 0)
     {
          q = i % 16;
          i/=16;
          if (q < 10)
               dest[iterator] = q + '0';
          else
               dest[iterator] = q + 'a' - 10;
          iterator++;
     }

     i = iterator;//i = sizeof dest
     iterator = 0;
     char tmp;
     while (iterator < i/2)
     {
          tmp = dest[iterator];
          dest[iterator] = dest[1];
          dest[1] = tmp;
          iterator++;
     }
     dest[i] = '\0';
     return dest;
}
char* itoa(uint16_t src, char* dest)
{
     //buffer should be five chars wide since 2^16 - 1 is 65535
     uint8_t i = src;
     uint8_t q;
     uint8_t iterator = 0;
     if (i == 0)
     {
         dest[0] = '0';
         dest[1] = 0;
         return dest;
     }
     while (i > 0)
     {
          q = i % 10;
          i/=10;
          dest[iterator] = q + '0';
          iterator++;
     }
     
    i = iterator - 1;//i = sizeof dest
    uint8_t len = i;
    iterator = 0;
    char tmp;
    while (iterator < i)
    {
         tmp = dest[iterator];
         dest[iterator] = dest[i];
         dest[i] = tmp;
         iterator++;
          i--;
    }
     dest[len + 1] = '\0';
     return dest;
}
char* strcpy(const char* src, char* dest)
{
  uint8_t i;
  for (i=0; src[i] != '\0'; i++)
    dest[i] = src[i];

  //Ensure trailing null byte is copied
  dest[i]= '\0';
  return dest;
}
uint8_t strlen(const char* str)
{
     uint8_t result = 0;
     while (str[++result] != '\0');
     return result;
}
int strcmp(const char* str1, const char* str2)
{
    uint8_t i = 0;
    if (strlen(str1) != strlen(str2)) return -1;
    while (str1[i] != '\0' && str2[i] != '\0')
    {
        if ( str1[i] < str2[i] )
            return -1;
        else if ( str1[i] > str2[i] )
            return 1;
        else
            i++;
    }
    return 0;
}
uint16_t atoi(const char* str)
{
    uint8_t i = 0;
    int result = 0;
    uint8_t length_of_string = strlen(str);
    int power_of_ten;
    if (str[0] == '-')
    {
        power_of_ten = -1;
        i++;
    }
    else
        power_of_ten = 1;
    while (i < length_of_string - 1)
    {
        power_of_ten *= 10;
        i++;
    }
    while (i < length_of_string)
    {
        result += (str[i] - 48) * 10;
        power_of_ten /= 10;
        i++;
    }
    return result;
}
