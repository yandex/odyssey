#ifndef KIWI_SASL_H
#define KIWI_SASL_H

typedef enum {
	KIWI_BE_SASL_INIT = 10,
	KIWI_BE_SASL_CONTINUE = 11,
	KIWI_BE_SASL_FINAL = 12,
} kiwi_be_sasl_msg_type_t;

#endif /* KIWI_SASL_H */