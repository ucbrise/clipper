from __future__ import print_function, absolute_import
import os
import yaml
import sys

cert_auth_env_var = "CLIPPER_K8S_CERT_AUTH"
client_cert_env_var = "CLIPPER_K8S_CLIENT_CERT"
client_key_env_var = "CLIPPER_K8S_CLIENT_KEY"
pw_env_var = "CLIPPER_K8S_PASSWORD"
required_vars = [cert_auth_env_var, client_cert_env_var, client_key_env_var, pw_env_var]


def write_config(dest_path):

    for v in required_vars:
        if v not in os.environ:
            print("{} not defined. Cannot construct kube config".format(v))
            sys.exit(1)

    cert_auth_data = os.environ[cert_auth_env_var]
    client_cert_data = os.environ[client_cert_env_var]
    client_key_data = os.environ[client_key_env_var]
    pw_data = os.environ[pw_env_var]

    base_config = {
        "kind": "Config",
        "preferences": {},
        "current-context": "jenkins.clipper-k8s-testing.com",
        "contexts": [
            {
                "name": "jenkins.clipper-k8s-testing.com",
                "context": {
                    "cluster": "jenkins.clipper-k8s-testing.com",
                    "user": "jenkins.clipper-k8s-testing.com",
                },
            }
        ],
        "clusters": [
            {
                "cluster": {
                    "certificate-authority-data": cert_auth_data,
                    "server": "https://api.jenkins.clipper-k8s-testing.com",
                },
                "name": "jenkins.clipper-k8s-testing.com",
            }
        ],
        "apiVersion": "v1",
        "users": [
            {
                "name": "jenkins.clipper-k8s-testing.com",
                "user": {
                    "username": "admin",
                    "password": pw_data,
                    "client-key-data": client_key_data,
                    "client-certificate-data": client_cert_data,
                },
            },
            {
                "name": "jenkins.clipper-k8s-testing.com-basic-auth",
                "user": {"username": "admin", "password": pw_data},
            },
        ],
    }
    with open(dest_path, "w") as f:
        yaml.dump(base_config, f)


if __name__ == "__main__":
    config_path = sys.argv[1]
    config_path = os.path.abspath(os.path.expanduser(config_path))
    print("Writing kube config to {}".format(config_path))
    write_config(config_path)
