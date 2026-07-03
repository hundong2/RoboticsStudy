#!/usr/bin/env bash
set -euo pipefail

sudo apt-get update
sudo apt-get install -y build-essential git libopencv-dev cmake libprotobuf-dev protobuf-compiler

mkdir -p "$HOME/src"
cd "$HOME/src"

if [ ! -d darknet ]; then
  git clone https://codeberg.org/CCodeRun/darknet.git
fi

cd darknet
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j"$(nproc)" package

echo
echo "빌드 완료. 설치하려면 다음 명령을 실행하세요:"
echo "sudo dpkg -i $HOME/src/darknet/build/darknet-*.deb"
