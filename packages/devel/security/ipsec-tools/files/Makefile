###################################################################
#                                                                 #
# Makefile to simply create the hash-named symlinks needed by     #
# OpenSSL to find the certificates.                               #
#                                                                 #
# 2004, Daniel Mueller <daniel at danm dot de>                    #
#                                                                 #
###################################################################
#                                                                 #
# All files should be stored in PEM format. If you want a peer to #
# be checked against a Certificate Authority (verify_cert on;)    #
# you have to provide the CA certificate using CA<filename>.pem   #
# as filename.                                                    #
# Furthermore, I recommend you to supply a Certificate Revocation #
# List (CRL) file using the following format:                     #
# CRL<filename>.pem                                               #
#                                                                 #
###################################################################

CA_SUFFIX=CA*
CRL_SUFFIX=CRL*

all:	.0 .r0

.0 :	
	@for file in `ls $(CA_SUFFIX) 2>/dev/null`; \
       	do ln -sf $$file `openssl x509 -noout -hash < $$file`.0; \
	done;

.r0 :	
	@for file in `ls $(CRL_SUFFIX) 2>/dev/null`; \
       	do ln -sf $$file `openssl x509 -noout -hash < $$file`.r0; \
	done;

clean:
	rm -f *.r0 *.0
