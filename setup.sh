# install mamba
curl -L -O "https://github.com/conda-forge/miniforge/releases/latest/download/Miniforge3-$(uname)-$(uname -m).sh"
bash Miniforge3-$(uname)-$(uname -m).sh

# install packages

source ~/.bashrc
mamba create -n batchlut gcc gxx cmake make ninja fmt mpfr openmp openssl -c conda-forge
mamba activate batchlut

# get repo

export PAT=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
git clone https://$PAT@github.com/timsu1104/BatchLUT.git
cd BatchLUT
git submodule update --init --recursive --progress
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=true -DLUT_INPUT_SIZE=20 -DLUT_OUTPUT_SIZE=128\
 -DOPENSSL_INCLUDE_DIR=$HOME/miniforge3/envs/batchlut/include/openssl\
 -DOPENSSL_CRYPTO_LIBRARY=$HOME/miniforge3/envs/batchlut/lib/libcrypto.so\
 -DOPENSSL_SSL_LIBRARY=$HOME/miniforge3/envs/batchlut/lib/libssl.so
cmake --build ./build --target batchlut --parallel

echo mamba activate batchlut >> ~/.bashrc
echo cd BatchLUT >> ~/.bashrc

# reenter
mamba activate batchlut
cd BatchLUT

# ssh
ssh -i ~/.ssh/batchlut-key.pem ubuntu@ec2-18-217-139-236.us-east-2.compute.amazonaws.com
ssh -i ~/.ssh/batchlut-key.pem ubuntu@ec2-13-58-200-114.us-east-2.compute.amazonaws.com