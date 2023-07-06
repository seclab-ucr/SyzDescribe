#! /usr/bin/python3
import json
import sys
import os
import shutil
import subprocess
from time import sleep

cwd = os.getcwd()

name_run = "syzlang_run.py"


# rename all other bitcode to built-in.bc
def rename_bc():
    for (dir_path, dir_names, file_names) in os.walk(cwd):
        for file_name in file_names:
            if file_name.endswith(".bc") and file_name != "built-in.bc":
                print(dir_path)
                os.mkdir(os.path.join(dir_path, file_name[:len(file_name)-3]))
                shutil.copy(os.path.join(dir_path, file_name),
                            os.path.join(dir_path, file_name[:len(file_name)-3], "built-in.bc"))


# copy script to all subdirectories of built-in.bc
def copy_script():
    for (dir_path, dir_names, file_names) in os.walk(cwd):
        for file_name in file_names:
            if file_name == "built-in.bc" and dir_path != cwd:
                print(dir_path)
                shutil.copy(os.path.join(cwd, name_run),
                            os.path.join(dir_path, name_run))


# run script in all subdirectories of built-in.bc
def run_script():
    for (dir_path, dir_names, file_names) in os.walk(cwd):
        for file_name in file_names:
            if file_name == name_run and dir_path != cwd:
                print(dir_path)
                os.chdir(dir_path)
                cmd = "python3 " + name_run + " &"
                subprocess.Popen(cmd, shell=True)


# copy all generate syzlang files to all_syscall_descriptions
def copy_syzlang():
    name = "syz_describe_"
    target = "all_syscall_descriptions"
    for (dir_path, dir_names, file_names) in os.walk(cwd):
        for file_name in file_names:
            if not (file_name.startswith(name)):
                continue
            if dir_path.endswith(target):
                continue
            if file_name.endswith(".each.json") or file_name.endswith(".each.txt"):
                continue
            if os.stat(os.path.join(dir_path, file_name)).st_size < 10:
                continue
            print(os.path.join(dir_path, file_name))
            shutil.copy(os.path.join(dir_path, file_name),
                        os.path.join(".", target, file_name))


# copy all statistic files to all_syscall_descriptions
def copy_statistic():
    name = "statistic.txt"
    target = "all_syscall_descriptions"

    name1 = "overall.txt"
    name2 = "drivers.txt"

    f1 = open(os.path.join(".", target, name1), "w")
    f2 = open(os.path.join(".", target, name2), "w")

    for (dir_path, dir_names, file_names) in os.walk(cwd):
        for file_name in file_names:
            if file_name != name:
                continue
            if dir_path.endswith(target):
                continue

            print(os.path.join(dir_path, file_name))
            f = open(os.path.join(dir_path, file_name), "r")
            Lines = f.readlines()
            f.close()
            count = 0
            for line in Lines:
                if count == 0:
                    f1.writelines(line)
                else:
                    f2.writelines(line)
                count += 1


# clean all syzlang files
def clean():
    name = "syz_describe_"
    # name = "run.bash"
    for (dir_path, dir_names, file_names) in os.walk(cwd):
        for n in file_names:
            if n.startswith("syz_describe_"):
                cmd = "rm -rf " + os.path.join(dir_path, n)
                subprocess.Popen(cmd, shell=True)


def main():
    cmd = ""
    if len(sys.argv) == 2:
        cmd = sys.argv[1]

    if cmd == "rename_bc":
        rename_bc()
    elif cmd == "copy_script":
        copy_script()
    elif cmd == "run_script":
        run_script()
    elif cmd == "copy_syzlang":
        copy_syzlang()
    elif cmd == "copy_statistic":
        copy_statistic()
    elif cmd == "clean":
        clean()
    else:
        print(
            "Usage: python3 syzlang_run_all.py [rename_bc|copy_script|run_script|copy_syzlang|copy_statistic|clean]")


if __name__ == "__main__":
    main()
