/*
 * Advanced Encryption Standard
 * @author Dani Huertas
 * @email huertas.dani@gmail.com
 *
 * Based on the document FIPS PUB 197
 */
#include "aes.h"
#define AES_EN 0 //aes使能，128：128bit aes；192：192bit aes；256：256bit aes
void aes_init_ex(void);

uint8_t* aes_get_expansion_key(void);
uint8_t* aes_get_vi(void);
void aes_cbc_cipher(uint8_t* in, uint8_t* out, uint8_t* expansion_key, uint8_t* iv);
void aes_cbc_inv_cipher(uint8_t* in, uint8_t* out, 
                        uint8_t* expansion_key, uint8_t* iv);
void aes_cbc_cipher_buff(uint8_t* buff,uint16_t len, uint8_t* expansion_key, uint8_t* iv);
void aes_cbc_inv_cipher_buff(uint8_t* buff,uint16_t len, uint8_t* expansion_key, uint8_t* iv);