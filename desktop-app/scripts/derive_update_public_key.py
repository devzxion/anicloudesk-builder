#!/usr/bin/env python3
import base64
import os
from nacl.signing import SigningKey

secret = base64.b64decode(os.environ["UPDATE_ED25519_SECRET_KEY_BASE64"], validate=True)
if len(secret) == 64:
    secret = secret[:32]
if len(secret) != 32:
    raise SystemExit("manifest secret must decode to 32 or 64 bytes")
print(SigningKey(secret).verify_key.encode().hex())
