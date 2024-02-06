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
# ./dpdk-chat_server -l 0 -n 4 -- -p 3 -n 2
# ./dpdk-chat_server -l 0 -n 4 -- -p 3 -n 1
# mount -t hugetlbfs nodev /mnt/huge && echo 2048 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages && insmod capstone.ko && ./dpdk-chat_server -l 0 -n 4 -- -p 3 -n 2
# send_to_domain 0 5912 214 9124124 94 2014 1294 1204 29
# send_to_domain 1 592 5012 59 10 27 249 2419 20
# receive_from_domain 0
# receive_from_domain 1
