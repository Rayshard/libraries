# type: ignore
from genericpath import isfile
import sys
import os
import glob
import platform
import subprocess
import click
import pathlib

EXECUTABLE_EXTENSION = ".exe" if platform.system() == "Windows" else ""

@click.command()
@click.option("--std", type=str, default="c++2a", help="The C++ standard to use.")
@click.argument("library", type=str, required=True, nargs=1)
def build(std: str, library: str):
    print(f"Building {library}...")
    assert os.path.isdir(library), f"The directory '{library}/' does not exist!"

    cpp_files = [file for file in glob.glob(f"{library}/*.cpp")]
    assert len(cpp_files) != 0, f"There are no .cpp files in {library}/"

    print("\nThe following cpp files will be included in the build:")
    for file in cpp_files:
        print("\t" + file)

    # Create bin directory that houses executable
    if not os.path.isdir(f"{library}/bin/"):
        os.mkdir(f"{library}/bin/")

    # Run g++
    output_path = f"{library}/bin/main" + EXECUTABLE_EXTENSION
    build_call_args = ["g++", "-std=" + std, "-fdiagnostics-color=always", "-g", "-o", output_path] + cpp_files
    
    print(f"\n[CMD] {' '.join(build_call_args)}")
    subprocess.call(build_call_args)

@click.command()
@click.option("--type", type=click.Choice(['major', 'minor', 'patch'], case_sensitive=False), required=True)
@click.argument("library", type=str, required=True, nargs=1)
def release(type: str, library: str):
    assert os.path.isdir(library), f"The directory '{library}/' does not exist!"
    print(type)
    

@click.command()
@click.argument("library", type=str, required=True, nargs=1)
def run(library: str):
    assert os.path.isdir(library), f"The directory '{library}/' does not exist!"

    old_dir = os.getcwd()
    os.chdir(f"{library}/")

    executable_path = os.path.abspath(f"bin/main" + EXECUTABLE_EXTENSION)
    assert os.path.isfile(executable_path), f"The executable '{executable_path}' does not exist!"

    cmd = f"./{os.path.basename(os.path.dirname(executable_path))}/{os.path.basename(executable_path)}"
    print(f"[CMD] {cmd}\n")

    subprocess.call(cmd)
    os.chdir(old_dir)

@click.command()
@click.argument("name", type=str, required=True, nargs=1)
def create(name: str):
    assert not os.path.isdir(name), f"The directory '{name}/' already exists!"
    os.mkdir(f"{name}/")

    old_dir = os.getcwd()
    os.chdir(f"{name}/")

    pathlib.Path(f'{name}.h').touch(exist_ok=False)
    pathlib.Path('main.cpp').touch(exist_ok=False)
    pathlib.Path('README.md').touch(exist_ok=False)
    os.mkdir(f"bin/")

    with open('main.cpp', 'w') as f:
        f.write(f"#include \"{name}.h\"\n") 
        f.write("#include <iostream>\n\n") 
        f.write(f"using namespace {name};\n\n") 
        f.write("int main(int _argc, char* _argv[])\n")
        f.write("{\n")
        f.write("\tstd::cout << \"Hello, World!\" << std::endl;\n")
        f.write("}") 

    with open(f"{name}.h", 'w') as f:
        f.write(f"#pragma once\n\nnamespace {name}\n")
        f.write("{\n\n}")

    os.chdir(old_dir)