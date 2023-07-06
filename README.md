# SyzDescribe

## Next
1. A more powerful and general SyzDescribe based on under constrained symbolic execution.
1. To support net device drivers. 

## Update
1. Now SyzDescribe supports Linux kernel v6.1/v6.2.
2. Add a script to run all bitcode

## SyzDescribe: Principled, Automated, Static Generation of Syscall Descriptions for Kernel Drivers

[PDF](https://www.cs.ucr.edu/~zhiyunq/pub/oakland23_syzdescribe.pdf)
```
@inproceedings{conf/sp/SyzDescribe23,
  author       = {Yu Hao and
                  Guoren Li and
                  Xiaochen Zou and
                  Weiteng Chen and
                  Shitong Zhu and
                  Zhiyun Qian and
                  Ardalan Amiri Sani},
  title        = {SyzDescribe: Principled, Automated, Static Generation of Syscall Descriptions for Kernel Drivers},
  booktitle    = {44rd {IEEE} Symposium on Security and Privacy, {SP} 2023, San Francisco,
                  CA, USA, May 22-25, 2023},
  publisher    = {{IEEE}},
  year         = {2023},
}
```

## Build
Requirements: Ubuntu 22.04
```shell
sudo apt install -y git llvm cmake
git clone https://github.com/seclab-ucr/SyzDescribe.git
cd SyzDescribe
bash ./script/build.bash
```

Note: The version of LLVM/Clang is 14.

## Run
```shell
build/tools/SyzDescribe/SyzDescribe --config=config.json
```
> The config.json refers to `config/config.json `, for example:
```
{
  "bitcode": "built-in.bc", 
    // the path of the linked bitcode file of kernel modules
  "knowledge": "~/SyzDescribe/config/knowledge.json",
    // the path of the knowledge file
  "version": "v6.2"
    // the version of the kernel, used to generate debug info
}
```
The generated syscall descriptions are `syz_describe_*.txt`, which can directly used in syzkaller based on [doc](https://github.com/google/syzkaller/blob/master/docs/syscall_descriptions.md).

> There are two knowledge files for different versions of the kernel.
> Please choose the correct one.

## Run All
There are two scripts to help run SyzDescribe on all bitcode.
```
script/syzlang_run.py
script/syzlang_run_all.py
```
Usage:
1. copy two scripts to the bitcode dir.
2. change the value in syzlang_run.py based on comments
3. rename all other bitcode to built-in.bc if needed
  ```
  python3 syzlang_run_all.py rename_bc
  ```
4. copy script to all subdirectories of built-in.bc
  ```
  python3 syzlang_run_all.py copy_script
  ```
5. run script in all subdirectories of built-in.bc
  ```
  python3 syzlang_run_all.py run_script
  ```
6. copy all generate syzlang files to all_syscall_descriptions
  ```
  python3 syzlang_run_all.py copy_syzlang
  ```


## Linked LLVM Bitcode for Linux Kernel: `built-in.bc`
refer to [https://github.com/ZHYfeng/Generate_Linux_Kernel_Bitcode/tree/master/v5.12](https://github.com/ZHYfeng/Generate_Linux_Kernel_Bitcode/tree/master/v5.12)
> use `-save-temps` and `-g` to generate LLVM bitcode with debug info and less optimization

**Please do not use a huge bitcode file, e.g., `drivers/built-in.bc`.**
**I would suggest to generate syscall descriptions for each `drivers/*/built-in.bc`.**


## Generated syscall descriptions

SyzDescribe/SyzDescribe_Syscall_Description for v5.12.

[SyzDescribe_Syscall_Description](https://github.com/ZHYfeng/SyzDescribe_Syscall_Description) for all.

## Others
Ported DIFUZE used in paper: [https://github.com/ZHYfeng/PortedDIFUZE](https://github.com/ZHYfeng/PortedDIFUZE)



