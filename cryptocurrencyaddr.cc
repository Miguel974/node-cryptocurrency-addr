#include <node.h>
#include <node_buffer.h>
#include <v8.h>

#include <stdint.h>
#include <string.h>
#include "portable_endian.h"

using namespace node;
using namespace v8;

#if NODE_MAJOR_VERSION >= 4

#define DECLARE_INIT(x) \
    void x(Local<Object> exports)

#define DECLARE_FUNC(x) \
    void x(const FunctionCallbackInfo<Value>& args)

#if NODE_MAJOR_VERSION >= 12
#define DECLARE_SCOPE \
    v8::Isolate* isolate = args.GetIsolate(); \
    Local<Context> currentContext = isolate->GetCurrentContext();
#else
#define DECLARE_SCOPE \
    v8::Isolate* isolate = args.GetIsolate();
#endif

#define SET_BUFFER_RETURN(x, len) \
    args.GetReturnValue().Set(Buffer::Copy(isolate, x, len).ToLocalChecked());

#define SET_BOOLEAN_RETURN(x) \
    args.GetReturnValue().Set(Boolean::New(isolate, x));

#define RETURN_EXCEPT(msg) \
    do { \
        isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, msg))); \
        return; \
    } while (0)

#else

#define DECLARE_INIT(x) \
    void x(Handle<Object> exports)

#define DECLARE_FUNC(x) \
    Handle<Value> x(const Arguments& args)

#define DECLARE_SCOPE \
    HandleScope scope

#define SET_BUFFER_RETURN(x, len) \
    do { \
        Buffer* buff = Buffer::New(x, len); \
        return scope.Close(buff->handle_); \
    } while (0)

#define SET_BOOLEAN_RETURN(x) \
    return scope.Close(Boolean::New(x));

#define RETURN_EXCEPT(msg) \
    return ThrowException(Exception::Error(String::New(msg)))

#endif // NODE_MAJOR_VERSION

static const int8_t charset_rev[128] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	15, -1, 10, 17, 21, 20, 26, 30,  7,  5, -1, -1, -1, -1, -1, -1,
	-1, 29, -1, 24, 13, 25,  9,  8, 23, -1, 18, 22, 31, 27, 19, -1,
	1,  0,  3, 16, 11, 28, 12, 14,  6,  4,  2, -1, -1, -1, -1, -1,
	-1, 29, -1, 24, 13, 25,  9,  8, 23, -1, 18, 22, 31, 27, 19, -1,
	1,  0,  3, 16, 11, 28, 12, 14,  6,  4,  2, -1, -1, -1, -1, -1
};

/* It's assumed that there is no chance of sending invalid chars to these
 * functions as they should have been checked beforehand. */
static void bech32_decode(uint8_t *data, int *data_len, const char *input)
{
	int input_len = strlen(input), hrp_len, i;

	*data_len = 0;
	while (*data_len < input_len && input[(input_len - 1) - *data_len] != '1')
		++(*data_len);
	hrp_len = input_len - (1 + *data_len);
	*(data_len) -= 6;
	for (i = hrp_len + 1; i < input_len; i++) {
		int v = (input[i] & 0x80) ? -1 : charset_rev[(int)input[i]];

		if (i + 6 < input_len)
			data[i - (1 + hrp_len)] = v;
	}
}

static void convert_bits(char *out, int *outlen, const uint8_t *in,
			 int inlen)
{
	const int outbits = 8, inbits = 5;
	uint32_t val = 0, maxv = (((uint32_t)1) << outbits) - 1;
	int bits = 0;

	while (inlen--) {
		val = (val << inbits) | *(in++);
		bits += inbits;
		while (bits >= outbits) {
			bits -= outbits;
			out[(*outlen)++] = (val >> bits) & maxv;
		}
	}
}


static const int b58tobin_tbl[] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1,  0,  1,  2,  3,  4,  5,  6,  7,  8, -1, -1, -1, -1, -1, -1,
	-1,  9, 10, 11, 12, 13, 14, 15, 16, -1, 17, 18, 19, 20, 21, -1,
	22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, -1, -1, -1, -1, -1,
	-1, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, -1, 44, 45, 46,
	47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57
};

/* b58bin should always be at least 25 bytes long and already checked to be
 * valid. */
void b58tobin(char *b58bin, const char *b58)
{
	uint32_t c, bin32[7];
	int len, i, j;
	uint64_t t;

	memset(bin32, 0, 7 * sizeof(uint32_t));
	len = strlen((const char *)b58);
	for (i = 0; i < len; i++) {
		c = b58[i];
		c = b58tobin_tbl[c];
		for (j = 6; j >= 0; j--) {
			t = ((uint64_t)bin32[j]) * 58 + c;
			c = (t & 0x3f00000000ull) >> 32;
			bin32[j] = t & 0xffffffffull;
		}
	}
	*(b58bin++) = bin32[0] & 0xff;
	for (i = 1; i < 7; i++) {
		*((uint32_t *)b58bin) = htobe32(bin32[i]);
		b58bin += sizeof(uint32_t);
	}
}

static int address_to_pubkeytxn(char *pkh, const char *addr)
{
	char b58bin[25] = {};

	b58tobin(b58bin, addr);
	pkh[0] = 0x76;
	pkh[1] = 0xa9;
	pkh[2] = 0x14;
	memcpy(&pkh[3], &b58bin[1], 20);
	pkh[23] = 0x88;
	pkh[24] = 0xac;
	return 25;
}

static int address_to_scripttxn(char *psh, const char *addr)
{
	char b58bin[25] = {};

	b58tobin(b58bin, addr);
	psh[0] = 0xa9;
	psh[1] = 0x14;
	memcpy(&psh[2], &b58bin[1], 20);
	psh[22] = 0x87;
	return 23;
}

static int segaddress_to_txn(char *p2h, const char *addr)
{
	int data_len, witdata_len = 0;
	char *witdata = &p2h[2];
	uint8_t data[84];

	bech32_decode(data, &data_len, addr);
	p2h[0] = data[0];
	/* Witness version is > 0 */
	if (p2h[0])
		p2h[0] += 0x50;
	convert_bits(witdata, &witdata_len, data + 1, data_len - 1);
	p2h[1] = witdata_len;
	return witdata_len + 2;
}

/* Convert an address to a transaction and return the length of the transaction */
int address_to_txn(char *p2h, const char *addr, const bool script, const bool segwit)
{
	if (segwit)
		return segaddress_to_txn(p2h, addr);
	if (script)
		return address_to_scripttxn(p2h, addr);
	return address_to_pubkeytxn(p2h, addr);
}

DECLARE_FUNC(addressToScript) {
  DECLARE_SCOPE;

  if (args.Length() < 3)
      RETURN_EXCEPT("You must provide address, isScript and isWitness.");

#if NODE_MAJOR_VERSION >= 12
  String::Utf8Value target(isolate, args[0]->ToString(isolate));
  bool isScript = args[1]->BooleanValue(isolate);
  bool isWitness = args[2]->BooleanValue(isolate);
#else
  String::Utf8Value target(args[0]->ToString());
  bool isScript = args[1]->BooleanValue();
  bool isWitness = args[2]->BooleanValue();
#endif
  const char *input = *target;
  char output[255];

  SET_BUFFER_RETURN(output, address_to_txn(output, input, isScript, isWitness));
}

DECLARE_INIT(init) {
    NODE_SET_METHOD(exports, "addressToScript", addressToScript);
}

NODE_MODULE(cryptocurrencyaddr, init)
