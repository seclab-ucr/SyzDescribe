# SyzDescribe

# Still working on the open source. Keep an eye on this repo.

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

## Run
```shell
build/tools/SyzDescribe/SyzDescribe --config_json=config.json
```
> The config.json refers to `config/config.json `, for example:
```
{
  "bitcode": "built-in.bc", 
    // the path of the linked bitcode file of kernel modules
  "knowledge": "~/SyzDescribe/config/knowledge.json",
    // the path of the knowledge file
  "version": "v5.12"
    // the version of the kernel, used to generate debug info
}
```
The generated syscall descriptions are `syz_describe_*.txt`, which can directly used in syzkaller based on [doc](https://github.com/google/syzkaller/blob/master/docs/syscall_descriptions.md).

## Linked LLVM Bitcode for Linux Kernel: `built-in.bc`
refer to [https://github.com/ZHYfeng/Generate_Linux_Kernel_Bitcode/tree/master/v5.12](https://github.com/ZHYfeng/Generate_Linux_Kernel_Bitcode/tree/master/v5.12)
> use `-save-temps` and `-g` to generate LLVM bitcode with debug info and less optimization

**Please do not use a huge bitcode file, e.g., `drivers/built-in.bc`.**
**I would suggest to generate syscall descriptions for each `drivers/*/built-in.bc`.**


## Example Results
Generated syscall descriptions: SyzDescribe_Syscall_Description


## Others
Ported DIFUZE used in paper: [https://github.com/ZHYfeng/PortedDIFUZE](https://github.com/ZHYfeng/PortedDIFUZE)

## Next
1. Because the bitcodes generated from the Linux v6.1 are different from the Linux v5.12.
There will be some additional changes coming for the Linux v6.1.

1. A more powerful and general tool based on under constrained symbolic execution would come in the future.

