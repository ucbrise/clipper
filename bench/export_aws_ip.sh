#!/usr/bin/env bash

AWS_GET_IP_ADDR="http://169.254.169.254/latest/meta-data/local-ipv4"

# Gets the current AWS instance's IP, if it exists.
# This will only time out if not called from an AWS instance
aws_ip=$(curl $AWS_GET_IP_ADDR)
echo "No clipper_ip supplied. Assuming that this script is being run on an AWS instance. Will set CLIPPER_IP to the current host's IP: $aws_ip"
export IP=$aws_ip
