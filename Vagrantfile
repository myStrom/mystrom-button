# -*- mode: ruby -*-
# vi: set ft=ruby :

ENV["LC_ALL"] = "en_US.UTF-8"

Vagrant.configure(2) do |config|
	config.vm.box = "ubuntu/trusty32"
	config.vm.provision :shell, path: "bootstrap.sh"
	config.vm.network :forwarded_port, guest: 22, host: 12433

	config.ssh.forward_agent = true

	config.vm.provider "virtualbox" do |vb|
		vb.gui = false
		vb.memory = 768
		vb.customize ["modifyvm", :id, "--cpuexecutioncap", "100"]
		vb.customize ["modifyvm", :id, "--ioapic", "on"]
		vb.customize ["modifyvm", :id, "--natdnshostresolver1", "on"]
		vb.cpus = 1
	end
end
