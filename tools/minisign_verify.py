#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# ESTAMPIE: check a minisign signature (https://jedisct1.github.io/minisign/) without installing minisign: the format
# is Ed25519 over the file's BLAKE2b-512 ("ED", prehashed) plus a second signature binding the trusted comment.
# Ed25519 is checked with the openssl command (OpenSSL 3 or later); Python's hashlib has BLAKE2b.
#   tools/minisign_verify.py PUBLIC_KEY FILE [SIGNATURE]     (default SIGNATURE: FILE.minisig)
# Exits 0 and prints the trusted comment when both signatures are good, 1 otherwise.
import base64
import hashlib
import os
import subprocess
import sys
import tempfile

ED25519_SPKI = bytes.fromhex("302a300506032b6570032100")   # DER prefix of a raw 32-byte Ed25519 public key.


def ed25519_ok(public_key, message, signature):
    with tempfile.TemporaryDirectory() as tmp:
        paths = {name: os.path.join(tmp, name) for name in ("key.der", "msg", "sig")}
        for name, data in (("key.der", ED25519_SPKI + public_key), ("msg", message), ("sig", signature)):
            with open(paths[name], "wb") as f:
                f.write(data)
        r = subprocess.run(["openssl", "pkeyutl", "-verify", "-pubin", "-keyform", "DER", "-inkey", paths["key.der"],
                            "-rawin", "-in", paths["msg"], "-sigfile", paths["sig"]], capture_output=True)
        return r.returncode == 0


def verify(public_key_b64, path, sig_path):
    key = base64.b64decode(public_key_b64)
    if len(key) != 42 or key[:2] != b"Ed":
        sys.exit("minisign_verify: not a minisign public key")
    key_id, key = key[2:10], key[10:]
    with open(sig_path, encoding="utf-8") as f:
        lines = f.read().splitlines()
    sig = base64.b64decode(lines[1])
    trusted = lines[2]
    global_sig = base64.b64decode(lines[3])
    if not trusted.startswith("trusted comment: "):
        sys.exit("minisign_verify: malformed signature file")
    algorithm, sig_key_id, sig = sig[:2], sig[2:10], sig[10:]
    if sig_key_id != key_id:
        sys.exit("minisign_verify: signed by a different key")
    with open(path, "rb") as f:
        if algorithm == b"ED":
            message = hashlib.blake2b(f.read(), digest_size=64).digest()
        elif algorithm == b"Ed":
            message = f.read()
        else:
            sys.exit("minisign_verify: unknown signature algorithm")
    comment = trusted[len("trusted comment: "):]
    if not ed25519_ok(key, message, sig):
        sys.exit(f"minisign_verify: {path}: BAD signature")
    if not ed25519_ok(key, sig + comment.encode(), global_sig):
        sys.exit(f"minisign_verify: {path}: BAD trusted comment signature")
    print(f"minisign_verify: {os.path.basename(path)}: good signature ({comment})")


if __name__ == "__main__":
    if len(sys.argv) not in (3, 4):
        sys.exit("usage: tools/minisign_verify.py PUBLIC_KEY FILE [SIGNATURE]")
    verify(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) == 4 else sys.argv[2] + ".minisig")
