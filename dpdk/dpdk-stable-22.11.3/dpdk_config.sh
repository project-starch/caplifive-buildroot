modprobe vfio
modprobe vfio-pci
echo 1 > /sys/module/vfio/parameters/enable_unsafe_noiommu_mode
echo 2048 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
python3 /home/ubu/dpdk-stable-22.11.3/usertools/dpdk-devbind.py --bind=vfio-pci enp0s2
python3 /home/ubu/dpdk-stable-22.11.3/usertools/dpdk-devbind.py --bind=vfio-pci enp0s3