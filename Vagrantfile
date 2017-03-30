# -*- mode: ruby -*-
# vi: set ft=ruby :

Vagrant.configure("2") do |config|
  config.vm.box = "debian/testing64"

  # Enable provisioning with a shell script. Additional provisioners such as
  # Puppet, Chef, Ansible, Salt, and Docker are also available. Please see the
  # documentation for more information about their specific syntax and use.
  config.vm.provision "shell", inline: <<-SHELL
    apt-get update
    apt-get upgrade
    apt-get install -y cmake redis-server libhiredis-dev libev-dev libboost-all-dev libzmq3-dev g++ git

    cd /vagrant
    ./configure
    cd debug/
    make

    ../bin/run_unittests.sh

  SHELL
end
