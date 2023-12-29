if [ ! -d "/mnt/huge" ]; then
	mkdir /mnt/huge
fi
mount -t hugetlbfs nodev /mnt/huge
# modprobe vfio
# modprobe vfio-pci
# echo 1 > /sys/module/vfio/parameters/enable_unsafe_noiommu_mode
echo 2048 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
# python3 /etc/init.d/.usertools/dpdk-devbind.py --unbind 0000:00:02.0
# python3 /etc/init.d/.usertools/dpdk-devbind.py --unbind 0000:00:03.0
# python3 /etc/init.d/.usertools/dpdk-devbind.py --bind=vfio-pci eth1
# python3 /etc/init.d/.usertools/dpdk-devbind.py --bind=vfio-pci eth2
# -l 0 -n 4 -- -p 3 -n 2