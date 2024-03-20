#include <fcntl.h>
#include <android/linux/binder.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "utils.h"

int main(int, char **) {
    const char * binder_dev_file = "/dev/binder";
    int binder_dev_fd = open(binder_dev_file, O_RDWR | O_CLOEXEC);
    if (binder_dev_fd < 0)
        pexit("error while open %s", binder_dev_file);

    printf("binder_dev_file = %s binder_dev_fd = %d\n", binder_dev_file, binder_dev_fd);

    struct binder_version vers;
    if (ioctl(binder_dev_fd, BINDER_VERSION, &vers) == -1)
        pexit("error while ioctl(BINDER_VERSION)");

    printf("binder: kernel driver version: '%d' user space version: '%d'\n", vers.protocol_version, BINDER_CURRENT_PROTOCOL_VERSION);
    if(vers.protocol_version != BINDER_CURRENT_PROTOCOL_VERSION) {
        printf("kernel and user space binder versions should match. exit(1).\n");
        exit(1);
    }

    const int binder_map_size = 128 * 1024; // from frameworks/native/cmds/servicemanager/servicemanager.c: main()
    void * binder_mapped_mem = mmap(NULL, binder_map_size, PROT_READ, MAP_PRIVATE, binder_dev_fd, 0);
    if (binder_mapped_mem == MAP_FAILED)
        pexit("error while mmap(binder_fd) binder_map_size = %d\n", binder_map_size);

    printf("enter to ping service manager\n");
    char buf[32];
    gets(buf);

    // only one CONTEXT_MGR is allowed
//   if(ioctl(binder_dev_fd, BINDER_SET_CONTEXT_MGR, 0) == -1)
//      pexit("error while ioctl(BINDER_SET_CONTEXT_MGR)");

//     uint32_t enter_looper_cmd = BC_ENTER_LOOPER;
// 
//     struct binder_write_read enter_looper_bwr = {};
//     enter_looper_bwr.write_buffer = (binder_uintptr_t) &enter_looper_cmd;
//     enter_looper_bwr.write_size = sizeof(enter_looper_cmd);
// 
//     if(ioctl(binder_dev_fd, BINDER_WRITE_READ, &enter_looper_cmd) < 0)
//         pexit("error while ioctl(BINDER_WRITE_READ, BC_ENTER_LOOPER)");

// 1. ping service manager

    struct binder_transaction_buffer writebuf = {};
    struct binder_write_read bwr = {};
    uint32_t readbuf[256];
    
    writebuf.cmd = BC_TRANSACTION;
    writebuf.txn.target.handle = 0; // 0 for CONTEXT_MGR
    writebuf.txn.code = PING_TRANSACTION;
    writebuf.txn.flags = 0;
    writebuf.txn.data_size = 0;
    writebuf.txn.offsets_size = 0;
    writebuf.txn.data.ptr.buffer = 0;
    writebuf.txn.data.ptr.offsets = 0;

    bwr.write_size = sizeof(writebuf);
    bwr.write_buffer = (uintptr_t) &writebuf;
    bwr.write_consumed = 0;

    bwr.read_size = sizeof(readbuf);
    bwr.read_buffer = (uintptr_t)readbuf;
    bwr.read_consumed = 0;

    printf("send PING_TRANSACTION txn:\n");
    binder_dump_txn(&writebuf.txn);

    binder_transaction_data * txn = transact(binder_dev_fd, bwr);
    printf("reply for PING_TRANSACTION:\n");
    binder_dump_txn(txn);

    printf("free buffer\n");
    free_buffer(binder_dev_fd, (uintptr_t) txn->data.ptr.buffer);

    printf("\n");

    printf("enter to list service #3\n");
    gets(buf);

// 2. get name of binder service #3:

    uint16_t svcmgr_id[] = {
        'a', 'n', 'd', 'r', 'o', 'i', 'd', '.', 'o', 's', '.',
        'I', 'S', 'e', 'r', 'v', 'i', 'c', 'e', 'M', 'a', 'n', 'a', 'g', 'e', 'r',
    };

    struct {
        uint32_t strict_mode_header;

        // android string, len in chars, array of 16 bits chars and final 32 bits zero
        uint32_t svcmgr_id_half_len;
        uint16_t svcmgr_id[sizeof(svcmgr_id) / 2];
        uint32_t svcmgr_id_zero;

        uint32_t service_n;
    } __attribute__((packed)) svc_mgr_list_cmd_data;

    svc_mgr_list_cmd_data.strict_mode_header = 0;
    svc_mgr_list_cmd_data.svcmgr_id_half_len = sizeof(svcmgr_id) / 2;
    memcpy(svc_mgr_list_cmd_data.svcmgr_id, svcmgr_id, sizeof(svcmgr_id));
    svc_mgr_list_cmd_data.svcmgr_id_zero = 0;
    svc_mgr_list_cmd_data.service_n = 3;

    writebuf.cmd = BC_TRANSACTION;
    writebuf.txn.target.handle = 0; // 0 for CONTEXT_MGR
    writebuf.txn.code = SVC_MGR_LIST_SERVICES;
    writebuf.txn.flags = 0;
    writebuf.txn.data_size = sizeof(svc_mgr_list_cmd_data);
    writebuf.txn.offsets_size = 0;
    writebuf.txn.data.ptr.buffer = (uintptr_t)&svc_mgr_list_cmd_data;
    writebuf.txn.data.ptr.offsets = 0;

    bwr.write_size = sizeof(writebuf);
    bwr.write_buffer = (uintptr_t) &writebuf;
    bwr.write_consumed = 0;
        
    bwr.read_size = sizeof(readbuf);
    bwr.read_consumed = 0;
    bwr.read_buffer = (uintptr_t)readbuf;

    printf("send SVC_MGR_LIST_SERVICES txn:\n");
    binder_dump_txn(&writebuf.txn);

    txn = transact(binder_dev_fd, bwr);
    printf("reply for SVC_MGR_LIST_SERVICES:\n");
    binder_dump_txn(txn);

    printf("free buffer\n");
    free_buffer(binder_dev_fd, (uintptr_t) txn->data.ptr.buffer);

    printf("\n");

// 3. get handle of service "TestSimpleServer"
// source: frameworks/native/cmds/servicemanager/bctest.c

    uint16_t lookup_id[] = {
        'T', 'e', 's', 't', 'S', 'i', 'm', 'p', 'l', 'e', 'S', 'e', 'r', 'v', 'e', 'r'
    };

    struct {
        uint32_t strict_mode_header;

        // android string, len in chars, array of 16 bits chars and final 32 bits zero
        uint32_t svcmgr_id_half_len;
        uint16_t svcmgr_id[sizeof(svcmgr_id) / 2];
        uint32_t svcmgr_id_zero;

        uint32_t lookup_id_half_len;
        uint16_t lookup_id[sizeof(lookup_id) / 2];
        uint32_t lookup_id_zero;

    } __attribute__((packed)) svc_mgr_lookup_cmd_data;

    svc_mgr_lookup_cmd_data.strict_mode_header = 0;
    svc_mgr_lookup_cmd_data.svcmgr_id_half_len = sizeof(svcmgr_id) / 2;
    memcpy(svc_mgr_lookup_cmd_data.svcmgr_id, svcmgr_id, sizeof(svcmgr_id));
    svc_mgr_lookup_cmd_data.svcmgr_id_zero = 0;
    svc_mgr_lookup_cmd_data.lookup_id_half_len = sizeof(lookup_id) / 2;
    memcpy(svc_mgr_lookup_cmd_data.lookup_id, lookup_id, sizeof(lookup_id));
    svc_mgr_lookup_cmd_data.lookup_id_zero = 0;

    writebuf.cmd = BC_TRANSACTION;
    writebuf.txn.target.handle = 0; // 0 for CONTEXT_MGR
    writebuf.txn.code = SVC_MGR_GET_SERVICE;
    writebuf.txn.flags = 0;
    writebuf.txn.data_size = sizeof(svc_mgr_lookup_cmd_data);
    writebuf.txn.offsets_size = 0;
    writebuf.txn.data.ptr.buffer = (uintptr_t)&svc_mgr_lookup_cmd_data;
    writebuf.txn.data.ptr.offsets = 0;

    bwr.write_size = sizeof(writebuf);
    bwr.write_buffer = (uintptr_t) &writebuf;
    bwr.write_consumed = 0;

    bwr.read_size = sizeof(readbuf);
    bwr.read_buffer = (uintptr_t)readbuf;
    bwr.read_consumed = 0;

    printf("send SVC_MGR_GET_SERVICE txn:\n");
    binder_dump_txn(&writebuf.txn);

    txn = transact(binder_dev_fd, bwr);
    printf("reply for SVC_MGR_GET_SERVICE:\n");
    binder_dump_txn(txn);

    binder_size_t * offs = (binder_size_t *)(uintptr_t)txn->data.ptr.offsets;
    struct flat_binder_object * test_service_flat_binder = (struct flat_binder_object *) (((char*)(uintptr_t)txn->data.ptr.buffer) + *offs);
    uint32_t test_service_handle = test_service_flat_binder->handle;
    printf("test_service_flat_binder handle = %u\n", test_service_handle);
    binder_acquire(binder_dev_fd, test_service_handle);

    printf("free buffer\n");
    free_buffer(binder_dev_fd, (uintptr_t) txn->data.ptr.buffer);

    printf("enter to send TEST_SERVICE_COMMAND\n");
    gets(buf);

// 4. call TestSimpleServer::TEST_SERVICE_COMMAND
    writebuf.cmd = BC_TRANSACTION;
    writebuf.txn.target.handle = test_service_handle;
    writebuf.txn.code = 1; // TEST_SERVICE_COMMAND
    writebuf.txn.flags = TF_ACCEPT_FDS;
    uint32_t param = 0x66;
    writebuf.txn.data_size = sizeof(param);
    writebuf.txn.offsets_size = 0;
    writebuf.txn.data.ptr.buffer = (uintptr_t)&param;
    writebuf.txn.data.ptr.offsets = 0;

    bwr.write_size = sizeof(writebuf);
    bwr.write_buffer = (uintptr_t) &writebuf;
    bwr.write_consumed = 0;

    bwr.read_size = sizeof(readbuf);
    bwr.read_buffer = (uintptr_t)readbuf;
    bwr.read_consumed = 0;

    printf("send TEST_SERVICE_COMMAND txn:\n");
    binder_dump_txn(&writebuf.txn);

    txn = transact(binder_dev_fd, bwr);
    printf("reply for TEST_SERVICE_COMMAND:\n");
    binder_dump_txn(txn);
    
    printf("\n");

    gets(buf);
    return 0;
}


