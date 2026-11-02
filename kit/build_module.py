# Builds the native game module (gGMFE69_recomp.dll) from DolRecomp's C output.
#
# Everything is compiled from one source root with RELATIVE paths: clang keys the profile data of
# static functions on the source path it was given, so the shipped PGO profile only matches when
# every PC builds with the same relative names. The root holds:
#   mod/  module_export.c, gen_module_tables.py   (RecompCore module-template)
#   gx/   GXRuntime include/ and src/core/         (the CPU runtime linked into the module)
#   abi/  RecompCore StaticRecomp ABI headers
#   generated/  DolRecomp output: generated.h, generated_smc.txt, main.dol, chunks/*.c
#
# Usage: python build_module.py --root DIR --clang BIN_DIR --python EXE --out DLL
#                               [--profile-use FILE | --profile-generate] [-j N]
import argparse
import concurrent.futures
import glob
import os
import subprocess
import sys

CORE = ["cpu.c", "cpu_exception.c", "cpu_interpreter.c", "cpu_interpreter_table.c",
        "cpu_interpreter_float.c", "cpu_interpreter_integer.c"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--clang", required=True, help="llvm-mingw bin folder")
    ap.add_argument("--python", default=sys.executable)
    ap.add_argument("--out", required=True)
    ap.add_argument("--profile-use")
    ap.add_argument("--profile-generate", action="store_true")
    ap.add_argument("-j", type=int, default=os.cpu_count() or 4)
    a = ap.parse_args()

    os.chdir(a.root)
    clang = os.path.join(a.clang, "clang.exe")
    os.makedirs("obj", exist_ok=True)

    tables = "obj/module_tables.inc"
    subprocess.run([a.python, "-I", "mod/gen_module_tables.py", "generated/generated.h",
                    "generated/generated_smc.txt", "generated/main.dol", tables], check=True)

    pgo = []
    if a.profile_generate:
        pgo = ["-fprofile-generate"]
    elif a.profile_use:
        pgo = ["-fprofile-use=" + os.path.abspath(a.profile_use).replace("\\", "/"),
               "-Wno-profile-instr-unprofiled", "-Wno-profile-instr-out-of-date"]
    cflags = ["-O3", "-DNDEBUG", "-std=gnu11", "-flto=thin", "-fvisibility=hidden",
              "-ffp-contract=off", "-fno-fast-math",
              '-DDOLRECOMP_CPU_HEADER="core/cpu.h"', '-DMODULE_GAME_ID="GMFE69"',
              "-DgGMFE69_recomp_EXPORTS",
              "-Igenerated", "-Igx/include", "-Iabi", "-Iobj"] + pgo

    sources = ["mod/module_export.c"] + ["gx/src/core/" + c for c in CORE] + \
        sorted(p.replace("\\", "/") for p in glob.glob("generated/chunks/*.c"))
    if len(sources) < 10:
        raise SystemExit("no generated chunks under generated/chunks")

    def compile_one(src):
        obj = "obj/" + src.replace("/", "_")[:-2] + ".o"
        r = subprocess.run([clang, *cflags, "-c", src, "-o", obj],
                           capture_output=True, text=True)
        return obj, (r.stderr if r.returncode else None)

    objs, done = [], 0
    with concurrent.futures.ThreadPoolExecutor(max(1, a.j)) as pool:
        for obj, err in pool.map(compile_one, sources):
            if err:
                raise SystemExit("compile failed for %s:\n%s" % (obj, err[-4000:]))
            objs.append(obj)
            done += 1
            if done % 10 == 0 or done == len(sources):
                print("  compiled %d/%d" % (done, len(sources)), flush=True)

    rsp = "obj/link.rsp"
    with open(rsp, "w") as f:
        f.write("\n".join(objs) + "\n")
    out = os.path.abspath(a.out)
    print("  linking (ThinLTO, a few minutes)", flush=True)
    r = subprocess.run([clang, "-O3", "-flto=thin", *pgo, "-shared", "-o", out,
                        "-Wl,--thinlto-jobs=%d" % max(1, a.j), "@" + rsp])
    if r.returncode or not os.path.exists(out):
        raise SystemExit("link failed")
    print("  built " + out, flush=True)


if __name__ == "__main__":
    main()
