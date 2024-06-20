#!/bin/sh
current_dir=$PWD

cd ~/proj/venv 

. bin/activate

west zephyr-export

cd ${current_dir}
TOP=~/proj/velopera/repos/velopera-nrf-firmware
west build --build-dir ${TOP}/build ${TOP} \
	--pristine --board nrf9160dk_nrf9160_ns \
	--no-sysbuild -- -DNCS_TOOLCHAIN_VERSION=NONE -DCONF_FILE=${TOP}/prj.conf \
	${TOP}/boards/nrf9160dk_nrf9160_ns.conf -DDTC_OVERLAY_FILE=${TOP}/boards/nrf9160dk_nrf9160_ns.overlay \
	-G'Unix Makefiles' -DCMAKE_VERBOSE_MAKEFILE=ON -DCCACHE_ENABLE=0 \
	-DCMAKE_EXPORT_COMPILE_COMMANDS=1
