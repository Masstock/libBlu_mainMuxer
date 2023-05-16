###############################################################################
# libBlu mainMuxer Makefile                                                   #
###############################################################################

SHELL := /bin/sh
CC :=  gcc
LEX := flex
YACC := bison

CFLAGS := -std=c99 -Wall -Wextra -Winline
LDLIBS := -lm

EXEC := mainMuxer

SRC_PATH = src
OBJ_PATH = obj

###############################################################################
# OS dependant                                                                #
###############################################################################

ifneq "$(findstring linux, $(MAKECMDGOALS))" ""
LDLIBS += -ldl
endif

ifneq "$(findstring windows, $(MAKECMDGOALS))" ""
LDLIBS += -l:libiconv.a
endif

###############################################################################
# Compilation switches                                                        #
###############################################################################

COMPILE_WITH_IGS_COMPILER = 1
COMPILE_WITH_PGS_COMPILER = 1
COMPILE_WITH_INI_OPTIONS = 1

EXTCFLAGS :=
ifneq "$(findstring no_debug, $(MAKECMDGOALS))" ""
EXTCFLAGS += -D NDEBUGMSG
endif

ifneq "$(findstring build, $(MAKECMDGOALS))" ""
EXTCFLAGS += -D NDEBUG -O2
else
EXTCFLAGS += -g -ggdb
endif

ifneq "$(findstring leaks, $(MAKECMDGOALS))" ""
EXTCFLAGS += -fsanitize=address -fsanitize=leak
endif

ifneq "$(findstring profile, $(MAKECMDGOALS))" ""
EXTCFLAGS += -pg -fprofile-arcs
endif

###############################################################################
# Source files                                                                #
###############################################################################

SOURCE_FILES =																\
	codec/ac3/ac3_check.o													\
	codec/ac3/ac3_data.o													\
	codec/ac3/ac3_parser.o													\
	codec/ac3/mlp_parser.o													\
	codec/ac3/mlp_check.o													\
	codec/dts/dts_checks.o													\
	codec/dts/dts_dtshd_file.o												\
	codec/dts/dts_frame.o													\
	codec/dts/dts_parser.o													\
	codec/dts/dts_patcher_util.o											\
	codec/dts/dts_patcher.o													\
	codec/dts/dts_pbr_file.o												\
	codec/dts/dts_util.o													\
	codec/dts/dts_xll_checks.o												\
	codec/dts/dts_xll_util.o												\
	codec/dts/dts_xll.o														\
	codec/h262/h262_parser.o												\
	codec/h264/h264_checks.o												\
	codec/h264/h264_data.o													\
	codec/h264/h264_hrdVerifier.o											\
	codec/h264/h264_parser.o												\
	codec/h264/h264_patcher.o												\
	codec/h264/h264_util.o													\
	codec/hdmv/common/hdmv_check.o											\
	codec/hdmv/common/hdmv_common.o											\
	codec/hdmv/common/hdmv_context.o										\
	codec/hdmv/common/hdmv_data.o											\
	codec/hdmv/common/hdmv_palette_def.o									\
	codec/hdmv/common/hdmv_palette_gen.o									\
	codec/hdmv/common/hdmv_parser.o											\
	codec/hdmv/common/hdmv_pictures_common.o								\
	codec/hdmv/common/hdmv_pictures_indexer.o								\
	codec/hdmv/common/hdmv_pictures_list.o									\
	codec/hdmv/common/hdmv_pictures_pool.o									\
	codec/hdmv/common/hdmv_pictures_quantizer.o								\
	codec/hdmv/common/hdmv_timecodes.o										\
	codec/hdmv/igs_parser.o													\
	codec/hdmv/pgs_parser.o													\
	codec/lpcm/lpcm_parser.o												\
	codecsUtilities.o														\
	dtcpSettings.o															\
	elementaryStream.o														\
	elementaryStreamOptions.o												\
	elementaryStreamPesProperties.o											\
	elementaryStreamProperties.o											\
	esms/scriptCreation.o													\
	esms/scriptData.o														\
	esms/scriptParsing.o													\
	input/meta/metaFiles.o													\
	input/meta/metaFilesData.o												\
	input/meta/metaReader.o													\
	libs/cwalk/src/cwalk_wchar.o											\
	libs/cwalk/src/cwalk.o													\
	main.o																	\
	mainMuxer.o																\
	muxingContext.o															\
	muxingSettings.o														\
	packetIdentifier.o														\
	pesPackets.o															\
	stream.o																\
	streamCodingType.o														\
	streamHeap.o															\
	systemStream.o															\
	tsPackets.o																\
	tStdVerifier/bdavStd.o													\
	tStdVerifier/bufferingModel.o											\
	tStdVerifier/codec/ac3.o												\
	tStdVerifier/codec/dts.o												\
	tStdVerifier/codec/h264.o												\
	tStdVerifier/codec/hdmv.o												\
	tStdVerifier/codec/lpcm.o												\
	tStdVerifier/systemStreams.o											\
	util.o																	\
	util/bitStreamHandling.o												\
	util/circularBuffer.o													\
	util/common.o															\
	util/crcLookupTables.o													\
	util/errorCodes.o														\
	util/hashTables.o														\
	util/libraries.o														\
	util/textFilesHandling.o

