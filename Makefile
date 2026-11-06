TARGET               := backstab_nx
PORT_NPDM_PROGRAM_ID := 0x0100000000001040
PORT_CFLAGS          := -Ithird_party/ffmpeg32/include
PORT_LIBS            := -Lthird_party/ffmpeg32/lib -lavformat -lavcodec -lavutil
include runtime/runtime.mk
$(BUILD)/backstab_intro_dec.o: CFLAGS += -fno-short-enums
