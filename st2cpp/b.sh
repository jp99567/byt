
export QT5_VERSION=`ls -1 $HOME/Qt/|grep -P '^5.12.\d+$'|head -n1`
echo $QT5_VERSION
cmake ../st2cpp/ -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/home/j/Qt/$QT5_VERSION/gcc_64/
make
