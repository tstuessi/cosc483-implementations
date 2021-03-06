/* RSA Key generator 
 *
 * This file is based around the openssl BIGNUM class
 *
 * Tyler Stuessi
 * Jeremy Herwig
 */

#include <fstream>
#include <iostream>
#include <openssl/bn.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <string.h>


#include "hex_string_conv.h"
#include "rsa_signature.h"
#include "rsa_keygen.h"

#define NUM_TESTS 12

typedef unsigned int UINT;
typedef unsigned char UCHAR;

using namespace std;

//generate an n bit prime number
BIGNUM * generate_prime(int n) {
	//vars
	bool found;
	BIGNUM *prime;
	BIGNUM *a;
	BIGNUM *p_minus_one;
	BIGNUM *r;
	BIGNUM *one;
	BIGNUM *three;

	BN_CTX *ctx;

	int i;

	prime = BN_new();
	a = BN_new();
	p_minus_one = BN_new();
	r = BN_new();
	three = BN_new();
	one = BN_new();

	BN_one(one);
	BN_zero(three);

	//initialize a BN with the value 3
	//its inherently annoying to do this
	for(i = 0; i < 3; i++) BN_add(three, three, one);

	ctx = BN_CTX_new();

	found = false;

	//use fermats primality test to test for primes
	while(!found) {
		//generate n random bits
		//3rd arg being 1 requires that the 2 most significant bits will be 1
		//4th arg being 1 requires that it is odd
		BN_rand(prime, n, 1, 1);

		//two is already taken care of, so check prime - 1
		BN_sub(p_minus_one, prime, one);

		//n and n-1 are always coprime so we just need to do 
		//the modular exponentiation
		//if a^(n-1) mod N != 1, then N is composite
		//so continue with the loop and generate a new prime
		BN_mod_exp(r, p_minus_one, p_minus_one, prime, ctx);
		if(BN_is_one(r) == 0) {
			continue;
		}
		//test four more values
		found = true;
		for(i = 0; i < NUM_TESTS; i++) {
			//generate random value between 3 and prime
			BN_sub(r, prime, three);
			BN_rand_range(a, r);
			BN_add(a, a, three);

			//check to see that it is a part of prime's multiplicative group
			BN_gcd(r, a, prime, ctx);
			if(BN_is_one(r) == 0) {
				//try another one 
				--i;
				continue;
			}

			//check if a^(n-1) == 1
			BN_mod_exp(r, a, p_minus_one, prime, ctx);
			if(BN_is_one(r) == 0) {
				found = false;
				break;
			}
		}
	}

	BN_clear_free(r);
	BN_clear_free(a);
	BN_clear_free(p_minus_one);
	BN_clear_free(one);
	BN_clear_free(three);

	BN_CTX_free(ctx);

	return prime;
}


//working with the BIGNUM class is a real pain,
//but it makes a ton of things easier
void rsa_keygen(UINT security_param, string keyfile, string identity, string sigfile) {
	//vars
	BIGNUM *p;
	BIGNUM *q;
	BIGNUM *N;
	BIGNUM *p_minus_one;
	BIGNUM *q_minus_one;
	BIGNUM *order;
	BIGNUM *one;
	BIGNUM *i;
	BIGNUM *e;
	BIGNUM *d;
	BIGNUM *gcd;

	BN_CTX *ctx;

	int num_bytes_N, num_bytes_e, num_bytes_d;

	ofstream fout;

	//p and q are generated by generate_prime
	N = BN_new();
	p_minus_one = BN_new();
	q_minus_one = BN_new();
	order = BN_new();
	e = BN_new();
	d = BN_new();
	gcd = BN_new();
	one = BN_new();
	i = BN_new();

	BN_one(one);

	ctx = BN_CTX_new();
	
	//generate two prime numbers that are security_params
	RAND_poll();
	p = generate_prime(security_param);
	q = generate_prime(security_param);

	//multiply p and q together to get N
	BN_mul(N, p, q, ctx);

	//debugging
	char *temp = BN_bn2hex(N);
	OPENSSL_free(temp);

	//get the order of Zn*
	BN_sub(p_minus_one, p, one);
	BN_sub(q_minus_one, q, one);
	BN_mul(order, p_minus_one, q_minus_one, ctx);

	//starting with x=1, check gcd(2^x + 1, order) == 1
	//basically look through all fermat primes
	//after this loop executes, e will be coprime with the order
	BN_one(i);
	while(1) {
		BN_lshift1(i, i);
		BN_add(e, i, one);
		BN_gcd(gcd, e, order, ctx);

		if (BN_is_one(gcd)) {
			break;
		}
	}

	//compute d
	BN_mod_inverse(d, e, order, ctx);

	//convert to printable hex
	char *N_string = BN_bn2hex(N);
	char *e_string = BN_bn2hex(e);
	char *d_string = BN_bn2hex(d);

	//free everything you don't need for file i/o
	BN_clear_free(N);
	BN_clear_free(d);
	BN_clear_free(e);
	BN_clear_free(i);
	BN_clear_free(p);
	BN_clear_free(q);
	BN_clear_free(p_minus_one);
	BN_clear_free(q_minus_one);
	BN_clear_free(one);
	BN_clear_free(order);
	BN_clear_free(gcd);


	fout.open((keyfile + ".pub").c_str());
	fout << identity << endl;
	fout << security_param << endl;
	fout << N_string << endl;
	fout << e_string << endl;
	fout.close();

	fout.open(keyfile.c_str());
	fout << identity << endl;
	fout << security_param << endl;
	fout << N_string << endl;
	fout << d_string << endl;
	fout.close();

	OPENSSL_free(N_string);
	OPENSSL_free(e_string);
	OPENSSL_free(d_string);

	if (sigfile == "None") {
		sigfile = keyfile;
	} 


	/* Bad way to get around the fact that sign takes a hex encoded file */

	/* Recreate the contents of the file in a string */
	string txt, hex;
	UCHAR *bin;
	txt = identity + "\n" + to_string(security_param) + "\n" + string(N_string) + "\n" + string(d_string) + "\n";

	bin = (UCHAR *) malloc(txt.size());
	memcpy(bin, txt.c_str(), txt.size());

	hex;
	binary_to_hex(bin, txt.size(), hex);

	/* Write the hex rep to a hex encoded file */
	fout.open((keyfile + ".hex").c_str());
	fout << hex;
	fout.close();

	sign(sigfile, keyfile + ".hex", keyfile + ".sig");
}


