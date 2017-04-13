#pragma once
#include <string>
#include <linux/binder.h>

enum {
    PING_TRANSACTION  = B_PACK_CHARS('_','P','N','G'),
    SVC_MGR_GET_SERVICE = 1,
    SVC_MGR_CHECK_SERVICE,
    SVC_MGR_ADD_SERVICE,
    SVC_MGR_LIST_SERVICES,
};

struct binder_transaction_buffer{
    uint32_t cmd;
    struct binder_transaction_data txn;
} __attribute__((packed));

binder_transaction_data * transact(int binder_dev_fd, struct binder_write_read& bwr);
void binder_acquire(int binder_dev_fd, uint32_t handle);
void free_buffer(int binder_dev_fd, uintptr_t txn_data_buffer);

void pexit(const char * format, ...);

std::string binder_driver_command_str(binder_driver_command_protocol value);
std::string binder_driver_reply_str(binder_driver_return_protocol value);
void binder_dump_txn(struct binder_transaction_data *txn);

