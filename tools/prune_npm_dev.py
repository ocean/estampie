#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# ESTAMPIE: remove the development-only npm packages from an installed package tree (Emscripten's release ships its
# eslint / vite / test tooling in node_modules; emcc never loads them). Uses the tree's own package-lock.json
# (lockfileVersion 2 or 3, "dev": true), so no npm is needed, and rewrites the lock to match what is left, so a
# lockfile scan (tools/osv_scan.sh) reports only what can run.
#   tools/prune_npm_dev.py DIR       (DIR holds package-lock.json and node_modules/)
import json
import os
import shutil
import sys


def prune(root):
    lock_path = os.path.join(root, "package-lock.json")
    with open(lock_path, encoding="utf-8") as f:
        lock = json.load(f)
    if lock.get("lockfileVersion", 1) < 2:
        sys.exit(f"prune_npm_dev: {lock_path}: lockfileVersion 2 or 3 needed")
    packages = lock["packages"]
    dev = sorted((k for k, v in packages.items() if k and v.get("dev")), key=len, reverse=True)
    for key in dev:
        path = os.path.join(root, key)
        # The key is a path below root from the lock; refuse anything that would leave it.
        if os.path.commonpath([os.path.realpath(root), os.path.realpath(path)]) != os.path.realpath(root):
            sys.exit(f"prune_npm_dev: {key}: outside {root}")
        if os.path.isdir(path) and not os.path.islink(path):
            shutil.rmtree(path)
        elif os.path.lexists(path):
            os.remove(path)
        del packages[key]
    # The .bin links of removed packages now dangle.
    removed_links = 0
    for dirpath, dirnames, filenames in os.walk(os.path.join(root, "node_modules")):
        if os.path.basename(dirpath) == ".bin":
            for name in filenames + dirnames:
                link = os.path.join(dirpath, name)
                if os.path.islink(link) and not os.path.exists(link):
                    os.remove(link)
                    removed_links += 1
    packages[""].pop("devDependencies", None)
    with open(lock_path, "w", encoding="utf-8") as f:
        json.dump(lock, f, indent=2)
        f.write("\n")
    print(f"prune_npm_dev: {root}: removed {len(dev)} dev packages and {removed_links} .bin links, "
          f"{len(packages) - 1} remain")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: tools/prune_npm_dev.py DIR")
    prune(sys.argv[1])
