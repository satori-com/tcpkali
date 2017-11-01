
ASN_MODULEdir=

include Makefile.am.libasncodec

ASN_MODULE_CFLAGS+="-static"

generate:
	asn1c -gen-OER -gen-PER -pdu=TcpkaliMessage TcpkaliOrchestration.asn1