LEXER_FILES =
PARSER_FILES =

###############################################################################
# Optional modules                                                            #
###############################################################################

ifeq ($(COMPILE_WITH_IGS_COMPILER), 1)
# Enable IGS Compiler

EXTCFLAGS += `xml2-config --cflags`
LDLIBS += `xml2-config --libs`

SOURCE_FILES +=																\
	util/xmlParsing.o														\
	codec/hdmv/compiler/igs_compiler.o										\
	codec/hdmv/compiler/igs_compiler_data.o									\
	codec/hdmv/compiler/igs_debug.o											\
	codec/hdmv/compiler/igs_segmentsBuilding.o								\
	codec/hdmv/compiler/igs_xmlParser.o										\
	codec/hdmv/common/hdmv_libpng.o											\
	codec/hdmv/common/hdmv_pictures_io.o

else
EXTCFLAGS += -D DISABLE_IGS_COMPILER
endif

ifeq ($(COMPILE_WITH_PGS_COMPILER), 1)
# Enable PGS Compiler

# TODO

else
EXTCFLAGS += -D DISABLE_PGS_COMPILER
endif

ifeq ($(COMPILE_WITH_INI_OPTIONS), 1)
# Enable INI configuration file parser

SOURCE_FILES +=																\
	ini/iniParser.tab.o														\
	ini/iniLexer.yy.o														\
	ini/iniData.o															\
	ini/iniTree.o															\
	ini/iniHandler.o

LEXER_FILES += ini/iniLexer.lex
PARSER_FILES +=	ini/iniParser.y

else
EXTCFLAGS += -D DISABLE_INI
endif

###############################################################################
# Compilation instructions                                                    #
###############################################################################

.SUFFIXES:

OBJECTS = $(patsubst %, $(OBJ_PATH)/%, $(SOURCE_FILES))
WASTES = $(patsubst %.y, $(OBJ_PATH)/%.tab.h, $(PARSER_FILES))

# Building lexers from .lex:
$(OBJ_PATH)/%.yy.c: $(SRC_PATH)/%.lex
	$(LEX) -o $@ $<
$(OBJ_PATH)/%.yy.o: $(OBJ_PATH)/%.yy.c
	$(CC) $(CFLAGS) $(EXTCFLAGS) -I $(SRC_PATH) -I $(OBJ_PATH) -o $@ -c $<

# Building parsers from .y:
$(OBJ_PATH)/%.tab.c: $(SRC_PATH)/%.y
	$(YACC) -o $@ -d $< -Wconflicts-sr -Wcounterexamples
$(OBJ_PATH)/%.tab.o: $(OBJ_PATH)/%.tab.c
	$(CC) $(CFLAGS) $(EXTCFLAGS) -I $(SRC_PATH) -I $(OBJ_PATH) -o $@ -c $<

# Compiling source files to .o :
$(OBJ_PATH)/%.o: $(SRC_PATH)/%.c
	$(CC) $(CFLAGS) $(EXTCFLAGS) -o $@ -c $<

# Build binary from .o :
$(EXEC): $(OBJECTS)
	$(CC) $(CFLAGS) $(EXTCFLAGS) -o $(EXEC) $(OBJECTS) $(LDLIBS)

all: $(EXEC)

build: all
leaks: all
linux: all
no_debug: all
profile: all
windows: all

clean:
	rm -rf $(OBJECTS) $(WASTES)

mrproper: clean
	rm -rf $(EXEC)

.PHONY: clean mrproper