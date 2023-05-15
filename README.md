# I am still working on the open source of our work. Please keep an eye on this repo.

# SyzDescribe

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
There will be some additional changes coming for the Linux v6.1, so please stay tuned for updates.

2. A more powerful and general tool would come in the future.
