SRC_DIR := $(shell pwd)
DST_DIR := $(SRC_DIR)/build

.PHONY: all clean renderdoc qrenderdoc

all: renderdoc qrenderdoc
	@ln -sf "$(DST_DIR)/bin"

renderdoc:
	@mkdir -p "$(DST_DIR)" && cd "$(DST_DIR)" && \
		cmake -DENABLE_QRENDERDOC=OFF "$(SRC_DIR)" && \
		$(MAKE)

qrenderdoc: renderdoc
	@mkdir -p "$(DST_DIR)"/qrenderdoc && cd "$(DST_DIR)"/qrenderdoc && \
		qmake "CONFIG+=debug" "DESTDIR=$(DST_DIR)/bin" "$(SRC_DIR)"/qrenderdoc && \
		$(MAKE)

clean:
	@rm -rf "$(DST_DIR)" bin
