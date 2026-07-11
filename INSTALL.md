# AET Compiler Installation Guide

AET is a modified and extended compiler based on GCC 15.2.0.

## Current Installation Environment
- **Ubuntu 24.04**
- **GCC 15.2.0**

The entire compilation process usually takes **25–60 minutes**, depending on network speed and CPU performance.

## Recommended Configuration
- 4 GB or more RAM
- 20 GB or more disk space
- 4-core CPU or better

## Step 1: Download GCC 15.2.0 Source Code

Official download page:  
[https://gcc.gnu.org/mirrors.html](https://gcc.gnu.org/mirrors.html)

Recommended Chinese mirrors (faster):
- Alibaba Cloud: https://mirrors.aliyun.com/gnu/gcc/gcc-15.2.0/
- Tsinghua University: https://mirrors.tuna.tsinghua.edu.cn/gnu/gcc/gcc-15.2.0/

After downloading, you will get: `gcc-15.2.0.tar.xz`

## Step 2: Download AET Source Code

```bash
mkdir -p /home/my/aetgit
cd /home/my/aetgit
git clone https://github.com/onlineaet/aet.git
```

## Step 3: Create Working Directory and Extract GCC

```bash
mkdir -p /home/my/aet2.0
cd /home/my/aet2.0
```

Copy the downloaded `gcc-15.2.0.tar.xz` to this directory and extract it:

```bash
tar -xf gcc-15.2.0.tar.xz
```

The directory structure after extraction:
```
/home/my/aet2.0
    ├── gcc-15.2.0
    │   ├── config
    │   ├── gcc
    │   ├── libcpp
    │   └── ...
```

## Step 4: Copy AET Source Code into GCC

```bash
cd /home/my/aet2.0
cp -R /home/my/aetgit/aet/src/gcc/*    /home/my/aet2.0/gcc-15.2.0/gcc/
cp -R /home/my/aetgit/aet/src/libcpp/* /home/my/aet2.0/gcc-15.2.0/libcpp/
```

If prompted with `overwrite?`, enter `yes`.

## Step 5: Install Dependencies

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    flex \
    bison \
    gawk \
    texinfo \
    libgmp-dev \
    libmpfr-dev \
    libmpc-dev \
    libisl-dev \
    zstd \
    libzstd-dev \
    libz-dev
```

## Step 6: Build AET

> ⚠ **Important**: GCC does not allow building directly inside the source directory. You **must** use a separate build directory (out-of-tree build).

### 1. Create Build Directory

```bash
cd /home/my/aet2.0
mkdir build
cd build
```

### 2. Configure GCC/AET

```bash
../gcc-15.2.0/configure \
    --enable-languages=c,c++ \
    --disable-multilib \
    --disable-bootstrap \
    --prefix=/home/my/aet2.0/install
```

### 3. Compile

```bash
make -j$(nproc)
```

### 4. Install

```bash
make install
```

After installation, the compiler is located at:  
`/home/my/aet2.0/install/bin/gcc`

## Step 7: Configure PATH

**Temporary (current terminal only)**:

```bash
export PATH=/home/my/aet2.0/install/bin:$PATH
```

**Permanent**:

```bash
echo 'export PATH=/home/my/aet2.0/install/bin:$PATH' >> ~/.bashrc
source ~/.bashrc
```

## Step 8: Verify Installation

```bash
which gcc
gcc --version
```

Expected output:
```
gcc (AET 2.0.0) 15.2.0
```

## Step 9: Test AET

Create a test file `test.c`:

```c
#include <stdio.h>

class$ Test{
    void hello();
};

impl$ Test{
    void hello(){
        printf("hello world\n");
    }
};

int main()
{
    Test *t = new$ Test();
    t->hello();
    return 0;
}
```

Compile and run:

```bash
gcc test.c -o test
./test
```

If it prints `hello world`, AET is working correctly.

---

# Troubleshooting

### 1. `configure: error: GMP/MPFR/MPC not found`

Install the missing libraries:

```bash
sudo apt install -y \
    libgmp-dev \
    libmpfr-dev \
    libmpc-dev
```

### 2. Do not run `make` directly in the GCC source directory

**Wrong**:
```bash
cd gcc-15.2.0
make
```

**Correct**: Always use a separate `build` folder.

### 3. After modifying source code

```bash
rm -rf build
mkdir build
cd build
```

Then re-run configure and make.

---

# Installation Complete

**AET Compiler has been successfully installed!** 🎉
