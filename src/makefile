# vesper 辅助构建脚本
#
# 创建于 2023年12月26日 上海市嘉定区安亭镇

.DEFAULT_GOAL := all

.PHONY: prepare
prepare:
	cd ../ && mkdir -p build && cd build \
	&& cmake -G"Unix Makefiles" ../src


.PHONY: build
build: prepare
	cd ../build && cmake --build . -- -j 8
	cd ../ && mkdir -p target && cp build/vesper target/


.PHONY: clean
clean:
	rm -rf ../build
	rm -rf ../target


.PHONY: run
run: build
	cd ../target && ./vesper


.PHONY: all
all: build
