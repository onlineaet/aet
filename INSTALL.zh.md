# AET 编译器安装向导

AET 是基于 GCC 15.2.0 修改扩展的编译器。

## 当前安装环境
- Ubuntu 24.04
- GCC 15.2.0

整个编译过程通常需要 **25～60 分钟**，具体取决于网络速度与 CPU 性能。

## 推荐配置
- 4GB 以上内存
- 20GB 以上磁盘空间
- 4 核 CPU

## 第一步：下载 GCC 15.2.0 源码

官方下载地址：  
[https://gcc.gnu.org/mirrors.html](https://gcc.gnu.org/mirrors.html)

国内镜像推荐：
- 阿里云镜像：https://mirrors.aliyun.com/gnu/gcc/gcc-15.2.0/
- 清华镜像：https://mirrors.tuna.tsinghua.edu.cn/gnu/gcc/gcc-15.2.0/

下载完成后得到：`gcc-15.2.0.tar.xz`

## 第二步：下载 AET 源码

```bash
mkdir -p /home/my/aetgit
cd /home/my/aetgit
git clone https://github.com/onlineaet/aet.git
```

## 第三步：建立工作目录并解压 GCC

```bash
mkdir -p /home/my/aet2.0
cd /home/my/aet2.0
```

将下载的 `gcc-15.2.0.tar.xz` 复制到 `/home/my/aet2.0` 目录，然后解压：

```bash
tar -xf gcc-15.2.0.tar.xz
```

解压后的目录结构：
```
/home/my/aet2.0
    ├── gcc-15.2.0
    │   ├── config
    │   ├── gcc
    │   ├── libcpp
    │   └── ...
```

## 第四步：复制 AET 源码到 GCC

```bash
cd /home/my/aet2.0
cp -R /home/my/aetgit/aet/src/gcc/*    /home/my/aet2.0/gcc-15.2.0/gcc/
cp -R /home/my/aetgit/aet/src/libcpp/* /home/my/aet2.0/gcc-15.2.0/libcpp/
```

如果提示 `overwrite?`，请选择 `yes`。

## 第五步：安装依赖库

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

## 第六步：编译 AET

> ⚠ **重要**：GCC 不允许在源码目录内直接编译，必须使用独立 build 目录（out-of-tree build）。

### 1. 创建独立 build 目录

```bash
cd /home/my/aet2.0
mkdir build
cd build
```

### 2. 配置 GCC/AET

```bash
../gcc-15.2.0/configure \
    --enable-languages=c,c++ \
    --disable-multilib \
    --disable-bootstrap \
    --prefix=/home/my/aet2.0/install
```

### 3. 开始编译

```bash
make -j$(nproc)
```

### 4. 安装

```bash
make install
```

安装完成后，编译器位于：`/home/my/aet2.0/install/bin/gcc`

## 第七步：配置 PATH

**临时生效**（当前终端）：

```bash
export PATH=/home/my/aet2.0/install/bin:$PATH
```

**永久生效**：

```bash
echo 'export PATH=/home/my/aet2.0/install/bin:$PATH' >> ~/.bashrc
source ~/.bashrc
```

## 第八步：验证安装

```bash
which gcc
gcc --version
```

成功输出示例：
```
gcc (AET 2.0.0) 15.2.0
```

## 第九步：测试 AET

创建测试文件 `test.c`：

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

编译运行：

```bash
gcc test.c -o test
./test
```

输出 `hello world` 即表示安装成功。

---

# 常见问题

### 1. configure: error: GMP/MPFR/MPC not found

缺少依赖库，执行：

```bash
sudo apt install -y \
    libgmp-dev \
    libmpfr-dev \
    libmpc-dev
```

### 2. 不要在 GCC 源码目录直接 make

**错误做法**：

```bash
cd gcc-15.2.0
make
```

**正确做法**：使用独立的 `build` 目录。

### 3. 修改源码后建议重新 build

```bash
rm -rf build
mkdir build
cd build
```

然后重新执行 configure 和 make。

---

# 安装完成

至此，**AET 编译器已经安装成功**！ 🎉
