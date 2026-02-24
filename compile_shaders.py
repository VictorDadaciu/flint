import os
import subprocess
import argparse
import sys

def parse_args() -> None:
    parser = argparse.ArgumentParser(
        prog="SXI Shader Compiler Utility",
        description="Utility script that compiles GLSL code into SPIRV using Google's glslc compiler")
    parser.add_argument("-v", "--version", help="Prints the version of the program.", action='version', version="%(prog)s 1.0")
    parser.add_argument("glsl",
                        help="Input path to GLSL shader files.")
    parser.add_argument("-o", "--spirv",
                        help="Output path to generated SPIRV shader files. Defaults to input directory if omitted.",
                        dest="spirv")
    argv = sys.argv[1:]
    if (len(argv) == 0):
        argv = ["-h"]
    args = vars(parser.parse_args(argv))
    global GLSL_DIR
    GLSL_DIR = os.path.abspath(args["glsl"])
    if not (os.path.exists(GLSL_DIR) and os.path.isdir(GLSL_DIR)):
        parser.error("Input path must be a valid directory")
    spirv: str | None = args["spirv"]
    global SPIRV_DIR
    SPIRV_DIR = GLSL_DIR if spirv is None else os.path.abspath(spirv)

def to_glsl_file(filename: str) -> str:
    return os.path.join(GLSL_DIR, filename)

def to_spirv_file(filename: str) -> str:
    return os.path.join(SPIRV_DIR, filename + ".spv")

def is_accepted_file(dir: str, item: str, accepted_extensions: set[str]) -> bool:
    full_path: str = os.path.join(dir, item)
    if not os.path.isfile(full_path):
        return False
    _, extension = os.path.splitext(item)
    if extension not in accepted_extensions:
        return False
    return True

def get_files_with_extensions_from_dir(dir: str, accepted_extensions: set[str]) -> set[str]:
    return {item for item in os.listdir(dir) if is_accepted_file(dir, item, accepted_extensions)}

def clean_from_spirv_dir(to_delete: set[str]) -> None:
    if len(to_delete) == 0:
        return
    
    print(f"Cleaning {len(to_delete)} SPIRV file(s)")
    for file in {to_spirv_file(filename) for filename in to_delete}:
        print(f"    Deleting {file}...")
        os.remove(file)

def needs_recompilation(filename: str) -> bool:
    return os.path.getmtime(to_glsl_file(filename)) > os.path.getmtime(to_spirv_file(filename))

def compile_files(to_compile: set[str]) -> int:
    errors = 0
    for filename in to_compile:
        print(f"    Compiling {filename}...", end=" ")
        ret_val = subprocess.run(["glslc", to_glsl_file(filename), "-o", to_spirv_file(filename)], capture_output=True)
        if ret_val.returncode != 0:
            print("FAILED")
            error_text = ret_val.stderr.decode()
            error_text = error_text[error_text.find(filename) + len(filename) + 2:]
            print(error_text)
            errors += 1
        else:
            print("SUCCESS")
    return errors

def compile_new_files(to_compile: set[str]) -> int:
    if len(to_compile) == 0:
        return 0
    
    print(f"Compiling {len(to_compile)} new file(s):")
    return compile_files(to_compile)

def recompile_files(to_compile: set[str]) -> int:
    if len(to_compile) == 0:
        return 0
    
    print(f"Recompiling {len(to_compile)} file(s):")
    return compile_files(to_compile)

if __name__ == "__main__":
    parse_args()
    os.makedirs(SPIRV_DIR, exist_ok=True)
    glsl_filenames  = get_files_with_extensions_from_dir(GLSL_DIR,  {".comp"})
    spirv_filenames = {item[:-4] for item in get_files_with_extensions_from_dir(SPIRV_DIR, {".spv"})}

    clean_from_spirv_dir(spirv_filenames - glsl_filenames)
    errors_num = compile_new_files(glsl_filenames - spirv_filenames) + \
                 recompile_files(set(filter(needs_recompilation, glsl_filenames.intersection(spirv_filenames))))
    if errors_num > 0:
        print(f"{errors_num} file(s) failed to compile")
        exit(1)

    print("All shader modules up-to-date")
