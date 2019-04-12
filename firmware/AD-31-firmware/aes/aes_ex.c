/*
 * Advanced Encryption Standard
 * @author Dani Huertas
 * @email huertas.dani@gmail.com
 *
 * Based on the document FIPS PUB 197
 * pkcs7padding
 */

#include "aes_ex.h"
#include "string.h"
static uint8_t *aes_expansion_key;

/*key定义，128bit：16个字节，192bit：24个字节，256bit：32个字节*/
static  uint8_t aes_key[] = "FW2VN#N8DAL147L*";

static  uint8_t aes_vi[] = "2398DHY433UGFKL1X";
void aes_init_ex(void){
	aes_expansion_key = aes_init(sizeof(aes_key));
	aes_key_expansion(aes_key, aes_expansion_key);
}

uint8_t* aes_get_expansion_key(void){
	return aes_expansion_key;
}
uint8_t* aes_get_vi(void){
	return aes_vi;
}
static void xor_witch_iv(uint8_t* buf, uint8_t* iv){
 	uint8_t i;
  	for (i = 0; i < AES_EN / 8 ; ++i)  {// The block in AES is always 128bit no matter the key size
    	buf[i] ^= iv[i];
  	}
}
/*
 *   AES cbc cipher
 */
void aes_cbc_cipher(uint8_t* in, uint8_t* out, uint8_t* expansion_key, uint8_t* iv){  	
  	if(iv == NULL || expansion_key == NULL){
		//DBG_ERR("aes iv and expansion_key NULL\r");
	}
	else{		
    	xor_witch_iv(in, iv);
    	aes_cipher(in, out, expansion_key);
        for(uint8_t i = 0; i < 16; i++){
            iv[i] = out[i];  
        }
	}  	
}
/*
 *   aes cbc inv_cipher
 */
void aes_cbc_inv_cipher(uint8_t* in, uint8_t* out, uint8_t* expansion_key, uint8_t* iv){
  	if(iv == NULL || expansion_key == NULL){
		//DBG_ERR("aes iv and expansion_key NULL\r");
	}
	else{		
		aes_inv_cipher(in, out, expansion_key);
    	xor_witch_iv(out, iv);
		for(uint8_t i = 0; i < 16; i++){
            iv[i] = in[i];  
        }  
	}  	
}

void aes_cbc_cipher_buff(uint8_t* buff,uint16_t len, uint8_t* expansion_key, uint8_t* iv){
	
	if(iv == NULL || expansion_key == NULL){
		//DBG_ERR("aes iv and expansion_key NULL\r");
	}
	else{
		uint8_t ivtemp[16];
		uint8_t outtemp[16];
		uint8_t *p = NULL;
		p =  buff;
		memcpy(ivtemp, iv, 16);
		for(uint8_t i = 0; i < len / 16; i++){
			aes_cbc_cipher(p, outtemp, expansion_key, ivtemp);
			memcpy(p, outtemp, 16);
			p += 16;
			memcpy(ivtemp, outtemp, 16);
		}
	}	
}

void aes_cbc_inv_cipher_buff(uint8_t* buff,uint16_t len, uint8_t* expansion_key, uint8_t* iv){
	
	if(iv == NULL || expansion_key == NULL){
		//DBG_ERR("aes iv and expansion_key NULL\r");
	}
	else{
		uint8_t ivtemp[16];
		uint8_t outtemp[16];
		uint8_t *p = NULL;
		p =  buff;
		memcpy(ivtemp, iv, 16);
		for(uint8_t i = 0; i < len / 16; i++){
			aes_cbc_inv_cipher(p, outtemp, expansion_key, ivtemp);
			memcpy(ivtemp, p, 16);
			memcpy(p, outtemp, 16);
			p += 16;			
		}
	}	
}