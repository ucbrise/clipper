from __future__ import print_function
from subprocess import check_call
from subprocess import check_output

succeed = "-"*40

# Executes a Linux command.
# If `check` is True, the program will exit if it fails.
# Otherwise, an exception will be thrown if it fails.
def run(cmd, check=True):
	print("Execute:", cmd)
	if check:
		try:
			check_call(cmd, shell=True)
		except Exception as e:
			print(e)
			exit(1)
	else:
		return check_output(cmd, shell=True)

# Install common tools.
def install_common():
	tools = ['make', 'git', 'cmake', 'gcc', 'g++', 'libev-dev', \
	'redis-server', 'wget', 'unzip', 'libboost-all-dev']
	run("sudo add-apt-repository -y ppa:george-edison55/cmake-3.x")
	run("sudo add-apt-repository -y ppa:chris-lea/redis-server")
	run("sudo apt-get update")
	run("sudo apt-get install -y " + " ".join(str(i) for i in tools))
	print('Common tools installed')

# Returns True if the compilation check succeeds, i.e. the tool is installed.
# Returns False otherwise.
def compile_check(header, content, version):
	run("""printf '#include %s\n#include <iostream>\n
		int main() {std::cout << %s;}\n' > temp.cpp""" % (header, content))
	try:
		run("g++ -I/usr/local/include temp.cpp", False)
		if version not in run("./a.out", False):
			return False
	except Exception:
		return False
	finally:
		run("rm -f temp.cpp a.out")
	return True

# Install Hiredis.
def install_Hiredis():
	if compile_check("<hiredis/hiredis.h>", "\"\"", ""):
		print(succeed, "Hiredis already installed")
		return
	run("git clone https://github.com/redis/hiredis.git")
	run("cd hiredis/ && make && sudo make install")
	print(succeed, "Hiredis installed")

# Install Boost.
def install_Boost():
	version = "1_6"
	version_full = "1_63_0"
	url = "https://sourceforge.net/projects/boost/files/boost/1.63.0/boost_1_63_0.zip/download"
	if compile_check("<boost/version.hpp>", "BOOST_LIB_VERSION", version):
		print(succeed, "Boost %s already installed" % version)
		return
	run("wget -O boost_%s.zip %s" % (version_full, url))
	run("unzip boost_%s.zip -d boost" % version_full)
	run("cd boost/boost_%s/ && ./bootstrap.sh && ./b2 && sudo ./bjam install" % \
		version_full)
	print(succeed, 'Boost %s installed' % version_full)

# Install ZeroMQ.
def install_ZeroMQ():
	version = "4.1"
	version_full = "4.1.6"
	url = "https://github.com/zeromq/zeromq4-1/releases/download/v4.1.6/zeromq-4.1.6.zip"
	if compile_check("<zmq.h>", \
		"ZMQ_VERSION_MAJOR << \".\" << ZMQ_VERSION_MINOR", version):
		print(succeed, "ZeroMQ %s already installed" % version_full)
		return
	run("wget %s" % url)
	run("unzip zeromq-%s.zip -d zeromq" % version_full)
	run("cd zeromq/zeromq-%s && ./configure && make && sudo make install" % \
		version_full)
	print(succeed, "ZeroMQ %s installed" % version_full)

if __name__ == '__main__':
	install_common()
	install_Hiredis()
	install_Boost()
	install_ZeroMQ()
