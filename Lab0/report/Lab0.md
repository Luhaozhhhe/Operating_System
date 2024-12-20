# Lab0

主要是环境的配置

```
vim ~/.bashrc
```

进入环境的编辑

```
export RISCV=/home/luhaozhhhe/riscv/riscv64-unknown-elf-toolchain-10.2.0-2020.12.8-x86_64-linux-ubuntu14
export PATH=$RISCV/bin:$PATH
```

加入两行环境变量

```
source ~/.bashrc
```

保存环境的编辑

发现gcc已经安装好了

下面安装qemu，失败了好多次。。。

```
$ wget https://download.qemu.org/qemu-4.1.1.tar.xz
$ tar xvJf qemu-4.1.1.tar.xz
$ cd qemu-4.1.1
$ ./configure --target-list=riscv32-softmmu,riscv64-softmmu
$ make -j
$ sudo make install
```

![8b0d501cffbb7d73a71a76d325f2dae](E:\学学学\本科\大三上\操作系统\Lab\Lab0\report\Lab0.assets\8b0d501cffbb7d73a71a76d325f2dae.png)

qemu安装成功！

输入

```bash
$ qemu-system-riscv64 \
  --machine virt \
  --nographic \
  --bios default
```

![9f924854c6b7ae49e127f2fe60e0878](E:\学学学\本科\大三上\操作系统\Lab\Lab0\report\Lab0.assets\9f924854c6b7ae49e127f2fe60e0878.png)

说明环境配置成功！

我们进行lab0的测试

![image-20240912223156063](E:\学学学\本科\大三上\操作系统\Lab\Lab0\report\Lab0.assets\image-20240912223156063.png)

成功！lab0就结束了