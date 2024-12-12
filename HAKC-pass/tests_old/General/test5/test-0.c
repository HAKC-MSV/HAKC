//
// Created by de29664 on 3/28/24.
//

struct kvm_io_range {
    void *addr;
    int len;
};

struct kvm_io_bus {
    int dev_count;
    int ioeventfd_count;
    struct kvm_io_range range[];
};

int kvm_io_bus_cmp(struct kvm_io_range*, struct kvm_io_range*);
struct kvm_io_range* bsearch(struct kvm_io_range*, struct kvm_io_range*, int, int);

int kvm_io_bus_get_first_dev(struct kvm_io_bus *bus,
                                    void* addr, int len)
{
    struct kvm_io_range *range, key;
    int off;

    key = (struct kvm_io_range) {
            .addr = addr,
            .len = len,
    };

    range = bsearch(&key, bus->range, bus->dev_count,
                    sizeof(struct kvm_io_range));
    if (range == (void*)0)
        return -1;

    off = range - bus->range;

    return off;
}
