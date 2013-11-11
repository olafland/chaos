CONTIKI_PROJECT = chaos-test
all: $(CONTIKI_PROJECT)

OBJDUMP = msp430-objdump

bytes: 
	@$(OBJDUMP) -h $(CONTIKI_PROJECT).sky | perl -ne '$$b{$$1}=hex $$2 if /^\s*\d+\s*\.(text|data|bss)\s+(\S+)/; END { printf("%16d bytes in ROM\n%16d bytes in RAM\n",$$b{text}+$$b{data},$$b{data}+$$b{bss}); }'

CONTIKI = ../..
include $(CONTIKI)/Makefile.include
