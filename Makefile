# vesper 辅助构建脚本
#
# 创建于 2023年12月26日 上海市嘉定区安亭镇

.DEFAULT_GOAL := all

.PHONY: help
help:
	@echo "vesper makefile"
	@echo "---------------"
	@echo "commands available:"
	@echo "- make run"
	@echo "    build and launch vesper"
	@echo "- make build"
	@echo "    build vesper and copy the binary to \"target\" folder"
	@echo "- make"
	@echo "    alias for \"make all\""

.PHONY: prepare
prepare:
	mkdir -p build && cd build \
	&& cmake -G"Unix Makefiles" ../src


.PHONY: build
build: prepare
	cd build && cmake --build . -- -j 8
	mkdir -p target && cp build/vesper target/
	cd target && mkdir -p asm-dump \
	&& objdump -d ./vesper > asm-dump/vesper.text.asm
	@echo -e "\033[32mbuild success (vesper).\033[0m"


.PHONY: clean
clean:
	rm -rf ./build
	rm -rf ./target


.PHONY: run-no-args
run-no-args: build
	cd target && ./vesper


.PHONY: run
run: build
	cd target && ./vesper \
	--headless \
	--add-virtual-display 720*720 \
	--use-pixman-renderer \
	--exec-cmds "2,7,7,konsoledolphin" \
	--enable-vnc \
	--vnc-auth-passwd 123456 \
	--vnc-port 5900 \
	--libvncserver-passwd-file vesper-vnc-passwd \
	--enable-ctrl \
	--ctrl-domain-socket vesper-ctrl.sock


.PHONY: all
all: build
