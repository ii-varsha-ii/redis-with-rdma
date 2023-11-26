#include <infiniband/verbs.h>
#include <iostream>
#include <string>
using namespace std;

int main() {
    struct ibv_device **device_list;
    struct ibv_pd* protection_domain;
    int num_devices;
    int i;
    struct ibv_context* context = nullptr;
    string device_name = "t_siw";
    device_list = ibv_get_device_list(&num_devices);
    if (!device_list) {
        cerr << "Error, ibv_get_device_list() failed" << endl;
        exit(1);
    }
    for (i = 0; i < num_devices; ++i) {
        cout << "RDMA device[" << i << "]: name=" << ibv_get_device_name(device_list[i]) << endl;
        if (device_name.compare(ibv_get_device_name(device_list[i])) == 0) {
            context = ibv_open_device(device_list[i]);

            protection_domain = ibv_alloc_pd(context);

            break;
        }
    }
    ibv_free_device_list(device_list);
    if (context == nullptr) {
        cerr  << "Unable to find the device \n" << device_name;
    }
    // return context;
}