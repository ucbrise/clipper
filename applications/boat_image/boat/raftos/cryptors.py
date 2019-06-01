import base64

from .log import logger

try:
    from cryptography.fernet import Fernet
    from cryptography.hazmat.backends import default_backend
    from cryptography.hazmat.primitives import hashes
    from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC

    crypto_enabled = True

except ImportError:
    crypto_enabled = False

    logger.warning('cryptography is not installed!')


class BaseCryptor:
    def __init__(self, config):
        self.config = config

    def encrypt(self, data):
        raise NotImplemented

    def decrypt(self, data):
        raise NotImplemented


class Cryptor(BaseCryptor):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        kdf = PBKDF2HMAC(
            algorithm=hashes.SHA256(),
            length=32,
            salt=self.config.salt,
            iterations=100000,
            backend=default_backend()
        )
        self.f = Fernet(base64.urlsafe_b64encode(kdf.derive(self.config.secret_key)))

    def encrypt(self, data):
        return self.f.encrypt(data)

    def decrypt(self, data):
        return self.f.decrypt(data)


class DummyCryptor(BaseCryptor):
    def encrypt(self, data):
        return data

    def decrypt(self, data):
        return data


default_cryptor = Cryptor if crypto_enabled else DummyCryptor
