SRC_DIR := $(shell pwd)
DST_DIR := $(SRC_DIR)/build
CMAKE_PARAMS :=

.PHONY: all clean

all: renderdoc
	@mkdir -p "$(DST_DIR)" && cd "$(DST_DIR)" && \
		cmake $(CMAKE_PARAMS) "$(SRC_DIR)" && \
		$(MAKE)

clean:
	@rm -rf "$(DST_DIR)" bin
