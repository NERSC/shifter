yum -y install cmake openscap-utils openscap-python python-lxml git
git clone https://github.com/OpenSCAP/scap-security-guide.git
cd scap-security-guide/build
cmake ..
make -j4
make install
