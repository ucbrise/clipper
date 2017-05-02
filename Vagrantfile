# -*- mode: ruby -*-
# vi: set ft=ruby :

Vagrant.configure("2") do |config|
  config.vm.box = "debian/testing64"

  config.vm.provider "virtualbox" do |v|
    v.name = "clipper"
  end

  config.vm.provider "virtualbox" do |v|
    v.memory = 2048
    v.cpus = 2
  end

  config.vm.network "forwarded_port", guest: 8888, host: 8888

  config.vm.synced_folder ".", "/vagrant", type: "virtualbox"

  # Enable provisioning with a shell script. Additional provisioners such as
  # Puppet, Chef, Ansible, Salt, and Docker are also available. Please see the
  # documentation for more information about their specific syntax and use.
  config.vm.provision "shell", privileged: false, inline: <<-SHELL
    sudo apt-get update
    sudo apt-get upgrade -y
    sudo apt-get install -y cmake redis-server libhiredis-dev libev-dev libboost-all-dev libzmq3-dev g++ git openjdk-8-jdk maven python-zmq python-numpy libzmq-jni
    echo 'export JZMQ_HOME=/usr/lib/x86_64-linux-gnu/jni' >> ~/.bashrc

    # Allow running the tutorials notebooks inside the virtual machine
    sudo apt-get install -y python3-pip libffi-dev libssl-dev
    cd /vagrant/examples/tutorial
    sudo pip3 install -r requirements.txt

    # Installing docker
    sudo apt-get install -y apt-transport-https ca-certificates curl gnupg2 software-properties-common
    curl -fsSL https://download.docker.com/linux/debian/gpg | sudo apt-key add -
    sudo add-apt-repository \
       "deb [arch=amd64] https://download.docker.com/linux/debian \
          $(lsb_release -cs) \
             stable"
    sudo apt-get update
    sudo apt-get install -y docker-ce docker-compose
    sudo groupadd docker
    sudo usermod -aG docker vagrant

    cd /vagrant
    ./configure
    cd debug/
    make

    ../bin/run_unittests.sh

  SHELL
end
