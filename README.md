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
    // the version of the kernel, used for debug info
}
```
The generated syscall descriptions are `syz_describe_*.txt`, which can directly used in syzkaller based on [doc](https://github.com/google/syzkaller/blob/master/docs/syscall_descriptions.md).

## Linked LLVM Bitcode for Linux Kernel
refer to `https://github.com/ZHYfeng/Generate_Linux_Kernel_Bitcode/tree/master/v5.12`
> use `-save-temps` and `-g` to generate LLVM bitcode with debug info and less optimization


## Example

Generated syscall descriptions: `https://github.com/seclab-ucr/SyzDescribe_Syscall_Description`


## Others
Ported DIFUZE used in paper: `https://github.com/ZHYfeng/PortedDIFUZE`