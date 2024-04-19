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
	@echo "- make debug"
	@echo "    build vesper (with debug) and copy the binary to \"target\" folder"
	@echo "- make release"
	@echo "    build vesper (without debug) and copy the binary to \"target\" folder"
	@echo "- make"
	@echo "    alias for \"make all\""
	@echo "- (sudo) make install"
	@echo "    build vesper and copy binary to /usr/sbin"
	@echo "- (sudo) make uninstall"
	@echo "    uninstall vesper from system"

.PHONY: prepare-debug
prepare-debug:
	mkdir -p build && cd build \
	&& cmake -DCMAKE_BUILD_TYPE=Debug -G"Unix Makefiles" ../src


.PHONY: prepare-release
prepare-release:
	mkdir -p build && cd build \
	&& cmake -DCMAKE_BUILD_TYPE=Release -G"Unix Makefiles" ../src


# private target: --build
.PHONY: --build
--build:
	cd build && cmake --build . -- -j 8
	mkdir -p target && cp build/vesper target/
	cd target && mkdir -p asm-dump \
	&& objdump -d ./vesper > asm-dump/vesper.text.asm
	@echo -e "\033[32mbuild success (vesper).\033[0m"


.PHONY: build-debug
build-debug: prepare-debug --build


.PHONY: debug
debug: build-debug


.PHONY: build-release
build-release: prepare-release --build


.PHONY: release
release: build-release


.PHONY: clean
clean:
	rm -rf ./build
	rm -rf ./target


.PHONY: run-no-args
run-no-args: debug
	cd target && ./vesper


.PHONY: run
run: debug
	cd target && ./vesper \
		--add-virtual-display 720*720 \
		--use-pixman-renderer \
		--exec-cmds "konsole, dolphin" \
		--enable-vnc \
		--vnc-auth-passwd 123456 \
		--vnc-port 5900 \
		--libvncserver-passwd-file vesper-vnc-passwd \
		--enable-ctrl \
		--ctrl-domain-socket vesper.sock


.PHONY: run-release
run-release: release
	cd target && ./vesper \
		--add-virtual-display 720*720 \
		--use-pixman-renderer \
		--exec-cmds "konsole, dolphin" \
		--enable-vnc \
		--vnc-auth-passwd 123456 \
		--vnc-port 5900 \
		--libvncserver-passwd-file vesper-vnc-passwd \
		--enable-ctrl \
		--ctrl-domain-socket vesper.sock


.PHONY: run-headless
run-headless: debug
	cd target && ./vesper \
		--headless \
		--add-virtual-display 1440*900 \
		--use-pixman-renderer \
		--exec-cmds "konsole, dolphin" \
		--enable-vnc \
		--vnc-auth-passwd 123456 \
		--vnc-port 5900 \
		--libvncserver-passwd-file vesper-vnc-passwd \
		--enable-ctrl \
		--ctrl-domain-socket vesper.sock


.PHONY: install
install: release
	cp target/vesper /usr/sbin/vesper


.PHONY: uninstall
uninstall:
	rm -f /usr/sbin/vesper


.PHONY: all
all: debug
