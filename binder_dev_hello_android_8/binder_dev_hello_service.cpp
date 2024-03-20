#include <fcntl.h>
#include <linux/binder.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "utils.h"

void binder_func(binder_transaction_data * data) {
    printf("i am binder func data = %p\n");
}

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


    
// publish service "TestSimpleServer" to service manager
// source: frameworks/native/cmds/servicemanager/bctest.c
    struct binder_transaction_buffer writebuf = {};
    struct binder_write_read bwr = {};
    uint32_t readbuf[256];

    uint16_t svcmgr_id[] = {
        'a', 'n', 'd', 'r', 'o', 'i', 'd', '.', 'o', 's', '.',
        'I', 'S', 'e', 'r', 'v', 'i', 'c', 'e', 'M', 'a', 'n', 'a', 'g', 'e', 'r',
    };

    uint16_t lookup_id[] = {
        'T', 'e', 's', 't', 'S', 'i', 'm', 'p', 'l', 'e', 'S', 'e', 'r', 'v', 'e', 'r'
    };

    struct {
        struct {
            uint32_t strict_mode_header;

            // android string, len in chars, array of 16 bits chars and final 32 bits zero
            uint32_t svcmgr_id_half_len;
            uint16_t svcmgr_id[sizeof(svcmgr_id) / 2];
            uint32_t svcmgr_id_zero;

            uint32_t lookup_id_half_len;
            uint16_t lookup_id[sizeof(lookup_id) / 2];
            uint32_t lookup_id_zero;
        } buffer;
        
        struct flat_binder_object binder_obj;
    } __attribute__((packed)) svc_mgr_publish_cmd;

    svc_mgr_publish_cmd.buffer.strict_mode_header = 0;
    svc_mgr_publish_cmd.buffer.svcmgr_id_half_len = sizeof(svcmgr_id) / 2;
    memcpy(svc_mgr_publish_cmd.buffer.svcmgr_id, svcmgr_id, sizeof(svcmgr_id));
    svc_mgr_publish_cmd.buffer.svcmgr_id_zero = 0;
    svc_mgr_publish_cmd.buffer.lookup_id_half_len = sizeof(lookup_id) / 2;
    memcpy(svc_mgr_publish_cmd.buffer.lookup_id, lookup_id, sizeof(lookup_id));
    svc_mgr_publish_cmd.buffer.lookup_id_zero = 0;

    svc_mgr_publish_cmd.binder_obj.type = BINDER_TYPE_BINDER;
    svc_mgr_publish_cmd.binder_obj.flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
    svc_mgr_publish_cmd.binder_obj.binder = (binder_uintptr_t)&binder_func;
    svc_mgr_publish_cmd.binder_obj.cookie = 0;

    writebuf.cmd = BC_TRANSACTION;
    writebuf.txn.target.handle = 0; // 0 for CONTEXT_MGR
    writebuf.txn.code = SVC_MGR_ADD_SERVICE;
    writebuf.txn.flags = TF_ACCEPT_FDS;
    writebuf.txn.data_size = sizeof(svc_mgr_publish_cmd);
    writebuf.txn.data.ptr.buffer = (uintptr_t)&svc_mgr_publish_cmd;

    binder_uintptr_t binder_offset = sizeof(svc_mgr_publish_cmd.buffer);
    writebuf.txn.offsets_size = sizeof(binder_offset);
    writebuf.txn.data.ptr.offsets = (uintptr_t)&binder_offset;

    bwr.write_size = sizeof(writebuf);
    bwr.write_buffer = (uintptr_t) &writebuf;
    bwr.write_consumed = 0;

    bwr.read_size = sizeof(readbuf);
    bwr.read_buffer = (uintptr_t)readbuf;
    bwr.read_consumed = 0;

    printf("send SVC_MGR_ADD_SERVICE binder_func = %p txn:\n", binder_func);
    binder_dump_txn(&writebuf.txn);

    printf("reply for SVC_MGR_ADD_SERVICE\n");
    binder_transaction_data * txn = transact(binder_dev_fd, bwr);

    char buf[32];
    gets(buf);

    printf("request received:\n");
    gets(buf);

    txn = transact(binder_dev_fd, bwr);
    

//     printf("free buffer\n");
//     free_buffer(binder_dev_fd, (uintptr_t) txn->data.ptr.buffer);

    gets(buf);
    return 0;
}


